"""
训练命令处理器
"""
import asyncio
import logging

logger = logging.getLogger(__name__)

# 活跃训练任务
_active_tasks = {}


async def handle_start(payload: dict) -> dict:
    """启动训练任务"""
    from ..adapters.ultralytics_adapter import UltralyticsAdapter

    task_id = payload.get("task_id", "unknown")
    config = payload.get("config", {})

    adapter = UltralyticsAdapter()
    _active_tasks[task_id] = adapter

    # 异步启动训练
    asyncio.create_task(_run_training(task_id, adapter, config))

    return {"task_id": task_id, "status": "started"}


async def _run_training(task_id: str, adapter, config: dict):
    """异步执行训练"""
    try:
        result = await adapter.start_training(config)
        logger.info(f"Training {task_id} completed: {result}")
    except Exception as e:
        logger.error(f"Training {task_id} failed: {e}")
    finally:
        _active_tasks.pop(task_id, None)


async def handle_stop(payload: dict) -> dict:
    """停止训练任务"""
    task_id = payload.get("task_id", "")
    adapter = _active_tasks.get(task_id)
    if adapter:
        adapter.stop_training()
        return {"task_id": task_id, "status": "stopping"}
    return {"task_id": task_id, "status": "not_found"}


async def handle_status(payload: dict) -> dict:
    """查询训练状态"""
    task_id = payload.get("task_id", "")
    adapter = _active_tasks.get(task_id)
    if adapter:
        return adapter.get_status()
    return {"task_id": task_id, "status": "not_found"}
