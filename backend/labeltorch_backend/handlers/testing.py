"""测试处理器 - 模型评估任务管理

处理 testing.start / testing.stop / testing.status 命令
按任务类型分发到 Ultralytics 或 Anomalib 评估流程
"""
import asyncio
import logging
import os
from typing import Any

logger = logging.getLogger(__name__)

# 活跃测试任务
_active_tasks: dict[str, asyncio.Task] = {}


async def handle_start(payload: dict) -> dict:
    """启动测试任务"""
    task_id = payload.get("task_id", "unknown")
    model_version_id = payload.get("model_version_id", "")
    snapshot_id = payload.get("snapshot_id", "")
    config = payload.get("config", {})

    logger.info(f"Starting test task: {task_id}")

    # 启动后台测试任务
    from ..server import get_server
    server = get_server()

    task = asyncio.create_task(
        _run_testing(task_id, model_version_id, snapshot_id, config, server)
    )
    _active_tasks[task_id] = task

    return {"task_id": task_id, "status": "started"}


async def handle_stop(payload: dict) -> dict:
    """停止测试任务"""
    task_id = payload.get("task_id", "")
    if task_id in _active_tasks:
        _active_tasks[task_id].cancel()
        _active_tasks.pop(task_id, None)
        logger.info(f"Test task stopped: {task_id}")
        return {"task_id": task_id, "status": "stopped"}
    return {"task_id": task_id, "status": "not_found"}


async def handle_status(payload: dict) -> dict:
    """查询测试任务状态"""
    task_id = payload.get("task_id", "")
    is_active = task_id in _active_tasks
    return {"task_id": task_id, "is_active": is_active}


async def _run_testing(
    task_id: str,
    model_version_id: str,
    snapshot_id: str,
    config: dict,
    server: Any,
) -> None:
    """执行测试任务的后台协程"""
    try:
        server.send_event("test.started", task_id, {"task_id": task_id})

        # 获取模型权重路径和快照数据路径
        # 这里需要从数据库查询，简化处理使用配置中的路径
        weight_path = config.get("weight_path", "")
        data_path = config.get("data_path", "")

        if not weight_path or not data_path:
            raise ValueError("Missing weight_path or data_path in config")

        task_type = config.get("task_type", "detect")
        server.send_event("test.progress", task_id, {
            "task_id": task_id,
            "current": 0,
            "total": 1,
            "metrics": {},
        })

        if task_type == "anomaly":
            metrics, confusion_matrix, pr_curve = await _run_anomaly_testing(weight_path, data_path, config)
            logger.info(
                "Anomaly test task completed: %s, AUROC=%.4f",
                task_id,
                metrics.get("auroc", 0.0),
            )
        else:
            metrics, confusion_matrix, pr_curve = await _run_ultralytics_testing(weight_path, data_path, config)
            logger.info(
                "Detection test task completed: %s, mAP50=%.4f",
                task_id,
                metrics.get("mAP50", 0.0),
            )

        server.send_event("test.progress", task_id, {
            "task_id": task_id,
            "current": 1,
            "total": 1,
            "metrics": metrics,
        })

        server.send_event("test.succeeded", task_id, {
            "task_id": task_id,
            "metrics": metrics,
            "confusion_matrix": confusion_matrix,
            "pr_curve": pr_curve,
        })

    except asyncio.CancelledError:
        server.send_event("test.stopped", task_id, {"task_id": task_id})
        logger.info(f"Test task cancelled: {task_id}")

    except Exception as e:
        server.send_event("test.failed", task_id, {
            "task_id": task_id,
            "error": str(e),
        })
        logger.error(f"Test task failed: {task_id}, error: {e}")

    finally:
        _active_tasks.pop(task_id, None)


async def _run_ultralytics_testing(weight_path: str, data_path: str, config: dict):
    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise RuntimeError("Ultralytics not installed, cannot run testing") from exc

    model = YOLO(weight_path)

    loop = asyncio.get_event_loop()
    results = await loop.run_in_executor(
        None,
        lambda: model.val(
            data=data_path,
            batch=config.get("batch", 16),
            imgsz=config.get("imgsz", config.get("img_size", 640)),
            conf=config.get("conf_threshold", 0.25),
            iou=config.get("iou_threshold", 0.45),
            device=config.get("device", "auto"),
            verbose=False,
        ),
    )

    metrics = {}
    confusion_matrix = {}
    pr_curve = []

    if hasattr(results, "box") and results.box:
        metrics = {
            "mAP50": float(results.box.map50) if results.box.map50 is not None else 0,
            "mAP50-95": float(results.box.map) if results.box.map is not None else 0,
            "precision": float(results.box.mp) if results.box.mp is not None else 0,
            "recall": float(results.box.mr) if results.box.mr is not None else 0,
            "f1": float(results.box.f1) if hasattr(results.box, "f1") and results.box.f1 is not None else 0,
        }

    if hasattr(results, "confusion_matrix") and results.confusion_matrix is not None:
        try:
            cm = results.confusion_matrix.matrix
            confusion_matrix = {
                "matrix": cm.tolist() if hasattr(cm, "tolist") else [],
                "names": list(results.names.values()) if hasattr(results, "names") else [],
            }
        except Exception as exc:
            logger.warning("Failed to extract confusion matrix: %s", exc)

    return metrics, confusion_matrix, pr_curve


async def _run_anomaly_testing(weight_path: str, data_path: str, config: dict):
    try:
        import torch
        from anomalib.data import Folder
        from anomalib.engine import Engine
        from anomalib.models import get_model
    except ImportError as exc:
        raise RuntimeError("anomalib not installed, cannot run anomaly testing") from exc

    if not os.path.isdir(os.path.dirname(data_path)) and not os.path.isdir(data_path):
        raise RuntimeError(f"Anomaly dataset path not found: {data_path}")

    dataset_dir = os.path.dirname(data_path) if data_path.endswith(".yaml") else data_path
    model_family = config.get("model_family", "patchcore")
    imgsz = config.get("imgsz", config.get("img_size", 256))
    batch = config.get("batch", 16)
    device = config.get("device", "auto")

    model = get_model(model_family)
    checkpoint = torch.load(weight_path, map_location="cpu", weights_only=False)
    if "state_dict" in checkpoint:
        model.load_state_dict(checkpoint["state_dict"])
    elif "model" in checkpoint:
        state = checkpoint["model"].state_dict() if hasattr(checkpoint["model"], "state_dict") else checkpoint["model"]
        model.load_state_dict(state)

    model.eval()

    normal_test_dir = "test/good" if os.path.isdir(os.path.join(dataset_dir, "test", "good")) else None
    abnormal_dir = "test/defective" if os.path.isdir(os.path.join(dataset_dir, "test", "defective")) else None
    if not normal_test_dir:
        raise RuntimeError("Anomaly test dataset missing test/good directory")

    datamodule = Folder(
        name="product",
        root=dataset_dir,
        normal_dir="train/good",
        normal_test_dir=normal_test_dir,
        abnormal_dir=abnormal_dir,
        image_size=(imgsz, imgsz),
        train_batch_size=batch,
        eval_batch_size=batch,
        num_workers=0,
    )

    engine_kwargs = {}
    if device and device != "auto":
        actual_acc = "gpu" if ("cuda" in device or device.isdigit()) and torch.cuda.is_available() else "cpu"
        engine_kwargs["accelerator"] = actual_acc
        if actual_acc == "gpu" and device.isdigit():
            engine_kwargs["devices"] = [int(device)]
    else:
        engine_kwargs["accelerator"] = "gpu" if torch.cuda.is_available() else "cpu"

    engine = Engine(**engine_kwargs)
    loop = asyncio.get_event_loop()
    await loop.run_in_executor(None, lambda: engine.test(model=model, datamodule=datamodule))

    metrics = {}
    confusion_matrix = {"matrix": [], "names": ["good", "defective"]}
    pr_curve = []

    callback_metrics = getattr(getattr(engine, "trainer", None), "callback_metrics", {}) or {}
    for key, value in callback_metrics.items():
        try:
            numeric_value = float(value.item() if hasattr(value, "item") else value)
        except (TypeError, ValueError):
            continue
        key_text = str(key)
        if "image_AUROC" in key_text or "image_auroc" in key_text:
            metrics["auroc"] = numeric_value
            metrics["image_auroc"] = numeric_value
        elif "pixel_AUROC" in key_text or "pixel_auroc" in key_text:
            metrics["pixel_auroc"] = numeric_value
        elif "image_F1Score" in key_text or "image_f1score" in key_text:
            metrics["f1"] = numeric_value

    return metrics, confusion_matrix, pr_curve
