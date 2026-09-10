"""
推理命令处理器

支持单张图片推理和批量推理，可选生成可视化预览图（基于 supervision 渲染）
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
        save_annotated: 是否生成可视化预览图 (默认False)
        annotated_dir: 可视化图保存目录 (默认 source 同级 .labeltorch_annotated)
    """
    weight_path = payload.get("weight_path", "")
    source = payload.get("source", "")
    conf = payload.get("conf", 0.25)
    iou = payload.get("iou", 0.45)
    imgsz = payload.get("imgsz", 640)
    device = payload.get("device", "auto")
    save = payload.get("save", False)
    save_annotated = payload.get("save_annotated", False)
    annotated_dir = payload.get("annotated_dir", "")

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
        import cv2
        import numpy as np

        if device and device != "auto" and device != "cpu":
            if not torch.cuda.is_available():
                logger.warning(f"CUDA is not available. Falling back to CPU for inference (requested device was: {device}).")
                device = "cpu"

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

        # 准备可视化渲染器（save_annotated 模式）
        box_annotator = None
        label_annotator = None
        color_palette = None
        if save_annotated:
            # 使用 supervision 默认 12 色调色板，保证类别颜色稳定
            color_palette = sv.ColorPalette.default()
            box_annotator = sv.BoxAnnotator(color=color_palette, thickness=2)
            label_annotator = sv.LabelAnnotator(color=color_palette, text_thickness=1, text_scale=0.5)
            # 默认输出目录：source 同级 .labeltorch_annotated
            if not annotated_dir:
                if os.path.isdir(source):
                    annotated_dir = os.path.join(source, ".labeltorch_annotated")
                else:
                    annotated_dir = os.path.join(os.path.dirname(source), ".labeltorch_annotated")
            os.makedirs(annotated_dir, exist_ok=True)

        predictions = []
        for result in results:
            pred = {
                "path": result.path,
                "boxes": [],
                "annotated_path": "",
            }

            # 加载原图用于可视化渲染
            frame = None
            if save_annotated:
                img_path = result.path
                if os.path.isfile(img_path):
                    frame = cv2.imread(img_path)
                if frame is None:
                    # 兜底：从 result.boxes 渲染空白图
                    h = int(result.orig_h) if hasattr(result, "orig_h") and result.orig_h else 640
                    w = int(result.orig_w) if hasattr(result, "orig_w") and result.orig_w else 640
                    frame = np.zeros((h, w, 3), dtype=np.uint8)

            if result.boxes is not None and len(result.boxes) > 0:
                detections = sv.Detections.from_ultralytics(result)
                for xyxy, class_id, confidence in zip(detections.xyxy, detections.class_id, detections.confidence):
                    box_info = {
                        "class_id": int(class_id),
                        "class_name": result.names[int(class_id)],
                        "confidence": float(confidence),
                        "xyxy": xyxy.tolist(),
                    }
                    pred["boxes"].append(box_info)

                # 渲染可视化预览图
                if save_annotated and frame is not None:
                    labels = [
                        f"{result.names[int(cid)]} {conf:.2f}"
                        for cid, conf in zip(detections.class_id, detections.confidence)
                    ]
                    frame = box_annotator.annotate(scene=frame, detections=detections)
                    frame = label_annotator.annotate(scene=frame, detections=detections, labels=labels)

            # 保存可视化图（即使无检测框也保存原图副本，便于对比）
            if save_annotated and frame is not None:
                base = os.path.splitext(os.path.basename(result.path))[0]
                out_path = os.path.join(annotated_dir, f"{base}.jpg")
                cv2.imwrite(out_path, frame)
                pred["annotated_path"] = out_path

            predictions.append(pred)

        total_boxes = sum(len(p["boxes"]) for p in predictions)

        return {
            "status": "succeeded",
            "predictions": predictions,
            "total_images": len(predictions),
            "total_boxes": total_boxes,
            "annotated_dir": annotated_dir if save_annotated else "",
        }

    except ImportError:
        return {"status": "failed", "error": "Ultralytics or Supervision is not installed"}
    except Exception as e:
        logger.error(f"Inference failed: {e}")
        return {"status": "failed", "error": str(e)}


async def handle_run_video(payload: dict) -> dict:
    """
    视频流推理（P2-6）

    payload:
        weight_path: 模型权重文件路径
        video_path: 视频文件路径
        output_path: 标注后视频输出路径
        conf: 置信度阈值 (默认0.25)
        iou: IoU阈值 (默认0.45)
        imgsz: 推理图片尺寸 (默认640)
        device: 推理设备 (cpu/0/auto)
    """
    weight_path = payload.get("weight_path", "")
    video_path = payload.get("video_path", "")
    output_path = payload.get("output_path", "")
    conf = payload.get("conf", 0.25)
    iou = payload.get("iou", 0.45)
    imgsz = payload.get("imgsz", 640)
    device = payload.get("device", "auto")

    if not weight_path or not os.path.isfile(weight_path):
        return {"status": "failed", "error": "Invalid weight_path"}
    if not video_path or not os.path.isfile(video_path):
        return {"status": "failed", "error": "Invalid video_path"}
    if not output_path:
        return {"status": "failed", "error": "Missing output_path"}

    try:
        from ultralytics import YOLO
        import torch
        import supervision as sv

        if device and device != "auto" and device != "cpu":
            if not torch.cuda.is_available():
                logger.warning("CUDA unavailable, falling back to CPU for video inference.")
                device = "cpu"

        model = YOLO(weight_path)
        color_palette = sv.ColorPalette.default()
        box_annotator = sv.BoxAnnotator(color=color_palette, thickness=2)
        label_annotator = sv.LabelAnnotator(color=color_palette, text_thickness=1, text_scale=0.5)

        def _process_video():
            video_info = sv.VideoInfo.from_video_path(video_path)
            total_frames = 0
            total_detections = 0
            with sv.VideoSink(output_path, video_info) as sink:
                for frame in sv.get_video_frames_generator(video_path):
                    results = model.predict(source=frame, conf=conf, iou=iou, imgsz=imgsz,
                                            device=device if device != "auto" else None,
                                            verbose=False)
                    for result in results:
                        if result.boxes is not None and len(result.boxes) > 0:
                            detections = sv.Detections.from_ultralytics(result)
                            labels = [
                                f"{result.names[int(cid)]} {cf:.2f}"
                                for cid, cf in zip(detections.class_id, detections.confidence)
                            ]
                            frame = box_annotator.annotate(scene=frame, detections=detections)
                            frame = label_annotator.annotate(scene=frame, detections=detections, labels=labels)
                            total_detections += len(detections)
                        sink.write_frame(frame)
                        total_frames += 1
            return total_frames, total_detections

        loop = asyncio.get_event_loop()
        total_frames, total_detections = await loop.run_in_executor(None, _process_video)

        return {
            "status": "succeeded",
            "output_path": output_path,
            "total_frames": total_frames,
            "total_detections": total_detections,
        }
    except ImportError:
        return {"status": "failed", "error": "Ultralytics or Supervision is not installed"}
    except Exception as e:
        logger.error(f"Video inference failed: {e}")
        return {"status": "failed", "error": str(e)}
