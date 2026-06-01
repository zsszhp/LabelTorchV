"""
Anomalib 异常检测训练适配器

封装 Anomalib API，支持 PatchCore/PADIM/STFPM 等异常检测算法的训练/推理/导出
"""
import asyncio
import logging
import os
import threading
from typing import Callable, Optional
from .base import TrainingAdapter, StopTrainingException

logger = logging.getLogger(__name__)


class AnomalibAdapter(TrainingAdapter):
    """Anomalib 异常检测训练适配器"""

    # 支持的异常检测模型（key 使用 anomalib get_model 的标准名称）
    MODEL_FAMILIES = {
        "efficient_ad": {"category": "reconstruction"},
        "patchcore": {"category": "feature_extraction"},
        "padim": {"category": "feature_extraction"},
        "stfpm": {"category": "reconstruction"},
        "cflow": {"category": "normalizing_flow"},
        "dfkde": {"category": "feature_extraction"},
        "dfm": {"category": "feature_extraction"},
        "ganomaly": {"category": "reconstruction"},
        "fastflow": {"category": "normalizing_flow"},
        "reverse_distillation": {"category": "reconstruction"},
        "csflow": {"category": "normalizing_flow"},
        "devnet": {"category": "few_shot"},
    }

    def __init__(self):
        self._model = None
        self._engine = None
        self._status = "idle"
        self._metrics = {}
        self._stop_event = threading.Event()
        self._on_epoch_end: Optional[Callable] = None
        self._run_dir: Optional[str] = None

    async def validate_config(self, config: dict) -> dict:
        """验证异常检测训练配置"""
        errors = []

        model_family = config.get("model_family", "")
        if model_family not in self.MODEL_FAMILIES:
            errors.append(
                f"不支持的异常检测模型: {model_family}，"
                f"支持: {list(self.MODEL_FAMILIES.keys())}"
            )

        if config.get("epochs", 0) <= 0:
            errors.append("epochs 必须 > 0")

        if config.get("batch", 0) <= 0:
            errors.append("batch 必须 > 0")

        data_path = config.get("data_yaml", config.get("data_path", ""))
        if not data_path:
            errors.append("必须指定数据路径 (data_yaml 或 data_path)")

        return {"valid": len(errors) == 0, "errors": errors}

    async def prepare_dataset_snapshot(self, snapshot_path: str, data_yaml: str) -> dict:
        """准备异常检测训练数据"""
        return {"snapshot_path": snapshot_path, "data_yaml": data_yaml, "ready": True}

    async def start_training(self, config: dict) -> dict:
        """
        启动Anomalib异常检测训练

        Anomalib使用不同于目标检测的数据组织方式：
        - 正常样本放在 normal/ 目录
        - 测试时的异常样本放在 abnormal/ 目录（可选）
        """
        try:
            from anomalib.engine import Engine
            from anomalib.models import get_model
            from anomalib.data import Folder  # 引入 Folder 模块
        except ImportError:
            self._status = "failed"
            return {
                "status": "failed",
                "error": "anomalib 未安装，请运行: pip install anomalib",
            }

        model_family = config.get("model_family", "patchcore")
        data_path = config.get("data_yaml", config.get("data_path", ""))
        epochs = config.get("epochs", 10)
        imgsz = config.get("imgsz", config.get("img_size", 256))
        batch = config.get("batch", 32)
        device = config.get("device", "auto")
        project_dir = config.get("project_dir", "")
        run_name = config.get("run_name", "anomaly_train")
        seed = config.get("seed", 42)
        task = config.get("task", "classification")

        self._status = "running"
        self._stop_event.clear()
        self._metrics = {}

        try:
            # 创建模型
            model = get_model(model_family)

            # 解析数据目录并自适应寻找对应路径
            dataset_dir = os.path.dirname(data_path) if data_path.endswith(".yaml") else data_path
            
            normal_dir = "images/train"
            normal_test_dir = "images/val"
            abnormal_dir = None
            
            # 若含有 train/good 标准结构则覆盖
            if os.path.isdir(os.path.join(dataset_dir, "train", "good")):
                normal_dir = "train/good"
                if os.path.isdir(os.path.join(dataset_dir, "test", "good")):
                    normal_test_dir = "test/good"
                else:
                    normal_test_dir = None
                if os.path.isdir(os.path.join(dataset_dir, "test", "defective")):
                    abnormal_dir = "test/defective"

            datamodule = Folder(
                name="product",
                root=dataset_dir,
                normal_dir=normal_dir,
                abnormal_dir=abnormal_dir,
                normal_test_dir=normal_test_dir,
                image_size=(imgsz, imgsz),
                train_batch_size=batch,
                eval_batch_size=batch,
                num_workers=0,  # Windows 兼容性
            )

            # 创建Engine（anomalib 2.4.2 的 Engine 只接受 callbacks/logger/default_root_dir/**kwargs，
            # 其中 **kwargs 会透传给 Lightning Trainer，如 max_epochs/accelerator 等，
            # 但 image_size/batch_size/task 不是 Trainer 参数，不能传给 Engine）
            engine_kwargs = dict(
                max_epochs=epochs,
                default_root_dir=project_dir if project_dir else "results",
            )
            # 自适应显卡判断
            import torch
            if device and device != "auto":
                actual_acc = "gpu" if ("cuda" in device or device.isdigit()) and torch.cuda.is_available() else "cpu"
                engine_kwargs["accelerator"] = actual_acc
                if actual_acc == "gpu" and device.isdigit():
                    engine_kwargs["devices"] = [int(device)]
            else:
                engine_kwargs["accelerator"] = "gpu" if torch.cuda.is_available() else "cpu"

            engine = Engine(**engine_kwargs)
            self._engine = engine
            self._model = model

            # 在线程池中执行训练，传递 datamodule 对象
            loop = asyncio.get_event_loop()
            results = await loop.run_in_executor(
                None,
                self._train_sync,
                engine, model, datamodule,
            )

            # 收集指标
            self._collect_metrics_from_engine(engine)

            self._status = "succeeded"
            return {
                "status": "succeeded",
                "run_dir": self._run_dir or "",
                "results": str(results),
            }

        except StopTrainingException:
            self._status = "stopped"
            return {"status": "stopped", "run_dir": self._run_dir or ""}
        except Exception as e:
            self._status = "failed"
            logger.error(f"Anomaly detection training failed: {e}")
            return {"status": "failed", "error": str(e)}

    def _train_sync(self, engine, model, datamodule):
        """同步执行异常检测训练（在线程池中运行）"""
        if self._stop_event.is_set():
            raise StopTrainingException("Training stopped by user")

        # 注册 Lightning 回调以推送 epoch 级进度
        callbacks = []
        if self._on_epoch_end:
            try:
                import pytorch_lightning as pl

                class EpochCallback(pl.Callback):
                    def __init__(self, adapter):
                        super().__init__()
                        self._adapter = adapter
                        self._total_epochs = 0

                    def on_train_start(self, trainer, pl_module):
                        self._total_epochs = trainer.max_epochs

                    def on_train_epoch_end(self, trainer, pl_module):
                        if self._adapter._stop_event.is_set():
                            trainer.should_stop = True
                            return
                        epoch = trainer.current_epoch + 1
                        # 提取 loss
                        loss = 0.0
                        if trainer.callback_metrics:
                            for key in ["loss", "train_loss", "avg_loss"]:
                                if key in trainer.callback_metrics:
                                    val = trainer.callback_metrics[key]
                                    loss = float(val.item() if hasattr(val, "item") else val)
                                    break

                        # 提取验证指标
                        metrics = {}
                        for key, val in trainer.callback_metrics.items():
                            try:
                                v = float(val.item() if hasattr(val, "item") else val)
                                # 映射 Anomalib 指标名到标准名
                                if "image_AUROC" in key or "image_auroc" in key:
                                    metrics["auroc"] = v
                                elif "pixel_AUROC" in key or "pixel_auroc" in key:
                                    metrics["pixel_auroc"] = v
                                elif "image_F1Score" in key or "image_f1score" in key:
                                    metrics["f1"] = v
                            except (ValueError, TypeError):
                                pass

                        if self._adapter._on_epoch_end:
                            self._adapter._on_epoch_end({
                                "epoch": epoch,
                                "total_epochs": self._total_epochs,
                                "loss": loss,
                                **metrics,
                            })

                callbacks.append(EpochCallback(self))
            except ImportError:
                logger.warning("pytorch_lightning not available, epoch callbacks disabled")

        # 执行训练
        if callbacks:
            engine.fit(model=model, datamodule=datamodule, callbacks=callbacks)
        else:
            results = engine.fit(model=model, datamodule=datamodule)

        # 获取训练结果目录
        try:
            if hasattr(engine, 'trainer') and hasattr(engine.trainer, 'log_dir'):
                self._run_dir = str(engine.trainer.log_dir)
            elif hasattr(engine, 'trainer') and hasattr(engine.trainer, 'default_root_dir'):
                self._run_dir = str(engine.trainer.default_root_dir)
        except Exception:
            pass

        return {"status": "succeeded"}

    async def stop_training(self) -> dict:
        """停止异常检测训练"""
        self._status = "stopping"
        self._stop_event.set()

        # 尝试通过Engine停止训练
        if self._engine and hasattr(self._engine, 'trainer'):
            try:
                self._engine.trainer.should_stop = True
            except Exception:
                pass

        return {"status": "stopping"}

    async def parse_logs(self, log_path: str) -> dict:
        """解析异常检测训练日志"""
        if not log_path or not os.path.isdir(log_path):
            return {"logs": []}

        # Anomalib 使用 Lightning 的 CSV logger
        csv_path = None
        for root, dirs, files in os.walk(log_path):
            for f in files:
                if f == "metrics.csv":
                    csv_path = os.path.join(root, f)
                    break
            if csv_path:
                break

        if not csv_path or not os.path.isfile(csv_path):
            return {"logs": []}

        try:
            import csv
            logs = []
            with open(csv_path, "r", encoding="utf-8") as f:
                reader = csv.DictReader(f, skipinitialspace=True)
                for row in reader:
                    entry = {}
                    for key, value in row.items():
                        key = key.strip() if key else key
                        try:
                            entry[key] = float(value.strip()) if value and value.strip() else None
                        except (ValueError, AttributeError):
                            entry[key] = value.strip() if value else None
                    logs.append(entry)

            return {"logs": logs, "count": len(logs)}
        except Exception as e:
            logger.error(f"Failed to parse anomaly logs: {e}")
            return {"logs": [], "error": str(e)}

    async def collect_metrics(self, run_dir: str) -> dict:
        """收集异常检测训练指标"""
        metrics = dict(self._metrics)

        if run_dir and os.path.isdir(run_dir):
            # 查找指标文件
            for root, dirs, files in os.walk(run_dir):
                for f in files:
                    if f == "metrics.csv":
                        csv_path = os.path.join(root, f)
                        try:
                            import csv
                            last_row = None
                            with open(csv_path, "r", encoding="utf-8") as fh:
                                reader = csv.DictReader(fh, skipinitialspace=True)
                                for row in reader:
                                    last_row = row
                            if last_row:
                                for key, value in last_row.items():
                                    key = key.strip() if key else key
                                    try:
                                        metrics[key] = float(value.strip()) if value and value.strip() else None
                                    except (ValueError, AttributeError):
                                        pass
                        except Exception:
                            pass
                        break

        return {"metrics": metrics}

    def _collect_metrics_from_engine(self, engine):
        """从Anomalib Engine中提取训练指标"""
        try:
            if hasattr(engine, 'trainer') and hasattr(engine.trainer, 'callback_metrics'):
                cb_metrics = engine.trainer.callback_metrics
                metrics = {}
                for key, value in cb_metrics.items():
                    try:
                        if hasattr(value, 'item'):
                            metrics_val = value.item()
                        else:
                            metrics_val = float(value)
                        metrics[key] = metrics_val
                    except (ValueError, TypeError):
                        metrics[key] = str(value)
                self._metrics.update(metrics)

            # 提取关键异常检测指标
            key_mapping = {
                "image_AUROC": "image_AUROC",
                "pixel_AUROC": "pixel_AUROC",
                "image_F1Score": "image_F1Score",
                "pixel_F1Score": "pixel_F1Score",
            }
            for src_key, dst_key in key_mapping.items():
                for full_key in self._metrics:
                    if src_key in full_key:
                        self._metrics[dst_key] = self._metrics[full_key]
                        break

        except Exception as e:
            logger.warning(f"Failed to extract metrics from engine: {e}")

    async def export_model(self, weight_path: str, format: str, options: dict) -> dict:
        """导出异常检测模型"""
        try:
            import torch

            if format == "onnx":
                return await self._export_onnx(weight_path, options)
            elif format == "pt":
                return await self._export_pt(weight_path, options)
            else:
                return {"status": "failed", "error": f"异常检测模型不支持导出格式: {format}"}

        except ImportError:
            return {"status": "failed", "error": "torch 未安装"}
        except Exception as e:
            return {"status": "failed", "error": str(e)}

    async def _export_onnx(self, weight_path: str, options: dict) -> dict:
        """导出ONNX格式并写入自定义元数据"""
        import json

        try:
            imgsz = options.get("imgsz", 256)
            output_path = weight_path.rsplit(".", 1)[0] + ".onnx"

            # 优先使用 Anomalib Engine 导出（需要已训练的 model 和 engine 实例）
            exported_via_engine = False
            if self._engine and self._model:
                try:
                    from anomalib.deploy import ExportType
                    # anomalib 2.4.2: engine.export 需要传入 model 对象
                    export_path = self._engine.export(
                        model=self._model,
                        export_type=ExportType.ONNX,
                        input_size=(imgsz, imgsz),
                    )
                    if export_path and os.path.isfile(str(export_path)):
                        import shutil
                        shutil.copy2(str(export_path), output_path)
                        exported_via_engine = True
                except Exception as e:
                    logger.warning(f"Failed to export via Engine: {e}, falling back to torch.onnx.export")

            # 备用方案：使用 anomalib 的 load_from_checkpoint + Engine 导出
            if not exported_via_engine and weight_path.endswith(".ckpt"):
                try:
                    from anomalib.deploy import ExportType
                    from anomalib.models import get_model
                    import torch

                    model_family = options.get("model_family", "efficient_ad")
                    model = get_model(model_family)
                    checkpoint = torch.load(weight_path, map_location="cpu", weights_only=False)
                    if "state_dict" in checkpoint:
                        model.load_state_dict(checkpoint["state_dict"])
                    elif "model" in checkpoint:
                        state = checkpoint["model"].state_dict() if hasattr(checkpoint["model"], 'state_dict') else checkpoint["model"]
                        model.load_state_dict(state)

                    engine = Engine()
                    export_path = engine.export(
                        model=model,
                        export_type=ExportType.ONNX,
                        input_size=(imgsz, imgsz),
                    )
                    if export_path and os.path.isfile(str(export_path)):
                        import shutil
                        shutil.copy2(str(export_path), output_path)
                        exported_via_engine = True
                except Exception as e:
                    logger.warning(f"Failed to export via checkpoint loading: {e}")

            # 最终 fallback：原生 torch.onnx 导出
            if not exported_via_engine:
                import torch

                checkpoint = torch.load(weight_path, map_location="cpu", weights_only=False)
                if isinstance(checkpoint, dict) and "model" in checkpoint:
                    model = checkpoint["model"]
                elif isinstance(checkpoint, dict) and "state_dict" in checkpoint:
                    return {
                        "status": "failed",
                        "error": "仅包含state_dict，缺少模型结构，无法直接使用 torch.onnx 导出。请确保 Engine 实例存在或使用 .ckpt 文件。",
                    }
                else:
                    model = checkpoint

                model.eval()
                dummy_input = torch.randn(1, 3, imgsz, imgsz)

                torch.onnx.export(
                    model,
                    dummy_input,
                    output_path,
                    opset_version=options.get("opset", 13),
                    input_names=["input"],
                    output_names=["output"],
                    dynamic_axes={
                        "input": {0: "batch_size"},
                        "output": {0: "batch_size"},
                    } if options.get("dynamic", True) else None,
                )

            # --- ONNX 元数据属性写入 (Metadata embedding) ---
            import onnx
            onnx_model = onnx.load(output_path)

            # 提取合理的推荐阈值（多来源优先级：checkpoint > metrics 缓存 > 默认值）
            image_threshold = 0.5
            pixel_threshold = 0.5

            # 来源1：从训练指标缓存中提取阈值
            if hasattr(self, "_metrics"):
                for key in self._metrics:
                    if "image_threshold" in key:
                        image_threshold = float(self._metrics[key])
                    if "pixel_threshold" in key:
                        pixel_threshold = float(self._metrics[key])

            # 来源2：从 checkpoint 文件中提取 Anomalib 标准化阈值（优先级最高）
            if weight_path.endswith(".ckpt"):
                try:
                    import torch
                    ckpt = torch.load(weight_path, map_location="cpu", weights_only=False)
                    if isinstance(ckpt, dict):
                        # Anomalib 2.x 标准格式：ckpt["Normalization"]
                        if "Normalization" in ckpt:
                            norm_state = ckpt["Normalization"]
                            if "image_threshold" in norm_state:
                                t = norm_state["image_threshold"]
                                image_threshold = float(t.item() if hasattr(t, "item") else t)
                            if "pixel_threshold" in norm_state:
                                t = norm_state["pixel_threshold"]
                                pixel_threshold = float(t.item() if hasattr(t, "item") else t)
                        # 兼容旧格式：state_dict 中含 normalization 前缀
                        state_dict = ckpt.get("state_dict", ckpt)
                        if isinstance(state_dict, dict):
                            for key in state_dict:
                                if "normalization.image_threshold" in key:
                                    t = state_dict[key]
                                    image_threshold = float(t.item() if hasattr(t, "item") else t)
                                if "normalization.pixel_threshold" in key:
                                    t = state_dict[key]
                                    pixel_threshold = float(t.item() if hasattr(t, "item") else t)
                except Exception as e:
                    logger.warning(f"Failed to extract thresholds from checkpoint: {e}")

            # 来源3：从已训练模型的 Anomalib Normalization 组件直接获取
            if self._model and hasattr(self._model, "image_threshold"):
                try:
                    image_threshold = float(self._model.image_threshold.item() if hasattr(self._model.image_threshold, "item") else self._model.image_threshold)
                except Exception:
                    pass
            if self._model and hasattr(self._model, "pixel_threshold"):
                try:
                    pixel_threshold = float(self._model.pixel_threshold.item() if hasattr(self._model.pixel_threshold, "item") else self._model.pixel_threshold)
                except Exception:
                    pass

            # 算法名称从配置或默认值获取
            algorithm = options.get("model_family", "efficient_ad")

            metadata = {
                "image_threshold": image_threshold,
                "pixel_threshold": pixel_threshold,
                "input_size": [imgsz, imgsz],
                "normalization_mean": [0.485, 0.456, 0.406],
                "normalization_std": [0.229, 0.224, 0.225],
                "model_type": "anomaly_detection",
                "algorithm": algorithm,
            }

            # 清空旧元数据并写入新属性
            while len(onnx_model.metadata_props) > 0:
                onnx_model.metadata_props.pop()

            for k, v in metadata.items():
                entry = onnx.StringStringEntryProto(
                    key=str(k),
                    value=json.dumps(v) if isinstance(v, (list, dict)) else str(v)
                )
                onnx_model.metadata_props.append(entry)

            onnx.save(onnx_model, output_path)
            logger.info(f"Metadata embedded in ONNX: {output_path}, thresholds: image={image_threshold:.4f}, pixel={pixel_threshold:.4f}")

            return {"status": "succeeded", "export_path": output_path}

        except Exception as e:
            logger.error(f"Failed to export ONNX: {e}")
            return {"status": "failed", "error": str(e)}

    async def _export_pt(self, weight_path: str, options: dict) -> dict:
        """复制pt权重文件"""
        import shutil

        output_path = options.get("output_path", "")
        if output_path:
            shutil.copy2(weight_path, output_path)
            return {"status": "succeeded", "export_path": output_path}
        return {"status": "succeeded", "export_path": weight_path}

    def get_status(self) -> dict:
        """获取当前训练状态"""
        return {"status": self._status, "metrics": self._metrics}
