"""测试处理器 - 模型评估任务管理

处理 testing.start / testing.stop / testing.status 命令
按任务类型分发到 Ultralytics 或 Anomalib 评估流程

P1-16：从 Ultralytics results 真实提取 PR/F1/置信度曲线，替换恒返回 []
P1-17：部署阈值推荐（F1 最优点 / 漏检超检代价最优点）+ Go/No-Go 结论
P1-18：逐类 AP/P/R 与漏检排序，接入 FP/FN 队列数据
"""
import asyncio
import logging
import math
import os
from typing import Any

logger = logging.getLogger(__name__)

# 活跃测试任务
_active_tasks: dict[str, asyncio.Task] = {}

# 曲线采样点数（与 ultralytics ap_per_class 的 1000 点置信度网格对齐）
CURVE_POINTS = 1000

# Go/No-Go 门限（P1-17）：检测 mAP50>0.85，异常 AUROC>0.95
GATE_MAP50 = 0.85
GATE_AUROC = 0.95


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

        # 路径白名单：权重与数据路径必须落在项目根、快照目录或系统临时目录内
        from ..tools.path_security import collect_allowed_roots, require_path_within

        allowed_roots = collect_allowed_roots(config, extra_roots=[data_path])
        require_path_within(weight_path, allowed_roots, field="weight_path", must_exist=True)
        require_path_within(data_path, allowed_roots, field="data_path", must_exist=False)

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
            # P1-17：结构化结论卡，便于 UI 直接绑定
            "threshold_recommendation": metrics.get("threshold_recommendation", {}),
            "go_no_go": metrics.get("go_no_go", {}),
            # P1-18：逐类指标
            "per_class": metrics.get("per_class", []),
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


# ============================================================================
# P1-16：曲线提取工具
# ============================================================================

def _curve_points_from_xy(x_vals, y_vals) -> list[dict]:
    """将一维曲线转成 [{"confidence": x, "value": y}] 点列（浮点安全）。"""
    points = []
    if x_vals is None or y_vals is None:
        return points
    try:
        xs = list(x_vals)
        ys = list(y_vals)
    except TypeError:
        return points
    for x, y in zip(xs, ys):
        try:
            xf = float(x)
            yf = float(y)
        except (TypeError, ValueError):
            continue
        if not (math.isfinite(xf) and math.isfinite(yf)):
            continue
        points.append({"confidence": xf, "value": yf})
    return points


def _extract_pr_curve_from_results(results) -> list[dict]:
    """从 Ultralytics results 提取 PR 曲线点。

    优先使用 results.curves_results / results.box.curves_results：
      curves_results[0] = [px, prec_values, "Recall", "Precision"]
    其中 px 为 recall 轴（0..1），prec_values 为对应 precision（按类平均）。
    输出字段（前端绑定约定）：recall / precision / confidence / f1
    """
    curves = None
    if hasattr(results, "curves_results"):
        try:
            curves = results.curves_results
        except Exception:
            curves = None
    if not curves and hasattr(results, "box") and results.box is not None:
        try:
            curves = results.box.curves_results
        except Exception:
            curves = None

    pr_points: list[dict] = []
    if curves and len(curves) > 0:
        try:
            entry = curves[0]
            # entry = [x(recall), y(precision), x_label, y_label]
            xs = entry[0]
            ys = entry[1]
            # 多类时 ys 形状 (nc, 1000)，按类取均值作为整体 PR 曲线
            import numpy as np

            ys_arr = np.asarray(ys, dtype=float)
            if ys_arr.ndim == 2:
                ys_mean = ys_arr.mean(axis=0)
            else:
                ys_mean = ys_arr
            xs_arr = np.asarray(xs, dtype=float).reshape(-1)

            # F1 曲线按置信度网格（curves_results[1] = F1-Confidence）
            f1_by_conf = {}
            if len(curves) > 1:
                f1_entry = curves[1]
                f1_xs = np.asarray(f1_entry[0], dtype=float).reshape(-1)
                f1_ys = np.asarray(f1_entry[1], dtype=float)
                if f1_ys.ndim == 2:
                    f1_ys = f1_ys.mean(axis=0)
                for cx, cy in zip(f1_xs, f1_ys):
                    f1_by_conf[round(float(cx), 4)] = float(cy)

            for rx, py in zip(xs_arr, ys_mean):
                rf = float(rx)
                pf = float(py)
                if not (math.isfinite(rf) and math.isfinite(pf)):
                    continue
                # PR 置信度轴与 F1 置信度轴不同；confidence 字段填该点 F1 轴近似值，
                # 无法对齐时填 0，前端仅在需要时展示
                conf_guess = f1_by_conf.get(round(rf, 4), 0.0)
                # F1 由 P/R 现算，保证 pr_curve 点自带 f1 字段
                f1_val = (2 * pf * rf / (pf + rf)) if (pf + rf) > 0 else 0.0
                pr_points.append({
                    "recall": rf,
                    "precision": pf,
                    "confidence": conf_guess,
                    "f1": f1_val,
                })
        except Exception as exc:
            logger.warning("Failed to build PR curve points from curves_results: %s", exc)

    return pr_points


def _extract_confidence_curves(results) -> tuple[list[dict], list[dict], list[dict]]:
    """提取 F1/P/R 关于置信度的曲线（P1-16），返回 (f1_curve, p_curve, r_curve)。

    点格式：{"confidence": float, "f1"|"precision"|"recall": float}
    """
    curves = None
    if hasattr(results, "curves_results"):
        try:
            curves = results.curves_results
        except Exception:
            curves = None
    if not curves and hasattr(results, "box") and results.box is not None:
        try:
            curves = results.box.curves_results
        except Exception:
            curves = None

    f1_curve: list[dict] = []
    p_curve: list[dict] = []
    r_curve: list[dict] = []

    if not curves or len(curves) < 4:
        return f1_curve, p_curve, r_curve

    import numpy as np

    def _mean_ys(entry):
        xs = np.asarray(entry[0], dtype=float).reshape(-1)
        ys = np.asarray(entry[1], dtype=float)
        if ys.ndim == 2:
            ys = ys.mean(axis=0)
        return xs, ys

    try:
        # curves_results[1] = F1-Confidence, [2] = P-Confidence, [3] = R-Confidence
        for idx, target, key in ((1, f1_curve, "f1"), (2, p_curve, "precision"), (3, r_curve, "recall")):
            xs, ys = _mean_ys(curves[idx])
            for x, y in zip(xs, ys):
                xf, yf = float(x), float(y)
                if not (math.isfinite(xf) and math.isfinite(yf)):
                    continue
                target.append({"confidence": xf, key: yf})
    except Exception as exc:
        logger.warning("Failed to extract confidence curves: %s", exc)

    return f1_curve, p_curve, r_curve


def _extract_per_class_metrics(results) -> list[dict]:
    """P1-18：提取逐类 AP/P/R，并按漏检数（FN）降序排序。

    输出元素字段：
      classIndex, className, precision, recall, f1, ap50, ap,
      support(实例数), fn_count(漏检), fp_count(超检/误检)
    """
    per_class: list[dict] = []
    try:
        import numpy as np

        names = {}
        if hasattr(results, "names"):
            names = results.names or {}
        box = getattr(results, "box", None)
        if box is None:
            return per_class

        p = np.asarray(box.p, dtype=float).reshape(-1) if len(box.p) else np.zeros(0)
        r = np.asarray(box.r, dtype=float).reshape(-1) if len(box.r) else np.zeros(0)
        f1 = np.asarray(box.f1, dtype=float).reshape(-1) if len(getattr(box, "f1", [])) else np.zeros(0)
        ap50 = np.asarray(box.ap50, dtype=float).reshape(-1) if len(box.ap50) else np.zeros(0)
        ap = np.asarray(box.ap, dtype=float).reshape(-1) if len(box.ap) else np.zeros(0)
        ap_class_index = list(getattr(box, "ap_class_index", []) or [])

        # 每类 GT 实例数（ultralytics DetMetrics.nt_per_class）
        nt_per_class = getattr(results, "nt_per_class", None)
        if nt_per_class is None:
            nt_per_class = getattr(box, "nt_per_class", None)

        for i, cls_idx in enumerate(ap_class_index):
            cls_idx_int = int(cls_idx)
            support = 0
            if nt_per_class is not None:
                try:
                    support = int(np.asarray(nt_per_class).reshape(-1)[cls_idx_int])
                except Exception:
                    support = 0

            precision = float(p[i]) if i < len(p) else 0.0
            recall = float(r[i]) if i < len(r) else 0.0
            f1_val = float(f1[i]) if i < len(f1) else 0.0
            ap50_val = float(ap50[i]) if i < len(ap50) else 0.0
            ap_val = float(ap[i]) if i < len(ap) else 0.0

            # FN = 未检出的 GT 数；FP 由 precision 反推：fp ≈ tp/precision - tp
            fn_count = max(0, int(round(support * (1.0 - recall))))
            tp_count = support - fn_count
            if precision > 1e-9:
                fp_count = max(0, int(round(tp_count / precision - tp_count)))
            else:
                fp_count = max(0, int(round(support * 2)))  # precision=0 时给出保守上界

            per_class.append({
                "classIndex": cls_idx_int,
                "className": str(names.get(cls_idx_int, names.get(str(cls_idx_int), f"class_{cls_idx_int}"))),
                "precision": precision,
                "recall": recall,
                "f1": f1_val,
                "ap50": ap50_val,
                "ap": ap_val,
                "support": support,
                "fn_count": fn_count,
                "fp_count": fp_count,
            })
    except Exception as exc:
        logger.warning("Failed to extract per-class metrics: %s", exc)
        return per_class

    # 每类漏检最多排序（降序），同漏检数时误检多的优先
    per_class.sort(key=lambda x: (x["fn_count"], x["fp_count"]), reverse=True)
    return per_class


def _extract_confusion_matrix(results) -> dict:
    """提取混淆矩阵（保持既有字段名 matrix/names）。"""
    confusion_matrix: dict = {}
    if hasattr(results, "confusion_matrix") and results.confusion_matrix is not None:
        try:
            cm = results.confusion_matrix.matrix
            confusion_matrix = {
                "matrix": cm.tolist() if hasattr(cm, "tolist") else [],
                "names": list(results.names.values()) if hasattr(results, "names") else [],
            }
        except Exception as exc:
            logger.warning("Failed to extract confusion matrix: %s", exc)
    return confusion_matrix


# ============================================================================
# P1-17：阈值推荐 + Go/No-Go
# ============================================================================

def _recommend_threshold(
    f1_curve: list[dict],
    p_curve: list[dict],
    r_curve: list[dict],
    config: dict,
) -> dict:
    """计算推荐 conf 阈值。

    方法：
      - cost_weighted：配置了 miss_cost / false_alarm_cost 时，最小化
        代价 = miss_cost*(1-recall) + false_alarm_cost*(1-precision)
      - max_f1：默认，取 F1 曲线最高点

    返回：
      recommended_conf, method,
      f1_at_threshold, precision_at_threshold, recall_at_threshold,
      expected_miss_rate(漏检率=1-recall), expected_false_alarm_rate(超检率=1-precision)
    """
    result = {
        "recommended_conf": 0.25,
        "method": "max_f1",
        "f1_at_threshold": 0.0,
        "precision_at_threshold": 0.0,
        "recall_at_threshold": 0.0,
        "expected_miss_rate": 1.0,
        "expected_false_alarm_rate": 1.0,
    }
    if not f1_curve:
        return result

    miss_cost = float(config.get("miss_cost", 1.0) or 1.0)
    false_alarm_cost = float(config.get("false_alarm_cost", 1.0) or 1.0)
    use_cost = bool(config.get("use_cost_weighted", False)) or (
        "miss_cost" in config or "false_alarm_cost" in config
    )

    # 将 P/R 曲线对齐到同一置信度轴（均以 confidence 为键）
    def _curve_to_map(curve: list[dict], key: str) -> dict:
        out = {}
        for pt in curve:
            try:
                out[round(float(pt.get("confidence", 0.0)), 4)] = float(pt.get(key, 0.0))
            except (TypeError, ValueError):
                continue
        return out

    p_map = _curve_to_map(p_curve, "precision")
    r_map = _curve_to_map(r_curve, "recall")

    best_conf = result["recommended_conf"]
    best_score = float("-inf")
    best_metrics = (0.0, 0.0, 0.0)

    for pt in f1_curve:
        try:
            conf = float(pt.get("confidence", 0.0))
            f1_val = float(pt.get("f1", 0.0))
        except (TypeError, ValueError):
            continue
        if not (math.isfinite(conf) and math.isfinite(f1_val)):
            continue

        conf_key = round(conf, 4)
        precision = p_map.get(conf_key, 0.0)
        recall = r_map.get(conf_key, 0.0)

        if use_cost:
            score = -(miss_cost * (1.0 - recall) + false_alarm_cost * (1.0 - precision))
            method = "cost_weighted"
        else:
            score = f1_val
            method = "max_f1"

        if score > best_score:
            best_score = score
            best_conf = conf
            best_metrics = (f1_val, precision, recall)
            result["method"] = method

    f1_val, precision, recall = best_metrics
    result["recommended_conf"] = float(best_conf)
    result["f1_at_threshold"] = float(f1_val)
    result["precision_at_threshold"] = float(precision)
    result["recall_at_threshold"] = float(recall)
    result["expected_miss_rate"] = float(max(0.0, 1.0 - recall))
    result["expected_false_alarm_rate"] = float(max(0.0, 1.0 - precision))
    return result


def _build_go_no_go(metrics: dict, config: dict) -> dict:
    """P1-17：Go/No-Go 结论。

    门限：
      - 检测/分类类任务：mAP50 > 0.85
      - 异常任务：AUROC > 0.95
    相对 baseline 回归判定：传入 config.baseline_metrics（如 {"mAP50":0.9}），
    当前指标低于 baseline*(1-baseline_tolerance) 视为回归。
    """
    is_anomaly = "auroc" in metrics and "mAP50" not in metrics
    gate_metric = "AUROC" if is_anomaly else "mAP50"
    gate_threshold = GATE_AUROC if is_anomaly else GATE_MAP50

    if is_anomaly:
        gate_value = float(metrics.get("auroc", metrics.get("image_auroc", 0.0)) or 0.0)
        current_value = gate_value
        metric_key = "auroc"
    else:
        gate_value = float(metrics.get("mAP50", 0.0) or 0.0)
        current_value = gate_value
        metric_key = "mAP50"

    gate_passed = gate_value > gate_threshold

    reasons: list[str] = []
    if gate_passed:
        reasons.append(f"{gate_metric}={gate_value:.4f} 超过门限 {gate_threshold}")
    else:
        reasons.append(f"{gate_metric}={gate_value:.4f} 未达门限 {gate_threshold}")

    # baseline 回归判定
    baseline_metrics = config.get("baseline_metrics") or {}
    regression: bool | None = None
    baseline_value: float | None = None
    if isinstance(baseline_metrics, dict) and metric_key in baseline_metrics:
        try:
            baseline_value = float(baseline_metrics[metric_key])
            tolerance = float(config.get("baseline_tolerance", 0.0) or 0.0)
            if current_value < baseline_value * (1.0 - tolerance) - 1e-9:
                regression = True
                reasons.append(
                    f"相对 baseline 回归：{metric_key} {current_value:.4f} < "
                    f"{baseline_value:.4f}*(1-{tolerance:.3f})"
                )
            else:
                regression = False
                reasons.append(f"相对 baseline 无回归：{metric_key} {current_value:.4f} ≥ {baseline_value:.4f}")
        except (TypeError, ValueError):
            regression = None

    decision = "go" if gate_passed and regression is not True else "no_go"
    if decision == "no_go" and gate_passed and regression is True:
        reasons.append("门限通过但相对 baseline 回归，判定 NO-GO")
    elif decision == "go":
        reasons.append("结论：GO（可部署）")
    else:
        reasons.append("结论：NO-GO（不建议部署）")

    return {
        "decision": decision,
        "gate_metric": gate_metric,
        "gate_threshold": gate_threshold,
        "gate_value": gate_value,
        "gate_passed": gate_passed,
        "regression_vs_baseline": regression,
        "baseline_value": baseline_value,
        "current_value": current_value,
        "reasons": reasons,
    }


async def _run_ultralytics_testing(weight_path: str, data_path: str, config: dict):
    try:
        from ultralytics import YOLO
    except ImportError as exc:
        raise RuntimeError("Ultralytics not installed, cannot run testing") from exc

    # 风险说明：YOLO(pt) 内部经 torch.load 反序列化，路径已在调用方限制于项目内
    model = YOLO(weight_path)

    # 分类任务的 data 参数是 ImageFolder 根目录（含 train/ val/），而非 yaml 文件
    val_data = data_path
    if getattr(model, "task", None) == "classify":
        val_data = os.path.dirname(data_path) if str(data_path).endswith(".yaml") else data_path

    loop = asyncio.get_event_loop()
    results = await loop.run_in_executor(
        None,
        lambda: model.val(
            data=val_data,
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

    def _safe_float(value, default=0.0):
        try:
            if value is None:
                return default
            if hasattr(value, "mean"):
                return float(value.mean())
            return float(value)
        except (TypeError, ValueError):
            return default

    if hasattr(results, "top1"):
        # 分类任务：val 结果无 box 属性，准确率在 top1/top2
        metrics = {
            "top1": _safe_float(results.top1),
            "top2": _safe_float(getattr(results, "top2", 0.0)),
            # 主指标位别名：UI 主指标卡读 mAP50，分类任务展示 Top-1 值
            "mAP50": _safe_float(results.top1),
        }

    if hasattr(results, "box") and results.box:
        metrics = {
            "mAP50": _safe_float(results.box.map50),
            "mAP50-95": _safe_float(results.box.map),
            "precision": _safe_float(results.box.mp),
            "recall": _safe_float(results.box.mr),
            "f1": _safe_float(getattr(results.box, "f1", 0.0)),
        }

    # P1-16：真实 PR 曲线与置信度曲线
    pr_curve = _extract_pr_curve_from_results(results)
    f1_curve, p_curve, r_curve = _extract_confidence_curves(results)
    metrics["f1_curve"] = f1_curve
    metrics["precision_curve"] = p_curve
    metrics["recall_curve"] = r_curve
    metrics["pr_curve_points"] = len(pr_curve)

    # P1-18：逐类指标（按漏检降序）
    metrics["per_class"] = _extract_per_class_metrics(results)

    # P1-17：阈值推荐 + Go/No-Go
    metrics["threshold_recommendation"] = _recommend_threshold(f1_curve, p_curve, r_curve, config)
    metrics["go_no_go"] = _build_go_no_go(metrics, config)

    confusion_matrix = _extract_confusion_matrix(results)

    return metrics, confusion_matrix, pr_curve


# ============================================================================
# Anomalib 侧（P1-16：F1-Conf / 阈值曲线）
# ============================================================================

def _collect_anomaly_scores(model, datamodule) -> tuple[list[float], list[int]]:
    """收集测试集异常分数与标签（0=正常，1=异常），用于曲线扫描。"""
    scores: list[float] = []
    labels: list[int] = []
    try:
        import torch

        loader = datamodule.test_dataloader()
        was_training = model.training
        model.eval()
        with torch.no_grad():
            for batch in loader:
                if isinstance(batch, dict):
                    inputs = batch.get("image")
                    gt = batch.get("gt_label", batch.get("label"))
                else:
                    continue
                if inputs is None:
                    continue
                pred = model(inputs)
                # anomalib 各模型输出字段不统一：pred_score / anomaly_score
                if isinstance(pred, dict):
                    batch_scores = pred.get("pred_score", pred.get("anomaly_score"))
                else:
                    batch_scores = getattr(pred, "pred_score", None)
                if batch_scores is None:
                    continue
                if hasattr(batch_scores, "detach"):
                    batch_scores = batch_scores.detach().float().cpu().reshape(-1)
                else:
                    batch_scores = torch.tensor(batch_scores, dtype=torch.float32).reshape(-1)

                if gt is None:
                    continue
                if hasattr(gt, "detach"):
                    gt = gt.detach().int().cpu().reshape(-1)
                else:
                    gt = torch.tensor(gt, dtype=torch.int64).reshape(-1)

                for s, g in zip(batch_scores.tolist(), gt.tolist()):
                    scores.append(float(s))
                    labels.append(int(g))
        if was_training:
            model.train()
    except Exception as exc:
        logger.warning("Failed to collect anomaly scores for curves: %s", exc)
    return scores, labels


def _sweep_threshold_curves(scores: list[float], labels: list[int]) -> tuple[list[dict], list[dict]]:
    """扫描阈值生成 F1-Confidence 与 PR 曲线（P1-16）。

    返回 (f1_curve, pr_curve)：
      f1_curve: [{"confidence": thr, "f1": ...}]
      pr_curve: [{"recall": ..., "precision": ..., "confidence": thr, "f1": ...}]
    """
    f1_curve: list[dict] = []
    pr_curve: list[dict] = []
    if not scores or not labels:
        return f1_curve, pr_curve

    import numpy as np

    scores_arr = np.asarray(scores, dtype=float)
    labels_arr = np.asarray(labels, dtype=int)
    if scores_arr.size == 0:
        return f1_curve, pr_curve

    lo = float(np.min(scores_arr))
    hi = float(np.max(scores_arr))
    if not math.isfinite(lo) or not math.isfinite(hi) or hi <= lo:
        hi = lo + 1e-6

    thresholds = np.linspace(lo, hi, CURVE_POINTS)
    pos_total = max(1, int((labels_arr == 1).sum()))
    neg_total = max(1, int((labels_arr == 0).sum()))

    for thr in thresholds:
        pred_pos = scores_arr >= thr
        tp = int(((pred_pos) & (labels_arr == 1)).sum())
        fp = int(((pred_pos) & (labels_arr == 0)).sum())

        precision = tp / (tp + fp) if (tp + fp) > 0 else 1.0
        recall = tp / pos_total
        f1_val = (2 * precision * recall / (precision + recall)) if (precision + recall) > 0 else 0.0

        thr_f = float(thr)
        f1_curve.append({"confidence": thr_f, "f1": float(f1_val)})
        pr_curve.append({
            "recall": float(recall),
            "precision": float(precision),
            "confidence": thr_f,
            "f1": float(f1_val),
            "fp_rate": fp / neg_total,
        })

    return f1_curve, pr_curve


def _anomaly_threshold_recommendation(
    f1_curve: list[dict],
    pr_curve: list[dict],
    config: dict,
) -> dict:
    """异常任务的阈值推荐：默认 max F1，支持漏检/超检代价加权。"""
    result = {
        "recommended_conf": 0.5,
        "method": "max_f1",
        "f1_at_threshold": 0.0,
        "precision_at_threshold": 0.0,
        "recall_at_threshold": 0.0,
        "expected_miss_rate": 1.0,
        "expected_false_alarm_rate": 1.0,
    }
    if not f1_curve and not pr_curve:
        return result

    pr_by_conf = {round(float(p.get("confidence", 0.0)), 6): p for p in pr_curve}
    miss_cost = float(config.get("miss_cost", 1.0) or 1.0)
    false_alarm_cost = float(config.get("false_alarm_cost", 1.0) or 1.0)
    use_cost = bool(config.get("use_cost_weighted", False)) or (
        "miss_cost" in config or "false_alarm_cost" in config
    )

    best_thr = result["recommended_conf"]
    best_score = float("-inf")
    best_bundle = (0.0, 0.0, 0.0)

    source = f1_curve if f1_curve else [
        {"confidence": p.get("confidence", 0.0), "f1": p.get("f1", 0.0)} for p in pr_curve
    ]
    for pt in source:
        try:
            thr = float(pt.get("confidence", 0.0))
            f1_val = float(pt.get("f1", 0.0))
        except (TypeError, ValueError):
            continue
        pr_pt = pr_by_conf.get(round(thr, 6), {})
        precision = float(pr_pt.get("precision", 0.0) or 0.0)
        recall = float(pr_pt.get("recall", 0.0) or 0.0)
        if use_cost:
            score = -(miss_cost * (1.0 - recall) + false_alarm_cost * (1.0 - precision))
            result["method"] = "cost_weighted"
        else:
            score = f1_val
            result["method"] = "max_f1"
        if score > best_score:
            best_score = score
            best_thr = thr
            best_bundle = (f1_val, precision, recall)

    f1_val, precision, recall = best_bundle
    result["recommended_conf"] = float(best_thr)
    result["f1_at_threshold"] = float(f1_val)
    result["precision_at_threshold"] = float(precision)
    result["recall_at_threshold"] = float(recall)
    result["expected_miss_rate"] = float(max(0.0, 1.0 - recall))
    result["expected_false_alarm_rate"] = float(
        max(0.0, float(pr_by_conf.get(round(best_thr, 6), {}).get("fp_rate", 1.0 - precision)))
    )
    return result


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
    # 安全加载：优先 weights_only=True，anomalib ckpt 回退时校验扩展名/路径并告警
    from ..tools.safe_loading import safe_load_weight
    checkpoint = safe_load_weight(weight_path, map_location="cpu")
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
        # anomalib 2.5：移除 image_size；val 从训练集切分避免小测试集除零
        val_split_mode="from_train",
        val_split_ratio=0.1,
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

    # P1-16：采集异常分数并扫描 F1-Conf / 阈值曲线
    f1_curve: list[dict] = []
    scores: list[float] = []
    labels: list[int] = []
    try:
        scores, labels = await loop.run_in_executor(
            None, lambda: _collect_anomaly_scores(model, datamodule)
        )
        f1_curve, pr_curve = _sweep_threshold_curves(scores, labels)
        metrics["f1_curve"] = f1_curve
        metrics["precision_curve"] = [
            {"confidence": p["confidence"], "precision": p["precision"]} for p in pr_curve
        ]
        metrics["recall_curve"] = [
            {"confidence": p["confidence"], "recall": p["recall"]} for p in pr_curve
        ]
        metrics["pr_curve_points"] = len(pr_curve)
    except Exception as exc:
        logger.warning("Anomaly curve extraction failed: %s", exc)
        metrics["f1_curve"] = []
        metrics["precision_curve"] = []
        metrics["recall_curve"] = []
        metrics["pr_curve_points"] = 0

    # P1-17：阈值推荐 + Go/No-Go（AUROC>0.95）
    metrics["threshold_recommendation"] = _anomaly_threshold_recommendation(f1_curve, pr_curve, config)
    metrics["go_no_go"] = _build_go_no_go(metrics, config)

    # P1-18：异常任务逐类（正常/异常）概要，按漏检降序
    metrics["per_class"] = _anomaly_per_class_summary(metrics, scores, labels)

    return metrics, confusion_matrix, pr_curve


def _anomaly_per_class_summary(metrics: dict, scores: list, labels: list) -> list[dict]:
    """异常任务逐类概要：正常/异常两类的召回与漏检（按漏检降序）。"""
    per_class: list[dict] = []
    try:
        import numpy as np

        labels_arr = np.asarray(labels, dtype=int) if labels else np.zeros(0, dtype=int)
        scores_arr = np.asarray(scores, dtype=float) if scores else np.zeros(0, dtype=float)

        thr = float(metrics.get("threshold_recommendation", {}).get("recommended_conf", 0.5) or 0.5)
        for cls_idx, cls_name in ((1, "defective"), (0, "good")):
            mask = labels_arr == cls_idx
            support = int(mask.sum())
            if support == 0:
                fn_count = 0
                recall = 0.0
            elif cls_idx == 1:
                pred_pos = scores_arr[mask] >= thr
                fn_count = int((~pred_pos).sum())
                recall = float(pred_pos.sum()) / max(1, support)
            else:
                # 正常类的「漏检」= 被误判为异常的数量
                pred_pos = scores_arr[mask] >= thr
                fn_count = int(pred_pos.sum())
                recall = float((~pred_pos).sum()) / max(1, support)

            per_class.append({
                "classIndex": cls_idx,
                "className": cls_name,
                "precision": float(metrics.get("f1", 0.0)),
                "recall": recall,
                "f1": float(metrics.get("f1", 0.0)),
                "ap50": float(metrics.get("auroc", 0.0)),
                "ap": float(metrics.get("auroc", 0.0)),
                "support": support,
                "fn_count": fn_count,
                "fp_count": 0,
            })
    except Exception as exc:
        logger.warning("Failed to build anomaly per-class summary: %s", exc)

    per_class.sort(key=lambda x: (x["fn_count"], x["fp_count"]), reverse=True)
    return per_class
