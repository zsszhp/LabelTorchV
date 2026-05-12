"""
训练命令处理器
"""
import asyncio
import logging
import os

from ..server import get_server
from ..tools.data_split import split_dataset

logger = logging.getLogger(__name__)

_active_tasks = {}


async def handle_start(payload: dict) -> dict:
    """启动训练任务"""
    from ..adapters.registry import TrainingAdapterRegistry, register_builtin_adapters

    register_builtin_adapters()

    task_id = payload.get("run_id", payload.get("task_id", "unknown"))
    config = payload.get("config", {})
    adapter_name = config.get("adapter", "ultralytics")

    adapter_class = TrainingAdapterRegistry.get(adapter_name)
    if adapter_class is None:
        return {"task_id": task_id, "status": "error",
                "error": f"Unknown adapter: {adapter_name}. Available: {TrainingAdapterRegistry.list_adapters()}"}

    adapter = adapter_class()
    _active_tasks[task_id] = adapter

    asyncio.create_task(_run_training(task_id, adapter, config))

    return {"task_id": task_id, "status": "started"}


async def _run_training(task_id: str, adapter, config: dict):
    """异步执行训练，发送IPC事件"""
    server = get_server()

    try:
        server.send_event("task.started", task_id, {
            "task_id": task_id,
            "config": config,
        })

        result = await adapter.start_training(config)

        if result.get("status") == "succeeded":
            run_dir = config.get("run_dir", "")
            best_weight = _find_best_weight(run_dir)
            last_weight = _find_last_weight(run_dir)
            metrics = await adapter.collect_metrics(run_dir) if run_dir else {}

            server.send_event("task.succeeded", task_id, {
                "task_id": task_id,
                "epochs_completed": config.get("epochs", 0),
                "early_stopped": False,
                "best_weight_path": best_weight,
                "last_weight_path": last_weight,
                "run_dir": run_dir,
                "metrics": metrics.get("metrics", {}),
            })
        else:
            server.send_event("task.failed", task_id, {
                "task_id": task_id,
                "error": result.get("error", "Unknown error"),
            })

    except Exception as e:
        logger.error(f"Training {task_id} failed: {e}")
        server.send_event("task.failed", task_id, {
            "task_id": task_id,
            "error": str(e),
        })
    finally:
        _active_tasks.pop(task_id, None)


def _find_best_weight(run_dir: str) -> str:
    """查找训练产出的best.pt"""
    if not run_dir:
        return ""
    best_path = os.path.join(run_dir, "weights", "best.pt")
    return best_path if os.path.isfile(best_path) else ""


def _find_last_weight(run_dir: str) -> str:
    """查找训练产出的last.pt"""
    if not run_dir:
        return ""
    last_path = os.path.join(run_dir, "weights", "last.pt")
    return last_path if os.path.isfile(last_path) else ""


async def handle_stop(payload: dict) -> dict:
    """停止训练任务"""
    task_id = payload.get("run_id", payload.get("task_id", ""))
    adapter = _active_tasks.get(task_id)
    if adapter:
        adapter.stop_training()
        return {"task_id": task_id, "status": "stopping"}
    return {"task_id": task_id, "status": "not_found"}


async def handle_status(payload: dict) -> dict:
    """查询训练状态"""
    task_id = payload.get("run_id", payload.get("task_id", ""))
    adapter = _active_tasks.get(task_id)
    if adapter:
        return adapter.get_status()
    return {"task_id": task_id, "status": "not_found"}


async def handle_list_adapters(payload: dict) -> dict:
    """列出所有已注册的训练适配器"""
    from ..adapters.registry import TrainingAdapterRegistry, register_builtin_adapters
    register_builtin_adapters()
    return {"adapters": TrainingAdapterRegistry.list_adapters()}


async def handle_data_split(payload: dict) -> dict:
    """数据集划分"""
    image_dir = payload.get("image_dir", "")
    label_dir = payload.get("label_dir", "")
    output_dir = payload.get("output_dir", "")
    val_ratio = payload.get("val_ratio", 0.2)
    seed = payload.get("seed", 42)

    if not image_dir or not label_dir or not output_dir:
        return {"error": "Missing required parameters: image_dir, label_dir, output_dir"}

    result = split_dataset(image_dir, label_dir, output_dir, val_ratio, seed)
    return result
