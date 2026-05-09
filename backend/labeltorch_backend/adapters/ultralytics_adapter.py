"""
Ultralytics 训练适配器

封装 Ultralytics YOLO 训练/推理/导出 API
"""
import asyncio
import logging
from typing import Any
from .base import TrainingAdapter

logger = logging.getLogger(__name__)


class UltralyticsAdapter(TrainingAdapter):
    """Ultralytics YOLO 训练适配器"""

    # 支持的模型家族
    MODEL_FAMILIES = {
        "yolov5": {"task": "detect"},
        "yolov8": {"task": "detect"},
        "yolov8_obb": {"task": "obb"},
        "yolov10": {"task": "detect"},
        "yolov11": {"task": "detect"},
    }

    def __init__(self):
        self._model = None
        self._trainer = None
        self._status = "idle"
        self._metrics = {}

    async def validate_config(self, config: dict) -> dict:
        """验证训练配置"""
        errors = []

        model_family = config.get("model_family", "")
        if model_family not in self.MODEL_FAMILIES:
            errors.append(f"不支持的模型家族: {model_family}")

        if config.get("epochs", 0) <= 0:
            errors.append("epochs 必须 > 0")

        if config.get("batch", 0) <= 0:
            errors.append("batch 必须 > 0")

        return {"valid": len(errors) == 0, "errors": errors}

    async def prepare_dataset_snapshot(self, snapshot_path: str, data_yaml: str) -> dict:
        """准备训练数据"""
        return {"snapshot_path": snapshot_path, "data_yaml": data_yaml, "ready": True}

    async def start_training(self, config: dict) -> dict:
        """
        启动Ultralytics训练

        在独立线程中执行训练，避免阻塞事件循环
        """
        from ultralytics import YOLO

        model_family = config.get("model_family", "yolov8")
        model_variant = config.get("model_variant", "n")  # n/s/m/l/x
        data_yaml = config.get("data_yaml", "")
        epochs = config.get("epochs", 100)
        imgsz = config.get("imgsz", 640)
        batch = config.get("batch", 16)
        device = config.get("device", "cpu")
        patience = config.get("patience", 50)
        resume = config.get("resume", False)
        pretrained = config.get("pretrained", config.get("pretrained_weights", None))

        self._status = "running"

        try:
            # 构建模型名称
            if model_family == "yolov8_obb":
                model_name = f"yolov8{model_variant}-obb.pt"
                task = "obb"
            elif model_family == "yolov5":
                model_name = f"yolov5{model_variant}.pt"
                task = "detect"
            elif model_family == "yolov10":
                model_name = f"yolov10{model_variant}.pt"
                task = "detect"
            elif model_family == "yolov11":
                model_name = f"yolo11{model_variant}.pt"
                task = "detect"
            else:
                model_name = f"yolov8{model_variant}.pt"
                task = "detect"

            # 加载模型
            if pretrained and not resume:
                self._model = YOLO(pretrained)
            else:
                self._model = YOLO(model_name)

            # 在线程池中执行训练
            loop = asyncio.get_event_loop()
            results = await loop.run_in_executor(
                None,
                lambda: self._model.train(
                    data=data_yaml,
                    epochs=epochs,
                    imgsz=imgsz,
                    batch=batch,
                    device=device,
                    patience=patience,
                    resume=resume,
                    verbose=True,
                )
            )

            self._status = "succeeded"
            return {"status": "succeeded", "results": str(results)}

        except Exception as e:
            self._status = "failed"
            logger.error(f"Training failed: {e}")
            return {"status": "failed", "error": str(e)}

    async def stop_training(self) -> dict:
        """停止训练"""
        self._status = "stopping"
        # TODO: 实现训练停止逻辑
        return {"status": "stopping"}

    async def parse_logs(self, log_path: str) -> dict:
        """解析训练日志"""
        # TODO: 实现日志解析
        return {"logs": []}

    async def collect_metrics(self, run_dir: str) -> dict:
        """收集训练指标"""
        # TODO: 实现指标收集
        return {"metrics": self._metrics}

    async def export_model(self, weight_path: str, format: str, options: dict) -> dict:
        """导出模型"""
        from ultralytics import YOLO

        try:
            model = YOLO(weight_path)
            export_path = model.export(
                format=format,
                imgsz=options.get("imgsz", 640),
                opset=options.get("opset", 13),
                dynamic=options.get("dynamic", True),
                simplify=options.get("simplify", True),
            )
            return {"status": "succeeded", "export_path": str(export_path)}
        except Exception as e:
            return {"status": "failed", "error": str(e)}

    def get_status(self) -> dict:
        """获取当前训练状态"""
        return {"status": self._status, "metrics": self._metrics}
