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
    from ..adapters.registry import TrainingAdapterRegistry

    task_id = payload.get("run_id", payload.get("task_id", "unknown"))
    config = payload.get("config", {})
    adapter_name = config.get("adapter", "ultralytics")

    adapter_class = TrainingAdapterRegistry.get(adapter_name)
    if adapter_class is None:
        return {"task_id": task_id, "status": "failed",
                "error": f"Unknown adapter: {adapter_name}. Available: {TrainingAdapterRegistry.list_adapters()}"}

    adapter = adapter_class()

    # 设置epoch回调，通过IPC推送进度事件
    server = get_server()
    def on_epoch_end(epoch_data: dict):
        """每个epoch结束时通过IPC推送进度"""
        try:
            epoch = epoch_data.get("epoch", 0)
            total = epoch_data.get("total_epochs", 0)
            loss = epoch_data.get("loss", 0)
            map50 = epoch_data.get("mAP50(B)", epoch_data.get("mAP50", 0))
            map50_95 = epoch_data.get("mAP50-95(B)", epoch_data.get("mAP50-95", 0))
            auroc = epoch_data.get("auroc", epoch_data.get("image_auroc", 0))
            pixel_auroc = epoch_data.get("pixel_auroc", 0)
            f1_score = epoch_data.get("f1", epoch_data.get("image_f1", 0))

            metrics_payload = {
                "mAP50": map50,
                "mAP50-95": map50_95,
                "precision": epoch_data.get("precision(B)", epoch_data.get("precision", 0)),
                "recall": epoch_data.get("recall(B)", epoch_data.get("recall", 0)),
                "map50": map50,
            }
            if auroc:
                metrics_payload["auroc"] = auroc
                metrics_payload["image_auroc"] = epoch_data.get("image_auroc", auroc)
            if pixel_auroc:
                metrics_payload["pixel_auroc"] = pixel_auroc
            if f1_score:
                metrics_payload["f1"] = f1_score

            server.send_event("task.progress", task_id, {
                "task_id": task_id,
                "epoch": epoch,
                "total_epochs": total,
                "loss": loss,
                "mAP50": map50,
                "mAP50-95": map50_95,
                "precision": epoch_data.get("precision(B)", epoch_data.get("precision", 0)),
                "recall": epoch_data.get("recall(B)", epoch_data.get("recall", 0)),
                "auroc": auroc,
                "pixel_auroc": pixel_auroc,
                "f1": f1_score,
                "metrics": metrics_payload,
            })

            # 同时发送日志事件，让UI层实时显示训练进度
            log_msg = f"Epoch {epoch}/{total} - loss: {loss:.4f}"
            if map50:
                log_msg += f", mAP50: {map50:.4f}"
            if map50_95:
                log_msg += f", mAP50-95: {map50_95:.4f}"
            if auroc:
                log_msg += f", AUROC: {auroc:.4f}"
            server.send_event("task.log", task_id, {
                "task_id": task_id,
                "message": log_msg,
            })
        except Exception as e:
            logger.warning(f"Failed to send epoch event: {e}")

    adapter.set_epoch_callback(on_epoch_end)

    _active_tasks[task_id] = adapter

    asyncio.create_task(_run_training(task_id, adapter, config, server))

    return {"task_id": task_id, "status": "started"}


async def _run_training(task_id: str, adapter, config: dict, server):
    """异步执行训练，发送IPC事件"""

    try:
        server.send_event("task.started", task_id, {
            "task_id": task_id,
            "config": config,
        })

        # 发送训练配置日志
        model_family = config.get("model_family", "yolov8")
        epochs = config.get("epochs", 100)
        batch = config.get("batch", 16)
        device = config.get("device", "auto")
        imgsz = config.get("imgsz", config.get("img_size", 640))
        server.send_event("task.log", task_id, {
            "task_id": task_id,
            "message": f"Training config: model={model_family}, epochs={epochs}, batch={batch}, device={device}, imgsz={imgsz}",
        })

        result = await adapter.start_training(config)

        if result.get("status") == "succeeded":
            # 优先使用adapter返回的run_dir，否则从config获取
            run_dir = result.get("run_dir", "") or config.get("run_dir", "")
            best_weight = _find_best_weight(run_dir)
            last_weight = _find_last_weight(run_dir)
            metrics = await adapter.collect_metrics(run_dir) if run_dir else {}

            server.send_event("task.log", task_id, {
                "task_id": task_id,
                "message": f"Training completed! best_weight={best_weight}, last_weight={last_weight}",
            })

            server.send_event("task.succeeded", task_id, {
                "task_id": task_id,
                "epochs_completed": config.get("epochs", 0),
                "early_stopped": False,
                "best_weight_path": best_weight,
                "last_weight_path": last_weight,
                "run_dir": run_dir,
                "metrics": metrics.get("metrics", {}),
            })
        elif result.get("status") == "stopped":
            # 用户手动停止训练
            run_dir = result.get("run_dir", "") or config.get("run_dir", "")
            best_weight = _find_best_weight(run_dir)
            last_weight = _find_last_weight(run_dir)

            server.send_event("task.stopped", task_id, {
                "task_id": task_id,
                "best_weight_path": best_weight,
                "last_weight_path": last_weight,
                "run_dir": run_dir,
            })
        else:
            error_msg = result.get("error", "Unknown error")
            server.send_event("task.log", task_id, {
                "task_id": task_id,
                "message": f"Training failed: {error_msg}",
            })
            server.send_event("task.failed", task_id, {
                "task_id": task_id,
                "error": error_msg,
            })

    except Exception as e:
        logger.error(f"Training {task_id} failed: {e}")
        server.send_event("task.log", task_id, {
            "task_id": task_id,
            "message": f"Training exception: {str(e)}",
        })
        server.send_event("task.failed", task_id, {
            "task_id": task_id,
            "error": str(e),
        })
    finally:
        _active_tasks.pop(task_id, None)


def _find_best_weight(run_dir: str) -> str:
    """查找训练产出的最佳权重文件（支持 YOLO best.pt 和 anomalib .ckpt）"""
    if not run_dir:
        return ""
    # YOLO 格式: weights/best.pt
    best_path = os.path.join(run_dir, "weights", "best.pt")
    if os.path.isfile(best_path):
        return best_path
    # anomalib 格式: 递归查找包含 "best" 的 .ckpt 文件
    import glob
    ckpt_files = glob.glob(os.path.join(run_dir, "**", "*best*.ckpt"), recursive=True)
    if ckpt_files:
        return ckpt_files[0]
    # 兜底：查找任意 .ckpt 文件
    ckpt_files = glob.glob(os.path.join(run_dir, "**", "*.ckpt"), recursive=True)
    if ckpt_files:
        return ckpt_files[-1]
    return ""


def _find_last_weight(run_dir: str) -> str:
    """查找训练产出的最新权重文件（支持 YOLO last.pt 和 anomalib .ckpt）"""
    if not run_dir:
        return ""
    # YOLO 格式: weights/last.pt
    last_path = os.path.join(run_dir, "weights", "last.pt")
    if os.path.isfile(last_path):
        return last_path
    # anomalib 格式: 递归查找包含 "last" 的 .ckpt 文件
    import glob
    ckpt_files = glob.glob(os.path.join(run_dir, "**", "*last*.ckpt"), recursive=True)
    if ckpt_files:
        return ckpt_files[0]
    return ""


async def handle_stop(payload: dict) -> dict:
    """停止训练任务"""
    task_id = payload.get("run_id", payload.get("task_id", ""))
    adapter = _active_tasks.get(task_id)
    if adapter:
        await adapter.stop_training()
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
    from ..adapters.registry import TrainingAdapterRegistry
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
