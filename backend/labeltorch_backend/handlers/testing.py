"""测试处理器 - 模型评估任务管理

处理 testing.start / testing.stop / testing.status 命令
使用 Ultralytics val 接口进行模型评估
"""
import asyncio
import json
import logging
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

        # 使用 Ultralytics 进行模型评估
        try:
            from ultralytics import YOLO

            model = YOLO(weight_path)

            # 发送进度事件
            server.send_event("test.progress", task_id, {
                "task_id": task_id,
                "current": 0,
                "total": 1,
                "metrics": {},
            })

            # 运行验证
            results = model.val(
                data=data_path,
                batch=config.get("batch", 16),
                imgsz=config.get("img_size", 640),
                conf=config.get("conf_threshold", 0.25),
                iou=config.get("iou_threshold", 0.45),
                device=config.get("device", "auto"),
                verbose=False,
            )

            # 提取指标
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
                except Exception as e:
                    logger.warning(f"Failed to extract confusion matrix: {e}")

            # 发送完成事件
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

            logger.info(f"Test task completed: {task_id}, mAP50={metrics.get('mAP50', 0):.4f}")

        except ImportError:
            raise RuntimeError("Ultralytics not installed, cannot run testing")

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
