"""
训练命令处理器
"""
import asyncio
import logging
import os

logger = logging.getLogger(__name__)

# 活跃训练任务
_active_tasks = {}


async def handle_check_resume(payload: dict) -> dict:
    """检测是否存在可续训的checkpoint"""
    run_dir = payload.get("run_dir", "")
    if not run_dir:
        return {"resumable": False, "reason": "no_run_dir"}

    last_pt = os.path.join(run_dir, "weights", "last.pt")
    if os.path.isfile(last_pt):
        return {"resumable": True, "checkpoint": last_pt}
    return {"resumable": False, "reason": "no_checkpoint"}


async def handle_start(payload: dict) -> dict:
    """启动训练任务"""
    from ..adapters.registry import TrainingAdapterRegistry, register_builtin_adapters

    register_builtin_adapters()  # Ensure built-ins are registered

    # C++ frontend sends "run_id", accept both keys for compatibility
    task_id = payload.get("run_id", payload.get("task_id", "unknown"))
    config = payload.get("config", {})
    adapter_name = config.get("adapter", "ultralytics")

    adapter_class = TrainingAdapterRegistry.get(adapter_name)
    if adapter_class is None:
        return {"task_id": task_id, "status": "error", "error": f"Unknown adapter: {adapter_name}. Available: {TrainingAdapterRegistry.list_adapters()}"}

    adapter = adapter_class()
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
