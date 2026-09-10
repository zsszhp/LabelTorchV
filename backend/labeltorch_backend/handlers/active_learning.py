"""
主动学习命令处理器

支持低置信样本回流、漏检/误检队列管理、难例优先级排序
"""
import asyncio
import logging
import os
from datetime import datetime

logger = logging.getLogger(__name__)


async def handle_collect_low_conf(payload: dict) -> dict:
    """
    收集低置信度样本（P1-3 增强：可选 GT 对比识别真 FP/FN）

    payload:
        weight_path: 模型权重文件路径
        source: 图片路径或目录
        conf_threshold: 置信度阈值 (默认0.3)
        iou: IoU阈值 (默认0.45)
        imgsz: 推理图片尺寸 (默认640)
        device: 推理设备 (cpu/0/auto)
        label_dir: GT 标签目录（可选，提供后启用难例挖掘，按 IoU 对比识别 FP/FN）
        iou_match_threshold: GT 匹配 IoU 阈值（默认0.5）
    """
    weight_path = payload.get("weight_path", "")
    source = payload.get("source", "")
    conf_threshold = payload.get("conf_threshold", 0.3)
    iou = payload.get("iou", 0.45)
    imgsz = payload.get("imgsz", 640)
    device = payload.get("device", "auto")
    label_dir = payload.get("label_dir", "")
    iou_match_threshold = payload.get("iou_match_threshold", 0.5)

    if not weight_path:
        return {"status": "failed", "error": "Missing weight_path"}
    if not source:
        return {"status": "failed", "error": "Missing source"}
    if not os.path.isfile(weight_path):
        return {"status": "failed", "error": f"Weight file not found: {weight_path}"}

    try:
        from ultralytics import YOLO
        import torch
        import supervision as sv

        if device and device != "auto" and device != "cpu":
            if not torch.cuda.is_available():
                logger.warning(f"CUDA is not available. Falling back to CPU for active learning (requested device was: {device}).")
                device = "cpu"

        model = YOLO(weight_path)

        loop = asyncio.get_event_loop()
        results = await loop.run_in_executor(
            None,
            lambda: model.predict(
                source=source,
                conf=conf_threshold,
                iou=iou,
                imgsz=imgsz,
                device=device if device != "auto" else None,
                save=False,
                verbose=False,
            )
        )

        # GT 对比模式标志
        has_gt = bool(label_dir) and os.path.isdir(label_dir)

        low_conf_samples = []
        false_positive_samples = []
        false_negative_samples = []

        for result in results:
            pred_boxes = []
            if result.boxes is not None and len(result.boxes) > 0:
                detections = sv.Detections.from_ultralytics(result)
                for xyxy, class_id, confidence in zip(detections.xyxy, detections.class_id, detections.confidence):
                    pred_boxes.append({
                        "class_id": int(class_id),
                        "confidence": float(confidence),
                        "xyxy": xyxy.tolist(),
                    })

            # GT 对比模式：加载该图的 GT 标签并按 IoU 匹配
            if has_gt:
                gt_boxes = _load_yolo_gt(label_dir, result.path)
                fp_boxes, fn_boxes, matched_pred_indices = _classify_predictions(
                    pred_boxes, gt_boxes, iou_match_threshold
                )
                # 误检：预测框未匹配到 GT
                if fp_boxes:
                    false_positive_samples.append({
                        "path": result.path,
                        "boxes": fp_boxes,
                        "max_confidence": max(b["confidence"] for b in fp_boxes),
                        "box_count": len(fp_boxes),
                        "error_type": "false_positive",
                    })
                # 漏检：GT 框未匹配到预测
                if fn_boxes:
                    false_negative_samples.append({
                        "path": result.path,
                        "boxes": fn_boxes,
                        "max_confidence": 0.0,
                        "box_count": len(fn_boxes),
                        "error_type": "false_negative",
                    })
                # 低置信且匹配 GT 的预测框（定位可能不准）
                low_boxes = [
                    pred_boxes[i] for i in range(len(pred_boxes))
                    if i in matched_pred_indices
                    and conf_threshold <= pred_boxes[i]["confidence"] < conf_threshold + 0.2
                ]
                if low_boxes:
                    low_conf_samples.append({
                        "path": result.path,
                        "boxes": low_boxes,
                        "max_confidence": max(b["confidence"] for b in low_boxes),
                        "box_count": len(low_boxes),
                        "error_type": "low_confidence",
                    })
            else:
                # 无 GT 模式：仅按置信度过滤（原始逻辑）
                low_boxes = [
                    b for b in pred_boxes
                    if conf_threshold <= b["confidence"] < conf_threshold + 0.2
                ]
                if low_boxes:
                    low_conf_samples.append({
                        "path": result.path,
                        "boxes": low_boxes,
                        "max_confidence": max(b["confidence"] for b in low_boxes),
                        "box_count": len(low_boxes),
                        "error_type": "low_confidence",
                    })

        # 按最大置信度排序（越低越优先）
        low_conf_samples.sort(key=lambda x: x["max_confidence"])
        false_positive_samples.sort(key=lambda x: x["max_confidence"])
        false_negative_samples.sort(key=lambda x: x["box_count"], reverse=True)

        response = {
            "status": "succeeded",
            "samples": low_conf_samples,
            "total_samples": len(low_conf_samples),
            "queue_type": "low-confidence",
            "collected_at": datetime.now().isoformat(),
        }
        if has_gt:
            response["false_positives"] = false_positive_samples
            response["false_negatives"] = false_negative_samples
            response["total_false_positives"] = len(false_positive_samples)
            response["total_false_negatives"] = len(false_negative_samples)
            response["gt_compared"] = True
        return response

    except ImportError:
        return {"status": "failed", "error": "Ultralytics is not installed"}
    except Exception as e:
        logger.error(f"Low confidence collection failed: {e}")
        return {"status": "failed", "error": str(e)}


def _load_yolo_gt(label_dir: str, image_path: str):
    """加载图片对应的 YOLO GT 标签（xyxy 像素格式）

    读取图片尺寸将归一化 xywh 转换为像素 xyxy，
    无图片或读取失败时回退到归一化坐标（调用方按归一化空间匹配）。
    """
    import os
    base = os.path.splitext(os.path.basename(image_path))[0]
    label_path = os.path.join(label_dir, base + ".txt")
    boxes = []
    if not os.path.isfile(label_path):
        return boxes

    # 读取图片尺寸用于归一化->像素转换
    img_w, img_h = 0, 0
    try:
        from PIL import Image
        with Image.open(image_path) as img:
            img_w, img_h = img.size
    except Exception as e:
        logger.warning(f"Failed to read image size {image_path}: {e}")

    try:
        with open(label_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) >= 5:
                    cid = int(parts[0])
                    cx, cy, w, h = float(parts[1]), float(parts[2]), float(parts[3]), float(parts[4])
                    if img_w > 0 and img_h > 0:
                        # 转换为像素 xyxy
                        x1 = (cx - w / 2.0) * img_w
                        y1 = (cy - h / 2.0) * img_h
                        x2 = (cx + w / 2.0) * img_w
                        y2 = (cy + h / 2.0) * img_h
                        boxes.append({
                            "class_id": cid,
                            "confidence": 1.0,
                            "xyxy": [x1, y1, x2, y2],
                        })
                    else:
                        # 回退：归一化 xywh
                        boxes.append({
                            "class_id": cid,
                            "confidence": 1.0,
                            "xywhn": [cx, cy, w, h],
                        })
    except Exception as e:
        logger.warning(f"Failed to load GT {label_path}: {e}")
    return boxes


def _compute_iou(box_a, box_b):
    """计算两个 xyxy 框的 IoU（像素空间）"""
    x1 = max(box_a[0], box_b[0])
    y1 = max(box_a[1], box_b[1])
    x2 = min(box_a[2], box_b[2])
    y2 = min(box_a[3], box_b[3])
    inter_w = max(0.0, x2 - x1)
    inter_h = max(0.0, y2 - y1)
    inter_area = inter_w * inter_h
    area_a = (box_a[2] - box_a[0]) * (box_a[3] - box_a[1])
    area_b = (box_b[2] - box_b[0]) * (box_b[3] - box_b[1])
    union = area_a + area_b - inter_area
    if union <= 0:
        return 0.0
    return inter_area / union


def _classify_predictions(pred_boxes, gt_boxes, iou_threshold):
    """
    按 IoU 匹配预测与 GT，分类 FP/FN/匹配

    返回: (false_positive_boxes, false_negative_boxes, matched_pred_indices)
    匹配规则：同类 + IoU >= iou_threshold，贪心匹配（IoU 降序）。
    pred_boxes 和 gt_boxes 均使用像素 xyxy 格式（_load_yolo_gt 已转换）。
    当 gt_boxes 为归一化 xywhn 格式（无图片尺寸回退场景）时，
    退化为同类存在即视为匹配。
    """
    matched_pred = set()
    matched_gt = set()

    # 判断 GT 是否已转换为像素 xyxy 格式
    gt_has_xyxy = bool(gt_boxes) and "xyxy" in gt_boxes[0]
    if not gt_has_xyxy:
        # 回退场景：GT 为归一化 xywhn，按同类存在即匹配
        for pi, p in enumerate(pred_boxes):
            p_cid = p["class_id"]
            for gi, g in enumerate(gt_boxes):
                if gi in matched_gt:
                    continue
                if g["class_id"] == p_cid:
                    matched_pred.add(pi)
                    matched_gt.add(gi)
                    break
    else:
        # 标准场景：基于 IoU 贪心匹配
        iou_pairs = []
        for pi, p in enumerate(pred_boxes):
            for gi, g in enumerate(gt_boxes):
                if g["class_id"] != p["class_id"]:
                    continue
                iou = _compute_iou(p["xyxy"], g["xyxy"])
                if iou >= iou_threshold:
                    iou_pairs.append((iou, pi, gi))
        # 按 IoU 降序贪心匹配
        iou_pairs.sort(key=lambda x: x[0], reverse=True)
        for iou, pi, gi in iou_pairs:
            if pi in matched_pred or gi in matched_gt:
                continue
            matched_pred.add(pi)
            matched_gt.add(gi)

    fp_boxes = [pred_boxes[i] for i in range(len(pred_boxes)) if i not in matched_pred]
    fn_boxes = [gt_boxes[i] for i in range(len(gt_boxes)) if i not in matched_gt]
    return fp_boxes, fn_boxes, matched_pred


async def handle_prioritize_queue(payload: dict) -> dict:
    """
    对主动学习队列进行优先级排序

    payload:
        queue_type: 队列类型 (low-confidence/false-positive/false-negative/hard-case)
        samples: 样本列表
        class_weights: 类别权重映射 {class_id: weight}
        strategy: 排序策略 (default/confidence/class-priority)
    """
    queue_type = payload.get("queue_type", "low-confidence")
    samples = payload.get("samples", [])
    class_weights = payload.get("class_weights", {})
    strategy = payload.get("strategy", "default")

    if not samples:
        return {"status": "succeeded", "sorted_samples": [], "total": 0}

    try:
        sorted_samples = []
        for sample in samples:
            priority_score = 0.0

            if strategy == "confidence":
                # 置信度越低优先级越高
                max_conf = sample.get("max_confidence", 1.0)
                priority_score = 1.0 - max_conf

            elif strategy == "class-priority":
                # 根据类别权重计算优先级
                boxes = sample.get("boxes", [])
                if boxes:
                    weights = [
                        class_weights.get(str(box.get("class_id", 0)), 1.0)
                        for box in boxes
                    ]
                    priority_score = sum(weights) / len(weights)
                else:
                    priority_score = 1.0

            else:  # default: 综合策略
                max_conf = sample.get("max_confidence", 1.0)
                box_count = sample.get("box_count", 0)

                # 基础分数：置信度反向
                conf_score = 1.0 - max_conf

                # 框数量加成：框越多越复杂
                box_score = min(box_count / 10.0, 1.0) * 0.3

                # 类别权重
                boxes = sample.get("boxes", [])
                if boxes and class_weights:
                    weights = [
                        class_weights.get(str(box.get("class_id", 0)), 1.0)
                        for box in boxes
                    ]
                    class_score = (sum(weights) / len(weights) - 1.0) * 0.2
                else:
                    class_score = 0.0

                priority_score = conf_score * 0.5 + box_score + class_score + 0.2

            sorted_samples.append({
                **sample,
                "priority_score": round(priority_score, 4),
            })

        # 按优先级降序排序
        sorted_samples.sort(key=lambda x: x["priority_score"], reverse=True)

        return {
            "status": "succeeded",
            "sorted_samples": sorted_samples,
            "total": len(sorted_samples),
            "queue_type": queue_type,
            "strategy": strategy,
        }

    except Exception as e:
        logger.error(f"Queue prioritization failed: {e}")
        return {"status": "failed", "error": str(e)}


async def handle_queue_stats(payload: dict) -> dict:
    """
    获取主动学习队列统计信息

    payload:
        queue_type: 队列类型
        samples: 样本列表
    """
    queue_type = payload.get("queue_type", "all")
    samples = payload.get("samples", [])

    stats = {
        "total_samples": len(samples),
        "queue_type": queue_type,
    }

    if samples:
        # 类别分布
        class_distribution = {}
        confidence_stats = {
            "min": 1.0,
            "max": 0.0,
            "avg": 0.0,
        }
        total_conf = 0.0
        total_boxes = 0

        for sample in samples:
            boxes = sample.get("boxes", [])
            total_boxes += len(boxes)

            for box in boxes:
                class_id = str(box.get("class_id", 0))
                class_distribution[class_id] = class_distribution.get(class_id, 0) + 1

                conf = box.get("confidence", 0.0)
                total_conf += conf
                confidence_stats["min"] = min(confidence_stats["min"], conf)
                confidence_stats["max"] = max(confidence_stats["max"], conf)

        if total_boxes > 0:
            confidence_stats["avg"] = round(total_conf / total_boxes, 4)

        stats["class_distribution"] = class_distribution
        stats["confidence_stats"] = confidence_stats
        stats["total_boxes"] = total_boxes
        stats["avg_boxes_per_sample"] = round(total_boxes / len(samples), 2)

    return {
        "status": "succeeded",
        "stats": stats,
    }
