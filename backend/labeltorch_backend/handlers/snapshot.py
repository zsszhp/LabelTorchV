"""
快照预览图处理器（P0-2）

基于 supervision.DetectionDataset 加载快照目录，采样若干图片，
用 BoxAnnotator 渲染标注框，合成网格预览图保存为 preview.jpg
"""
import asyncio
import logging
import os

logger = logging.getLogger(__name__)


async def handle_preview(payload: dict) -> dict:
    """
    生成快照预览图

    payload:
        snapshot_dir: 快照物理目录（包含 data.yaml + images/ + labels/）
        data_yaml_path: data.yaml 路径（若为空则用 snapshot_dir/data.yaml）
        max_samples: 采样图片数量（默认 9）
        grid_cols: 网格列数（默认 3）
        thumb_size: 单张缩略图边长（默认 320，等比缩放）
    """
    snapshot_dir = payload.get("snapshot_dir", "")
    data_yaml_path = payload.get("data_yaml_path", "") or os.path.join(snapshot_dir, "data.yaml")
    max_samples = int(payload.get("max_samples", 9))
    grid_cols = int(payload.get("grid_cols", 3))
    thumb_size = int(payload.get("thumb_size", 320))

    if not snapshot_dir or not os.path.isdir(snapshot_dir):
        return {"status": "failed", "error": f"Invalid snapshot_dir: {snapshot_dir}"}
    if not os.path.isfile(data_yaml_path):
        return {"status": "failed", "error": f"data.yaml not found: {data_yaml_path}"}

    try:
        import supervision as sv
        import cv2
        import numpy as np

        def _generate():
            # 定位 images/labels 目录（兼容 train/val 子目录与平铺两种结构）
            images_dir = os.path.join(snapshot_dir, "images", "train")
            labels_dir = os.path.join(snapshot_dir, "labels", "train")
            if not os.path.isdir(images_dir):
                images_dir = os.path.join(snapshot_dir, "images")
            if not os.path.isdir(labels_dir):
                labels_dir = os.path.join(snapshot_dir, "labels")

            if not os.path.isdir(images_dir):
                return {"error": f"images dir not found: {images_dir}"}

            # 优先使用 supervision.DetectionDataset 加载（需要 labels 目录）
            dataset = None
            if os.path.isdir(labels_dir):
                try:
                    dataset = sv.DetectionDataset.from_yolo(
                        images_directory_path=images_dir,
                        annotations_directory_path=labels_dir,
                        data_yaml_path=data_yaml_path,
                    )
                except Exception as e:
                    logger.warning(f"DetectionDataset load failed, fallback to plain images: {e}")
                    dataset = None

            # 收集样本图片路径
            image_paths = []
            if dataset is not None and len(dataset) > 0:
                image_paths = list(dataset.image_paths)
            else:
                exts = (".jpg", ".jpeg", ".png", ".bmp", ".webp")
                for f in sorted(os.listdir(images_dir)):
                    if f.lower().endswith(exts):
                        image_paths.append(os.path.join(images_dir, f))

            if not image_paths:
                return {"error": "No images found in snapshot"}

            # 均匀采样
            if len(image_paths) > max_samples:
                step = len(image_paths) / max_samples
                image_paths = [image_paths[int(i * step)] for i in range(max_samples)]

            # 渲染每张缩略图
            color_palette = sv.ColorPalette.default()
            box_annotator = sv.BoxAnnotator(color=color_palette, thickness=2)
            label_annotator = sv.LabelAnnotator(color=color_palette, text_thickness=1, text_scale=0.5)

            thumbnails = []
            for img_path in image_paths:
                frame = cv2.imread(img_path)
                if frame is None:
                    continue
                # 若有 dataset 则绘制 GT 框
                if dataset is not None and img_path in dataset.annotations:
                    detections = dataset.annotations[img_path]
                    if len(detections) > 0:
                        labels = [dataset.classes[int(cid)] if 0 <= int(cid) < len(dataset.classes) else str(cid)
                                  for cid in detections.class_id]
                        frame = box_annotator.annotate(scene=frame, detections=detections)
                        frame = label_annotator.annotate(scene=frame, detections=detections, labels=labels)
                # 等比缩放到 thumb_size
                h, w = frame.shape[:2]
                scale = thumb_size / max(h, w)
                new_w, new_h = max(1, int(w * scale)), max(1, int(h * scale))
                frame = cv2.resize(frame, (new_w, new_h), interpolation=cv2.INTER_AREA)
                thumbnails.append(frame)

            if not thumbnails:
                return {"error": "All images failed to load"}

            # 合成网格
            cols = min(grid_cols, len(thumbnails))
            rows = (len(thumbnails) + cols - 1) // cols
            cell_w = thumb_size
            cell_h = thumb_size
            grid = np.zeros((rows * cell_h, cols * cell_w, 3), dtype=np.uint8)
            for idx, thumb in enumerate(thumbnails):
                r, c = idx // cols, idx % cols
                th, tw = thumb.shape[:2]
                # 居中放置
                y_off = r * cell_h + (cell_h - th) // 2
                x_off = c * cell_w + (cell_w - tw) // 2
                grid[y_off:y_off + th, x_off:x_off + tw] = thumb

            # 原子写入：先写 .tmp 再 rename
            preview_path = os.path.join(snapshot_dir, "preview.jpg")
            tmp_path = preview_path + ".tmp"
            cv2.imwrite(tmp_path, grid, [cv2.IMWRITE_JPEG_QUALITY, 85])
            if os.path.exists(preview_path):
                os.remove(preview_path)
            os.rename(tmp_path, preview_path)

            return {
                "preview_path": preview_path,
                "sample_count": len(thumbnails),
                "grid_rows": rows,
                "grid_cols": cols,
            }

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(None, _generate)
        if "error" in result:
            return {"status": "failed", "error": result["error"]}
        result["status"] = "succeeded"
        return result

    except ImportError:
        return {"status": "failed", "error": "Supervision or OpenCV is not installed"}
    except Exception as e:
        logger.error(f"Snapshot preview generation failed: {e}")
        return {"status": "failed", "error": str(e)}
