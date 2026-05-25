"""
Ultralytics 训练适配器

封装 Ultralytics YOLO 训练/推理/导出 API
"""
import asyncio
import json
import logging
import os
import threading
from pathlib import Path
from typing import Any, Callable, Optional
from .base import TrainingAdapter

logger = logging.getLogger(__name__)


class UltralyticsAdapter(TrainingAdapter):
    """Ultralytics YOLO 训练适配器"""

    # 支持的模型家族
    MODEL_FAMILIES = {
        "yolov5": {"task": "detect"},
        "yolov8": {"task": "detect"},
        "yolov8_obb": {"task": "obb"},
        "yolov8_cls": {"task": "classify"},
        "yolov10": {"task": "detect"},
        "yolov11": {"task": "detect"},
    }

    def __init__(self):
        self._model = None
        self._trainer = None
        self._status = "idle"
        self._metrics = {}
        self._stop_event = threading.Event()
        # 训练进度回调，由 _run_training 设置
        self._on_epoch_end: Optional[Callable] = None
        # 训练结果目录
        self._run_dir: Optional[str] = None

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

        在独立线程中执行训练，通过回调推送epoch级别进度
        """
        from ultralytics import YOLO

        model_family = config.get("model_family", "yolov8")
        model_variant = config.get("model_variant", "n")
        data_yaml = config.get("data_yaml", "")
        epochs = config.get("epochs", 100)
        imgsz = config.get("imgsz", config.get("img_size", 640))
        batch = config.get("batch", 16)
        device = config.get("device", "cpu")
        import torch
        if device and device != "cpu":
            if not torch.cuda.is_available():
                logger.warning(f"CUDA is not available. Falling back to CPU training (requested device was: {device}).")
                device = "cpu"
        patience = config.get("patience", 50)
        resume = config.get("resume", False)
        amp = config.get("amp", True)
        workers = config.get("workers", 8)
        pretrained = config.get("pretrained", config.get("pretrained_weights", None))
        project_dir = config.get("project_dir", "")
        run_name = config.get("run_name", "train")

        self._status = "running"
        self._stop_event.clear()
        self._metrics = {}

        try:
            # 构建模型名称
            if model_family == "yolov8_obb":
                model_name = f"yolov8{model_variant}-obb.pt"
                task = "obb"
            elif model_family == "yolov8_cls":
                model_name = f"yolov8{model_variant}-cls.pt"
                task = "classify"
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

            # 在线程池中执行训练，使用回调捕获epoch进度
            loop = asyncio.get_event_loop()
            results = await loop.run_in_executor(
                None,
                self._train_sync,
                data_yaml, epochs, imgsz, batch, device,
                patience, resume, amp, workers, project_dir, run_name,
            )

            # 训练完成后收集指标
            self._collect_metrics_from_results(results)

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
            logger.error(f"Training failed: {e}")
            return {"status": "failed", "error": str(e)}

    def _train_sync(self, data_yaml, epochs, imgsz, batch, device,
                    patience, resume, amp, workers, project_dir, run_name):
        """同步执行训练（在线程池中运行）"""
        # 添加自定义回调来捕获epoch进度
        def on_fit_epoch_end(trainer):
            """每个epoch结束时回调"""
            if self._stop_event.is_set():
                # 通过抛出异常来中断训练
                raise StopTrainingException("Training stopped by user")

            # 记录训练结果目录
            if hasattr(trainer, 'save_dir'):
                self._run_dir = str(trainer.save_dir)

            # 收集当前epoch指标
            epoch_data = {}
            if hasattr(trainer, 'epoch'):
                epoch_data["epoch"] = trainer.epoch + 1
                epoch_data["total_epochs"] = epochs
            if hasattr(trainer, 'loss'):
                loss = trainer.loss
                if isinstance(loss, (list, tuple)):
                    epoch_data["loss"] = loss[0] if len(loss) > 0 else 0
                    epoch_data["box_loss"] = loss[0] if len(loss) > 0 else 0
                    epoch_data["cls_loss"] = loss[1] if len(loss) > 1 else 0
                    epoch_data["dfl_loss"] = loss[2] if len(loss) > 2 else 0
                else:
                    epoch_data["loss"] = float(loss)
            if hasattr(trainer, 'metrics') and trainer.metrics:
                metrics = trainer.metrics
                for key in ['metrics/mAP50(B)', 'metrics/mAP50-95(B)',
                            'metrics/precision(B)', 'metrics/recall(B)']:
                    if key in metrics:
                        short_key = key.replace('metrics/', '').replace('(B)', '')
                        epoch_data[short_key] = float(metrics[key])

            # 保存最新指标
            self._metrics.update(epoch_data)

            # 通过回调发送epoch事件
            if self._on_epoch_end:
                try:
                    self._on_epoch_end(epoch_data)
                except Exception as e:
                    logger.warning(f"Epoch callback error: {e}")

        # 注册回调
        self._model.add_callback("on_fit_epoch_end", on_fit_epoch_end)

        # 执行训练
        train_kwargs = dict(
            data=data_yaml,
            epochs=epochs,
            imgsz=imgsz,
            batch=batch,
            device=device,
            patience=patience,
            resume=resume,
            amp=amp,
            workers=workers,
            verbose=True,
        )
        if project_dir:
            train_kwargs["project"] = project_dir
        if run_name:
            train_kwargs["name"] = run_name

        results = self._model.train(**train_kwargs)
        return results

    async def stop_training(self) -> dict:
        """停止训练"""
        self._status = "stopping"
        self._stop_event.set()
        return {"status": "stopping"}

    async def parse_logs(self, log_path: str) -> dict:
        """
        解析Ultralytics训练日志

        读取results.csv文件，提取每个epoch的训练指标
        """
        if not log_path or not os.path.isdir(log_path):
            return {"logs": []}

        csv_path = os.path.join(log_path, "results.csv")
        if not os.path.isfile(csv_path):
            return {"logs": []}

        try:
            import csv
            logs = []
            with open(csv_path, "r", encoding="utf-8") as f:
                # Ultralytics CSV有前导空格，需要处理
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
            logger.error(f"Failed to parse logs: {e}")
            return {"logs": [], "error": str(e)}

    async def collect_metrics(self, run_dir: str) -> dict:
        """
        收集训练指标

        从results.csv和训练结果中提取最终指标
        """
        metrics = dict(self._metrics)

        # 如果有运行目录，尝试从results.csv读取最终指标
        if run_dir and os.path.isdir(run_dir):
            csv_path = os.path.join(run_dir, "results.csv")
            if os.path.isfile(csv_path):
                try:
                    import csv
                    last_row = None
                    best_row = None
                    best_map = -1.0
                    with open(csv_path, "r", encoding="utf-8") as f:
                        reader = csv.DictReader(f, skipinitialspace=True)
                        for row in reader:
                            last_row = row
                            # 查找最佳mAP行
                            map_key = None
                            for k in row.keys():
                                if k and "mAP50-95" in k:
                                    map_key = k.strip()
                                    break
                            if map_key and row.get(map_key):
                                try:
                                    val = float(row[map_key].strip())
                                    if val > best_map:
                                        best_map = val
                                        best_row = row
                                except (ValueError, AttributeError):
                                    pass

                    if last_row:
                        metrics["final_epoch"] = self._extract_float(last_row, "epoch")
                    if best_row:
                        metrics["best_mAP50-95"] = best_map
                        metrics["best_epoch"] = self._extract_float(best_row, "epoch")

                except Exception as e:
                    logger.error(f"Failed to read results.csv: {e}")

            # 检查权重文件
            weights_dir = os.path.join(run_dir, "weights")
            if os.path.isdir(weights_dir):
                best_pt = os.path.join(weights_dir, "best.pt")
                last_pt = os.path.join(weights_dir, "last.pt")
                metrics["best_weight_exists"] = os.path.isfile(best_pt)
                metrics["last_weight_exists"] = os.path.isfile(last_pt)

        return {"metrics": metrics}

    def _collect_metrics_from_results(self, results):
        """从Ultralytics训练结果对象中提取指标"""
        if results is None:
            return

        try:
            # Ultralytics Results对象包含多种指标
            if hasattr(results, 'results_dict'):
                for key, value in results.results_dict.items():
                    clean_key = key.replace('metrics/', '').replace('(B)', '')
                    try:
                        self._metrics[clean_key] = float(value)
                    except (ValueError, TypeError):
                        self._metrics[clean_key] = value

            # 提取关键指标
            if hasattr(results, 'box'):
                box = results.box
                if hasattr(box, 'map50'):
                    self._metrics["mAP50"] = float(box.map50)
                if hasattr(box, 'map'):
                    self._metrics["mAP50-95"] = float(box.map)
                if hasattr(box, 'mp'):
                    self._metrics["precision"] = float(box.mp)
                if hasattr(box, 'mr'):
                    self._metrics["recall"] = float(box.mr)

            # 记录训练保存目录
            if hasattr(results, 'save_dir'):
                self._run_dir = str(results.save_dir)

        except Exception as e:
            logger.warning(f"Failed to extract metrics from results: {e}")

    @staticmethod
    def _extract_float(row: dict, key_pattern: str) -> Optional[float]:
        """从CSV行中提取浮点值"""
        for key, value in row.items():
            if key and key_pattern in key:
                try:
                    return float(value.strip()) if value and value.strip() else None
                except (ValueError, AttributeError):
                    return None
        return None

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


class StopTrainingException(Exception):
    """训练停止异常，用于中断训练线程"""
    pass
