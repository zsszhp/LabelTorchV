"""
训练命令处理器

P1-19：训练失败可诊断——保留日志尾部并返回结构化建议
"""
import asyncio
import collections
import logging
import os

from ..server import get_server
from ..tools.data_split import split_dataset

logger = logging.getLogger(__name__)

_active_tasks = {}

# 每个任务保留最近 N 行日志，失败时随 task.failed 事件回传（P1-19）
LOG_TAIL_LINES = 50
_log_tails: dict[str, collections.deque] = {}


def _append_log_tail(task_id: str, message: str) -> None:
    """维护任务日志环形缓冲区。"""
    if task_id not in _log_tails:
        _log_tails[task_id] = collections.deque(maxlen=LOG_TAIL_LINES)
    _log_tails[task_id].append(str(message))


def _take_log_tail(task_id: str) -> list[str]:
    """取出并清理任务日志尾部。"""
    tail = list(_log_tails.pop(task_id, []))
    return tail


def diagnose_failure(error_message: str, config: dict | None = None) -> dict:
    """P1-19：根据失败信息生成结构化诊断建议。

    返回：
      {
        "code": "OOM" | "DATA_ERROR" | "CONFIG_ERROR" | "ENV_ERROR" | "UNKNOWN",
        "message": 中文摘要,
        "suggestions": [中文建议, ...]
      }
    """
    text = str(error_message or "")
    lowered = text.lower()
    config = config or {}

    def _batch_imgsz_hint() -> str:
        batch = config.get("batch", 16)
        imgsz = config.get("imgsz", config.get("img_size", 640))
        new_batch = max(1, int(batch) // 2) if batch else 8
        new_imgsz = 320 if int(imgsz or 640) >= 640 else max(128, int(imgsz or 640) // 2)
        return f"建议将 batch 降至 {new_batch}、imgsz 降至 {new_imgsz}"

    if any(k in lowered for k in ("cuda out of memory", "out of memory", "oom", "cudnn_status_alloc_failed")):
        return {
            "code": "OOM",
            "message": "显存不足（OOM）导致训练失败",
            "suggestions": [
                _batch_imgsz_hint(),
                "建议关闭混合精度以外的大 batch 累积，或改用更小模型",
                "建议确认无其他进程占用 GPU 显存",
            ],
        }

    if any(k in lowered for k in (
        "no labels found", "label", "dataset", "corrupt", "truncated",
        "file not found", "filenotfounderror", "no such file",
        "image file", "cannot identify image", "empty dataset",
    )):
        return {
            "code": "DATA_ERROR",
            "message": "数据集或标签存在问题导致训练失败",
            "suggestions": [
                "建议检查标签文件是否存在且格式正确（每行 class_id cx cy w h，归一化到 0-1）",
                "建议检查图片路径与标签路径对应关系、文件是否损坏",
                "建议运行数据集校验/导入扫描，确认 sample_count 与实际一致",
            ],
        }

    if any(k in lowered for k in (
        "config", "keyerror", "invalid", "unexpected key", "size mismatch",
        "shape", "missing key", "state_dict",
    )):
        return {
            "code": "CONFIG_ERROR",
            "message": "训练配置或权重不匹配导致训练失败",
            "suggestions": [
                "建议检查模型族（model_family）与任务类型是否匹配",
                "建议检查类别数（nc）与数据集类别定义是否一致",
                "建议检查断点续训权重与当前配置是否兼容",
            ],
        }

    if any(k in lowered for k in (
        "cuda", "cudnn", "device", "driver", "outofmemoryerror",
        "no module named", "importerror", "dll",
    )):
        return {
            "code": "ENV_ERROR",
            "message": "运行环境异常导致训练失败",
            "suggestions": [
                "建议在设置页运行环境检查，确认 Python/CUDA/PyTorch 可用",
                "建议改用 CPU（device=cpu）验证是否为 GPU 环境问题",
                "建议确认 labeltorch 环境依赖已安装完整",
            ],
        }

    return {
        "code": "UNKNOWN",
        "message": "训练失败，原因未归类",
        "suggestions": [
            "建议查看日志尾部定位具体报错",
            "建议降低 batch / epochs 后重试",
            "如问题持续，请保留日志尾部以便排查",
        ],
    }


async def handle_start(payload: dict) -> dict:
    """启动训练任务"""
    from ..adapters.registry import TrainingAdapterRegistry

    task_id = payload.get("run_id", payload.get("task_id", "unknown"))
    config = payload.get("config", {})
    adapter_name = config.get("adapter", "ultralytics")

    adapter_class = TrainingAdapterRegistry.get(adapter_name)
    if adapter_class is None:
        return {
            "task_id": task_id,
            "status": "failed",
            "error": {
                "code": "UNKNOWN_ADAPTER",
                "message": f"Unknown adapter: {adapter_name}. Available: {TrainingAdapterRegistry.list_adapters()}",
            },
        }

    adapter = adapter_class()

    # P0-17：启动训练前必须走适配器配置校验，拦截非法配置，
    # 避免 validate_config 沦为死代码、错误配置进入训练线程才失败
    try:
        validation = await adapter.validate_config(config)
    except Exception as e:
        logger.error(f"validate_config raised for adapter {adapter_name}: {e}")
        return {
            "task_id": task_id,
            "status": "failed",
            "error": {
                "code": "CONFIG_INVALID",
                "message": f"训练配置校验异常: {e}",
            },
        }

    if not isinstance(validation, dict) or not validation.get("valid", False):
        errors = []
        if isinstance(validation, dict):
            errors = validation.get("errors") or []
            if isinstance(errors, str):
                errors = [errors]
        if not errors:
            errors = ["配置校验未通过"]
        return {
            "task_id": task_id,
            "status": "failed",
            "error": {
                "code": "CONFIG_INVALID",
                "message": "训练配置校验失败: " + "; ".join(str(x) for x in errors),
                "details": [str(x) for x in errors],
            },
        }

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
            # 分类指标
            top1 = epoch_data.get("top1", 0)
            top5 = epoch_data.get("top5", 0)

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
            # 分类指标
            if top1:
                metrics_payload["top1"] = top1
            if top5:
                metrics_payload["top5"] = top5

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
                "top1": top1,
                "top5": top5,
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
            if top1:
                log_msg += f", top1: {top1:.4f}"
            if top5:
                log_msg += f", top5: {top5:.4f}"
            _append_log_tail(task_id, log_msg)
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
        config_msg = f"Training config: model={model_family}, epochs={epochs}, batch={batch}, device={device}, imgsz={imgsz}"
        _append_log_tail(task_id, config_msg)
        server.send_event("task.log", task_id, {
            "task_id": task_id,
            "message": config_msg,
        })

        result = await adapter.start_training(config)

        if result.get("status") == "succeeded":
            # 优先使用adapter返回的run_dir，否则从config获取
            run_dir = result.get("run_dir", "") or config.get("run_dir", "")
            best_weight = _find_best_weight(run_dir)
            last_weight = _find_last_weight(run_dir)
            metrics = await adapter.collect_metrics(run_dir) if run_dir else {}

            done_msg = f"Training completed! best_weight={best_weight}, last_weight={last_weight}"
            _append_log_tail(task_id, done_msg)
            server.send_event("task.log", task_id, {
                "task_id": task_id,
                "message": done_msg,
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
            _take_log_tail(task_id)  # 成功路径清理缓冲
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
            _take_log_tail(task_id)
        else:
            error_msg = result.get("error", "Unknown error")
            # 结构化错误 {code, message} 取 message，保证事件 payload 为字符串
            error_code = ""
            if isinstance(error_msg, dict):
                error_code = error_msg.get("code", "")
                error_msg = error_msg.get("message", str(error_msg))
            fail_log = f"Training failed: {error_msg}"
            _append_log_tail(task_id, fail_log)
            server.send_event("task.log", task_id, {
                "task_id": task_id,
                "message": fail_log,
            })
            # P1-19：失败事件携带日志尾部 + 结构化诊断建议
            diagnosis = diagnose_failure(error_msg, config)
            if error_code and diagnosis.get("code") == "UNKNOWN":
                diagnosis["code"] = error_code
            server.send_event("task.failed", task_id, {
                "task_id": task_id,
                "error": error_msg,
                "log_tail": _take_log_tail(task_id),
                "diagnosis": diagnosis,
            })

    except Exception as e:
        logger.error(f"Training {task_id} failed: {e}")
        fail_msg = f"Training exception: {str(e)}"
        _append_log_tail(task_id, fail_msg)
        server.send_event("task.log", task_id, {
            "task_id": task_id,
            "message": fail_msg,
        })
        # P1-19：异常路径同样输出日志尾部 + 结构化诊断建议
        diagnosis = diagnose_failure(str(e), config)
        server.send_event("task.failed", task_id, {
            "task_id": task_id,
            "error": str(e),
            "log_tail": _take_log_tail(task_id),
            "diagnosis": diagnosis,
        })
    finally:
        _active_tasks.pop(task_id, None)
        _log_tails.pop(task_id, None)


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
    from ..tools.path_security import collect_allowed_roots, ensure_within

    image_dir = payload.get("image_dir", "")
    label_dir = payload.get("label_dir", "")
    output_dir = payload.get("output_dir", "")
    val_ratio = payload.get("val_ratio", 0.2)
    seed = payload.get("seed", 42)

    if not image_dir or not label_dir or not output_dir:
        return {"error": {"code": "MISSING_PARAMS", "message": "Missing required parameters: image_dir, label_dir, output_dir"}}

    # 路径白名单：输入目录可来自请求内声明的数据根；输出目录仅允许项目根/快照/临时目录
    read_roots = collect_allowed_roots(payload, extra_roots=[image_dir, label_dir], include_data_dirs=True)
    write_roots = collect_allowed_roots(payload, include_data_dirs=False)
    for field, value in (("image_dir", image_dir), ("label_dir", label_dir)):
        err = ensure_within(value, read_roots, field=field, must_exist=False)
        if isinstance(err, dict):
            return err
    err = ensure_within(output_dir, write_roots, field="output_dir", must_exist=False)
    if isinstance(err, dict):
        return err

    result = split_dataset(image_dir, label_dir, output_dir, val_ratio, seed)
    return result
