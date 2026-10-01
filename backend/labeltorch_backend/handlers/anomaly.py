"""
异常检测推理处理器

支持单张/批量推理，生成异常热力图
基于 anomalib 2.4.2+ API 适配

批量语义：一次性对传入图片列表推理，结果与输入一一对应；
不再按图片 dirname 逐张建 PredictDataset 扫全目录（旧行为结果错且 O(N²)）。
"""
import asyncio
import logging
import os
from typing import List

logger = logging.getLogger(__name__)

# A9：Anomalib 支持的完整 12 个模型列表（与 anomalib_adapter.py 一致）
SUPPORTED_MODELS = [
    "patchcore",
    "padim",
    "stfpm",
    "cflow",
    "dfkde",
    "dfm",
    "ganomaly",
    "fastflow",
    "reverse_distillation",
    "csflow",
    "devnet",
    "efficient_ad",
]


async def handle_list_models(payload: dict) -> dict:
    """A9：返回后端支持的异常检测模型列表

    优先检测 anomalib 实际可用的模型，失败时返回完整列表。
    """
    try:
        # 尝试从 anomalib 获取实际支持的模型
        from anomalib.models import get_model  # noqa: F401

        # anomalib 已安装，返回完整支持列表
        return {
            "status": "succeeded",
            "models": SUPPORTED_MODELS,
            "count": len(SUPPORTED_MODELS),
        }
    except ImportError:
        # anomalib 未安装，返回空列表（前端会使用 fallback）
        logger.warning("anomalib not installed, returning empty model list")
        return {
            "status": "succeeded",
            "models": [],
            "count": 0,
        }
    except Exception as e:
        logger.error(f"Failed to list anomaly models: {e}")
        return {
            "status": "failed",
            "error": str(e),
            "models": [],
            "count": 0,
        }


async def handle_infer(payload: dict) -> dict:
    """异常检测推理：单张/批量图片，返回异常分数和热力图

    返回结构保证 image_path → score/label/heatmap 的明确映射：
    - predictions: 与入参 image_paths 同序的列表
    - by_image_path: 以图片路径为键的结果字典
    """
    try:
        from anomalib.engine import Engine
    except ImportError:
        return {
            "status": "failed",
            "error": {"code": "DEPENDENCY_MISSING", "message": "anomalib 未安装"},
        }

    from ..tools.path_security import (
        PathSecurityError,
        collect_allowed_roots,
        ensure_within,
    )

    weight_path = payload.get("weight_path", "")
    image_paths = payload.get("image_paths", [])
    model_family = payload.get("model_family", "efficient_ad")
    device = payload.get("device", "auto")
    imgsz = payload.get("imgsz", 256)

    # 单图调用方可能传单个字符串，统一为列表
    if isinstance(image_paths, str):
        image_paths = [image_paths]
    if not isinstance(image_paths, (list, tuple)):
        return {
            "status": "failed",
            "error": {"code": "INVALID_PARAMS", "message": "image_paths 必须为字符串列表"},
        }

    if not weight_path:
        return {
            "status": "failed",
            "error": {"code": "MISSING_PARAMS", "message": "缺少 weight_path"},
        }
    if not image_paths:
        return {
            "status": "failed",
            "error": {"code": "MISSING_PARAMS", "message": "未指定推理图片路径"},
        }

    # 路径白名单：权重与输入图片必须落在 projectRoot / snapshotDir / 系统临时目录内
    read_roots = collect_allowed_roots(payload, extra_roots=list(image_paths))
    write_roots = collect_allowed_roots(payload, include_data_dirs=False)

    checked_weight = ensure_within(weight_path, read_roots, field="weight_path", must_exist=True)
    if isinstance(checked_weight, dict):
        return checked_weight

    checked_images: List[str] = []
    for idx, img in enumerate(image_paths):
        err = ensure_within(img, read_roots, field=f"image_paths[{idx}]", must_exist=True)
        if isinstance(err, dict):
            return err
        checked_images.append(err)

    # 热力图输出目录：允许落在写白名单内；默认写到图片同级，越界则回退临时目录
    heatmap_dir = payload.get("heatmap_dir", "") or payload.get("anomaly_map_dir", "")
    if heatmap_dir:
        checked_heatmap_dir = ensure_within(heatmap_dir, write_roots, field="heatmap_dir", must_exist=False)
        if isinstance(checked_heatmap_dir, dict):
            return checked_heatmap_dir
    else:
        checked_heatmap_dir = ""

    try:
        from anomalib.models import get_model

        # 加载模型：
        # - .ckpt 优先按「落盘路径推导模型族 + hyper_parameters + state_dict」重建——
        #   anomalib ckpt 保存的是抽象基类 AnomalibModule，load_from_checkpoint 无法实例化；
        #   且 payload 未传 model_family 时默认值会错配（PatchCore ckpt 被建成 EfficientAd）
        # - torch.load 显式 weights_only=False：路径已白名单校验（项目内训练产物），
        #   PyTorch 2.6 起默认 weights_only=True 会拒绝 anomalib 全局对象
        model = None
        if str(checked_weight).endswith(".ckpt"):
            try:
                model = _rebuild_model_from_checkpoint(str(checked_weight))
                logger.info("Anomaly model rebuilt from checkpoint: %s", checked_weight)
            except Exception as e:
                logger.warning("checkpoint rebuild failed: %s, falling back to model_family option", e)
        if model is None:
            model = get_model(model_family)

        # 创建Engine用于推理（anomalib 2.4.2: Engine 只接受 callbacks/logger/default_root_dir/**kwargs，
        # **kwargs 透传给 Lightning Trainer，如 accelerator 等，
        # 但 image_size/task 不是 Trainer 参数，不能传给 Engine）
        engine_kwargs = dict()
        import torch
        if device and device != "auto":
            actual_acc = "gpu" if ("cuda" in device or device.isdigit()) and torch.cuda.is_available() else "cpu"
            engine_kwargs["accelerator"] = actual_acc
            if actual_acc == "gpu" and device.isdigit():
                engine_kwargs["devices"] = [int(device)]
        else:
            engine_kwargs["accelerator"] = "gpu" if torch.cuda.is_available() else "cpu"

        engine = Engine(**engine_kwargs)

        # 执行推理：一次性批量，结果与输入一一对应
        loop = asyncio.get_event_loop()
        results = await loop.run_in_executor(
            None,
            _infer_sync,
            engine, model, checked_weight, checked_images, imgsz, checked_heatmap_dir,
            read_roots, write_roots,
        )

        # by_image_path 明确映射，供前端按路径取 score/label/heatmap
        by_image_path = {
            item["image_path"]: {
                "score": item["anomaly_score"],
                "label": item["pred_label"],
                "heatmap": item["anomaly_map_path"],
            }
            for item in results
            if "image_path" in item
        }

        return {
            "status": "succeeded",
            "predictions": results,
            "by_image_path": by_image_path,
            "count": len(results),
        }

    except PathSecurityError as e:
        return {"status": "failed", "error": e.to_error_dict()}
    except Exception as e:
        logger.error(f"Anomaly inference failed: {e}")
        return {"status": "failed", "error": {"code": "INFER_FAILED", "message": str(e)}}


def _infer_sync(engine, model, weight_path, image_paths, imgsz=256, heatmap_dir="",
                read_roots=None, write_roots=None):
    """
    同步执行异常检测推理：一次 predict 覆盖全部传入图片。

    只对显式列表建数据集，不做目录扫描；结果按 image_path 对齐回输入顺序。
    """
    from ..tools.safe_loading import safe_load_weight

    # 加载权重：集中走 safe_load_weight（优先 weights_only=True，回退时校验路径并告警）
    # 权重按读白名单校验（可能位于外部数据目录），与 handle_infer 的前置校验一致
    checkpoint = safe_load_weight(weight_path, map_location="cpu", allowed_roots=read_roots)
    if isinstance(checkpoint, dict) and "model" in checkpoint:
        model.load_state_dict(checkpoint["model"].state_dict()
                              if hasattr(checkpoint["model"], 'state_dict')
                              else checkpoint["model"])
    elif isinstance(checkpoint, dict) and "state_dict" in checkpoint:
        model.load_state_dict(checkpoint["state_dict"])
    elif checkpoint is not None:
        # 直接就是 state_dict 或完整模型
        try:
            model.load_state_dict(checkpoint)
        except Exception:
            if hasattr(checkpoint, "eval"):
                model = checkpoint

    model.eval()

    # 精确图片列表数据集：跳过 PredictDataset 的目录扫描，保证 N 条输入 → N 条输出
    dataset = _ExactPredictDataset(image_paths, image_size=(imgsz, imgsz))

    # 一次性批量推理（engine.predict 接受 dataset，会自建 DataLoader）
    predictions = engine.predict(model=model, dataset=dataset)

    # 按 image_path 归并预测结果，再按输入顺序输出
    raw_by_key = {}
    for batch in predictions or []:
        # batch 为 ImageBatch：迭代得到逐图 ImageItem
        try:
            items = list(batch)
        except TypeError:
            items = [batch]
        for item in items:
            img_path = getattr(item, "image_path", None)
            if img_path is None:
                continue
            key = _path_key(str(img_path))
            raw_by_key[key] = item

    results = []
    for img_path in image_paths:
        pred_result = {
            "image_path": img_path,
            "anomaly_score": 0.0,
            "anomaly_map_path": "",
            "pred_label": "normal",
        }
        item = raw_by_key.get(_path_key(img_path))
        if item is None:
            # 模型侧未返回该图（异常跳过时），保留占位并在 error 中说明
            pred_result["error"] = "模型未返回该图片的推理结果"
            results.append(pred_result)
            continue

        try:
            score = getattr(item, "pred_score", None)
            if score is not None:
                pred_result["anomaly_score"] = float(score.item() if hasattr(score, "item") else score)

            label = getattr(item, "pred_label", None)
            if label is not None:
                # pred_label 为布尔或 0/1 张量
                is_anomalous = bool(label.item() if hasattr(label, "item") else label)
                pred_result["pred_label"] = "anomalous" if is_anomalous else "normal"

            anomaly_map = getattr(item, "anomaly_map", None)
            if anomaly_map is not None:
                import numpy as np
                if hasattr(anomaly_map, "cpu"):
                    anomaly_map = anomaly_map.cpu().numpy()
                if isinstance(anomaly_map, np.ndarray):
                    pred_result["anomaly_map_path"] = _save_anomaly_map(
                        anomaly_map, img_path, heatmap_dir, write_roots
                    )
        except Exception as e:
            logger.warning(f"Failed to parse prediction for {img_path}: {e}")
            pred_result["error"] = str(e)

        results.append(pred_result)

    return results


def _path_key(path: str) -> str:
    """路径归一化键：大小写与分隔符差异不影响匹配"""
    return os.path.normcase(os.path.normpath(str(path)))


class _ExactPredictDataset:
    """
    按显式路径列表构建的预测数据集。

    复用 anomalib PredictDataset 的样本协议（ImageItem + ImageBatch.collate），
    但不调用其目录扫描逻辑，保证输出顺序与输入路径一一对应。
    """

    def __init__(self, image_paths, image_size=(256, 256), transform=None):
        from pathlib import Path

        from anomalib.data import ImageBatch, ImageItem
        from anomalib.data.utils import read_image

        self.image_filenames = [Path(p) for p in image_paths]
        self.image_size = image_size
        self.transform = transform
        # 绑定 anomalib 的样本与批组装协议，engine.predict 才能自建 DataLoader
        self._image_item_cls = ImageItem
        self._read_image = staticmethod(read_image)
        self.collate_fn = ImageBatch.collate  # Engine.predict 依赖该属性

    def __len__(self) -> int:
        return len(self.image_filenames)

    def __getitem__(self, index: int):
        image_filename = self.image_filenames[index]
        image = self._read_image(image_filename, as_tensor=True)
        if self.transform is not None:
            image = self.transform(image)
        return self._image_item_cls(
            image=image,
            image_path=str(image_filename),
        )


def _save_anomaly_map(anomaly_map, image_path: str, heatmap_dir: str = "", write_roots=None) -> str:
    """保存异常热力图到文件

    输出位置优先级：
    1. 显式 heatmap_dir（已由调用方做写白名单校验）
    2. 图片同级 anomaly_maps/（当该目录落在写白名单内）
    3. 系统临时目录 labeltorch/anomaly_maps/（外部数据集图片的兜底，避免越界写盘）
    """
    import numpy as np

    output_dir = ""
    if heatmap_dir:
        output_dir = heatmap_dir
    else:
        candidate = os.path.join(os.path.dirname(image_path), "anomaly_maps")
        allowed = True
        if write_roots:
            try:
                from ..tools.path_security import require_path_within

                # 仅当候选目录位于写白名单内才写到图片旁
                require_path_within(candidate, write_roots, field="anomaly_map_dir")
            except Exception:
                allowed = False
        if allowed:
            output_dir = candidate
        else:
            import tempfile

            output_dir = os.path.join(tempfile.gettempdir(), "labeltorch", "anomaly_maps")

    try:
        os.makedirs(output_dir, exist_ok=True)
    except Exception as e:
        logger.warning(f"无法创建热力图目录 {output_dir}: {e}")
        return ""

    base_name = os.path.splitext(os.path.basename(image_path))[0]
    output_path = os.path.join(output_dir, f"{base_name}_heatmap.png")

    # 归一化到 0-255 伪彩色
    if anomaly_map.ndim > 2:
        anomaly_map = anomaly_map.squeeze()
    am_min = anomaly_map.min()
    am_max = anomaly_map.max()
    if am_max > am_min:
        anomaly_map = ((anomaly_map - am_min) / (am_max - am_min) * 255).astype(np.uint8)
    else:
        anomaly_map = np.zeros_like(anomaly_map, dtype=np.uint8)

    try:
        import cv2

        heatmap = cv2.applyColorMap(anomaly_map, cv2.COLORMAP_JET)
        cv2.imwrite(output_path, heatmap)
        return output_path
    except ImportError:
        # cv2未安装，使用PIL保存灰度图
        try:
            from PIL import Image

            Image.fromarray(anomaly_map).save(output_path)
            return output_path
        except Exception:
            return ""
    except Exception as e:
        logger.warning(f"Failed to save anomaly map: {e}")
        return ""


def _rebuild_model_from_checkpoint(weight_path: str):
    """从 anomalib ckpt 重建模型实例（推理/检测路径用）。

    anomalib 的 LightningModule.load_from_checkpoint 不可用：ckpt 保存的是抽象基类
    AnomalibModule（无法实例化）。因此按「落盘路径推导模型族 + hyper_parameters +
    state_dict」重建——与 anomalib_adapter._rebuild_model_from_checkpoint 同一方案。
    """
    import os
    import torch
    from anomalib.models import get_model

    parts = [p.lower().replace("_", "")
             for p in os.path.normpath(weight_path).replace("\\", "/").split("/")]
    known = ("patchcore", "padim", "efficientad", "stfpm", "fastflow", "dfkde", "dfm",
             "reversedistillation", "draem", "cflow", "csflow", "ganomaly", "supersimplenet", "winclip")
    alias = {"efficientad": "efficient_ad", "reversedistillation": "reverse_distillation"}
    family = None
    for p in parts:
        for name in known:
            if p == name.replace("_", ""):
                family = alias.get(name, name)
                break
        if family:
            break

    checkpoint = torch.load(weight_path, map_location="cpu", weights_only=False)
    state_dict = checkpoint.get("state_dict") if isinstance(checkpoint, dict) else None
    hp = checkpoint.get("hyper_parameters", {}) if isinstance(checkpoint, dict) else {}
    hparams = {k: hp[k] for k in ("backbone", "layers", "pre_trained",
                                  "coreset_sampling_ratio", "num_neighbors") if k in hp}

    try:
        model = get_model(family or "patchcore", **hparams)
    except Exception as e:
        logger.warning("get_model(%s, **hparams) failed: %s, trying plain get_model", family, e)
        model = get_model(family or "patchcore")
    if state_dict is not None:
        try:
            model.load_state_dict(state_dict)
        except Exception as e:
            logger.warning("strict load_state_dict failed: %s, retrying non-strict", e)
            model.load_state_dict(state_dict, strict=False)
    return model
