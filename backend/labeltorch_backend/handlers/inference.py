"""
推理命令处理器

支持单张图片推理和批量推理
"""
import asyncio
import logging
import os

logger = logging.getLogger(__name__)


async def handle_run(payload: dict) -> dict:
    """
    执行推理

    payload:
        weight_path: 模型权重文件路径
        source: 图片路径或目录
        conf: 置信度阈值 (默认0.25)
        iou: IoU阈值 (默认0.45)
        imgsz: 推理图片尺寸 (默认640)
        device: 推理设备 (cpu/0/auto)
        save: 是否保存结果 (默认False)
    """
    weight_path = payload.get("weight_path", "")
    source = payload.get("source", "")
    conf = payload.get("conf", 0.25)
    iou = payload.get("iou", 0.45)
    imgsz = payload.get("imgsz", 640)
    device = payload.get("device", "auto")
    save = payload.get("save", False)

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
                conf=conf,
                iou=iou,
                imgsz=imgsz,
                device=device if device != "auto" else None,
                save=save,
                verbose=False,
            )
        )

        predictions = []
        for result in results:
            pred = {
                "path": result.path,
                "boxes": [],
            }

            if result.boxes is not None:
                for box in result.boxes:
                    box_info = {
                        "class_id": int(box.cls[0]),
                        "class_name": result.names[int(box.cls[0])],
                        "confidence": float(box.conf[0]),
                        "xyxy": box.xyxy[0].tolist() if box.xyxy is not None and len(box.xyxy) > 0 else [],
                    }
                    pred["boxes"].append(box_info)

            predictions.append(pred)

        total_boxes = sum(len(p["boxes"]) for p in predictions)

        return {
            "status": "succeeded",
            "predictions": predictions,
            "total_images": len(predictions),
            "total_boxes": total_boxes,
        }

    except ImportError:
        return {"status": "failed", "error": "Ultralytics is not installed"}
    except Exception as e:
        logger.error(f"Inference failed: {e}")
        return {"status": "failed", "error": str(e)}
