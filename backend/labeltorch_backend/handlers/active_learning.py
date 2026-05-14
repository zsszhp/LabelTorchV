"""
主动学习命令处理器

支持低置信样本回流、漏检/误检队列管理、难例优先级排序
"""
import asyncio
import logging
import os
import json
from datetime import datetime

logger = logging.getLogger(__name__)


async def handle_collect_low_conf(payload: dict) -> dict:
    """
    收集低置信度样本

    payload:
        weight_path: 模型权重文件路径
        source: 图片路径或目录
        conf_threshold: 置信度阈值 (默认0.3)
        iou: IoU阈值 (默认0.45)
        imgsz: 推理图片尺寸 (默认640)
        device: 推理设备 (cpu/0/auto)
    """
    weight_path = payload.get("weight_path", "")
    source = payload.get("source", "")
    conf_threshold = payload.get("conf_threshold", 0.3)
    iou = payload.get("iou", 0.45)
    imgsz = payload.get("imgsz", 640)
    device = payload.get("device", "auto")

    if not weight_path:
        return {"status": "failed", "error": "Missing weight_path"}
    if not source:
        return {"status": "failed", "error": "Missing source"}
    if not os.path.isfile(weight_path):
        return {"status": "failed", "error": f"Weight file not found: {weight_path}"}

    try:
        from ultralytics import YOLO

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

        low_conf_samples = []
        for result in results:
            sample_boxes = []
            if result.boxes is not None:
                for box in result.boxes:
                    confidence = float(box.conf[0])
                    if confidence < conf_threshold + 0.2:  # 收集阈值附近的样本
                        sample_boxes.append({
                            "class_id": int(box.cls[0]),
                            "confidence": confidence,
                            "xyxy": box.xyxy[0].tolist() if box.xyxy is not None else [],
                        })

            if sample_boxes:
                low_conf_samples.append({
                    "path": result.path,
                    "boxes": sample_boxes,
                    "max_confidence": max(b["confidence"] for b in sample_boxes),
                    "box_count": len(sample_boxes),
                })

        # 按最大置信度排序（越低越优先）
        low_conf_samples.sort(key=lambda x: x["max_confidence"])

        return {
            "status": "succeeded",
            "samples": low_conf_samples,
            "total_samples": len(low_conf_samples),
            "queue_type": "low-confidence",
            "collected_at": datetime.now().isoformat(),
        }

    except ImportError:
        return {"status": "failed", "error": "Ultralytics is not installed"}
    except Exception as e:
        logger.error(f"Low confidence collection failed: {e}")
        return {"status": "failed", "error": str(e)}


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
