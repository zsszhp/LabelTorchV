"""
异常检测推理处理器

支持单张/批量推理，生成异常热力图
基于 anomalib 2.4.2 API 适配
"""
import asyncio
import logging
import os
import tempfile

import numpy as np

logger = logging.getLogger(__name__)


async def handle_infer(payload: dict) -> dict:
    """异常检测推理：单张/批量图片，返回异常分数和热力图"""
    try:
        from anomalib.engine import Engine
    except ImportError:
        return {"status": "failed", "error": "anomalib 未安装"}

    weight_path = payload.get("weight_path", "")
    image_paths = payload.get("image_paths", [])
    model_family = payload.get("model_family", "efficient_ad")
    device = payload.get("device", "auto")
    imgsz = payload.get("imgsz", 256)

    if not weight_path or not os.path.isfile(weight_path):
        return {"status": "failed", "error": f"模型权重文件不存在: {weight_path}"}

    if not image_paths:
        return {"status": "failed", "error": "未指定推理图片路径"}

    try:
        from anomalib.models import get_model

        # 加载模型
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

        # 执行推理
        loop = asyncio.get_event_loop()
        results = await loop.run_in_executor(
            None,
            _infer_sync,
            engine, model, weight_path, image_paths, imgsz,
        )

        return {
            "status": "succeeded",
            "predictions": results,
            "count": len(results),
        }

    except Exception as e:
        logger.error(f"Anomaly inference failed: {e}")
        return {"status": "failed", "error": str(e)}


def _infer_sync(engine, model, weight_path, image_paths, imgsz=256):
    """同步执行异常检测推理"""
    import torch

    # 加载权重
    checkpoint = torch.load(weight_path, map_location="cpu", weights_only=False)
    if "model" in checkpoint:
        model.load_state_dict(checkpoint["model"].state_dict()
                              if hasattr(checkpoint["model"], 'state_dict')
                              else checkpoint["model"])
    elif "state_dict" in checkpoint:
        model.load_state_dict(checkpoint["state_dict"])

    model.eval()

    results = []
    for img_path in image_paths:
        try:
            from anomalib.data import PredictDataset

            # 创建预测数据集（anomalib 2.4.2: PredictDataset 是 AnomalibDataModule 子类，
            # 可以直接传给 engine.predict 的 datamodule 参数）
            dataset = PredictDataset(
                root=os.path.dirname(img_path),
                image_size=(imgsz, imgsz),
            )

            # 执行推理（engine.predict 接受 datamodule，不是 dataloader）
            predictions = engine.predict(model=model, datamodule=dataset)

            # 提取结果
            pred_result = {
                "image_path": img_path,
                "anomaly_score": 0.0,
                "anomaly_map_path": "",
                "pred_label": "normal",
            }

            if predictions:
                for pred in predictions:
                    if hasattr(pred, 'pred_score'):
                        score = pred.pred_score
                        pred_result["anomaly_score"] = float(score) if hasattr(score, 'item') else float(score)
                    if hasattr(pred, 'pred_label'):
                        pred_result["pred_label"] = "anomalous" if pred.pred_label else "normal"
                    if hasattr(pred, 'anomaly_map'):
                        # 保存异常热力图
                        anomaly_map = pred.anomaly_map
                        if hasattr(anomaly_map, 'cpu'):
                            anomaly_map = anomaly_map.cpu().numpy()
                        if isinstance(anomaly_map, np.ndarray):
                            map_path = _save_anomaly_map(anomaly_map, img_path)
                            pred_result["anomaly_map_path"] = map_path
                    break

            results.append(pred_result)

        except Exception as e:
            logger.warning(f"Failed to infer image {img_path}: {e}")
            results.append({
                "image_path": img_path,
                "anomaly_score": 0.0,
                "error": str(e),
            })

    return results


def _save_anomaly_map(anomaly_map: np.ndarray, image_path: str) -> str:
    """保存异常热力图到文件"""
    try:
        import cv2

        # 归一化到0-255
        if anomaly_map.ndim > 2:
            anomaly_map = anomaly_map.squeeze()
        am_min = anomaly_map.min()
        am_max = anomaly_map.max()
        if am_max > am_min:
            anomaly_map = ((anomaly_map - am_min) / (am_max - am_min) * 255).astype(np.uint8)
        else:
            anomaly_map = np.zeros_like(anomaly_map, dtype=np.uint8)

        # 应用热力图颜色映射
        heatmap = cv2.applyColorMap(anomaly_map, cv2.COLORMAP_JET)

        # 保存
        base_name = os.path.splitext(os.path.basename(image_path))[0]
        output_dir = os.path.join(os.path.dirname(image_path), "anomaly_maps")
        os.makedirs(output_dir, exist_ok=True)
        output_path = os.path.join(output_dir, f"{base_name}_heatmap.png")
        cv2.imwrite(output_path, heatmap)

        return output_path

    except ImportError:
        # cv2未安装，使用PIL保存灰度图
        try:
            from PIL import Image

            if anomaly_map.ndim > 2:
                anomaly_map = anomaly_map.squeeze()
            am_min = anomaly_map.min()
            am_max = anomaly_map.max()
            if am_max > am_min:
                anomaly_map = ((anomaly_map - am_min) / (am_max - am_min) * 255).astype(np.uint8)
            else:
                anomaly_map = np.zeros_like(anomaly_map, dtype=np.uint8)

            base_name = os.path.splitext(os.path.basename(image_path))[0]
            output_dir = os.path.join(os.path.dirname(image_path), "anomaly_maps")
            os.makedirs(output_dir, exist_ok=True)
            output_path = os.path.join(output_dir, f"{base_name}_heatmap.png")
            Image.fromarray(anomaly_map).save(output_path)
            return output_path
        except Exception:
            return ""
    except Exception as e:
        logger.warning(f"Failed to save anomaly map: {e}")
        return ""
