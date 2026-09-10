"""
数据集处理器（P1-4 统计强化 / P2-5 格式标准化）

基于 supervision.DetectionDataset 提供：
- dataset.stats: 类别分布、框尺寸统计、样本数
- dataset.convert_to_yolo: COCO/Pascal VOC/YOLO 互转
"""
import asyncio
import logging
import os
from pathlib import Path

logger = logging.getLogger(__name__)


async def handle_stats(payload: dict) -> dict:
    """
    生成数据集统计（P1-4）

    payload:
        image_dir: 图片目录
        label_dir: 标签目录
        data_yaml_path: data.yaml 路径（可选，无则自动推断）
    """
    image_dir = payload.get("image_dir", "")
    label_dir = payload.get("label_dir", "")
    data_yaml_path = payload.get("data_yaml_path", "")

    if not image_dir or not os.path.isdir(image_dir):
        return {"status": "failed", "error": f"Invalid image_dir: {image_dir}"}
    if not label_dir or not os.path.isdir(label_dir):
        return {"status": "failed", "error": f"Invalid label_dir: {label_dir}"}

    try:
        import supervision as sv
        import numpy as np
        import yaml

        def _compute():
            # 自动推断 data.yaml
            yaml_path = data_yaml_path
            if not yaml_path or not os.path.isfile(yaml_path):
                # 扫描标签提取类别数
                max_cid = -1
                for txt in Path(label_dir).glob("*.txt"):
                    try:
                        for line in txt.read_text(encoding="utf-8", errors="ignore").splitlines():
                            parts = line.strip().split()
                            if len(parts) >= 5:
                                try:
                                    max_cid = max(max_cid, int(parts[0]))
                                except ValueError:
                                    continue
                    except Exception:
                        continue
                classes = [f"class_{i}" for i in range(max(max_cid + 1, 1))]
                # 写临时 yaml
                import tempfile
                tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".yaml", delete=False, encoding="utf-8")
                yaml.dump({"names": classes, "nc": len(classes), "train": "images", "val": "images"}, tmp)
                tmp.close()
                yaml_path = tmp.name
            else:
                with open(yaml_path, "r", encoding="utf-8") as f:
                    yd = yaml.safe_load(f)
                    classes = yd.get("names", [])
                    if isinstance(classes, dict):
                        classes = [classes[i] for i in sorted(classes.keys())]

            try:
                dataset = sv.DetectionDataset.from_yolo(
                    images_directory_path=image_dir,
                    annotations_directory_path=label_dir,
                    data_yaml_path=yaml_path,
                )
            finally:
                # 清理临时 yaml
                if not data_yaml_path and os.path.isfile(yaml_path):
                    try:
                        os.remove(yaml_path)
                    except Exception:
                        pass

            total_samples = len(dataset)
            if total_samples == 0:
                return {
                    "total_samples": 0,
                    "class_distribution": {},
                    "class_names": classes,
                }

            # 类别分布
            class_dist = {}
            box_widths = []
            box_heights = []
            box_areas = []
            annotated = 0

            for img_path, detections in dataset.annotations.items():
                if len(detections) > 0:
                    annotated += 1
                for cid, xyxy in zip(detections.class_id, detections.xyxy):
                    key = str(int(cid))
                    class_dist[key] = class_dist.get(key, 0) + 1
                    x1, y1, x2, y2 = xyxy
                    w = float(x2 - x1)
                    h = float(y2 - y1)
                    box_widths.append(w)
                    box_heights.append(h)
                    box_areas.append(w * h)

            result = {
                "total_samples": total_samples,
                "annotated_samples": annotated,
                "unlabeled_samples": total_samples - annotated,
                "class_distribution": class_dist,
                "class_names": classes,
                "total_boxes": len(box_widths),
            }

            if box_widths:
                wa = np.array(box_areas)
                result["box_size_stats"] = {
                    "width_min": round(float(np.min(box_widths)), 2),
                    "width_max": round(float(np.max(box_widths)), 2),
                    "width_avg": round(float(np.mean(box_widths)), 2),
                    "height_min": round(float(np.min(box_heights)), 2),
                    "height_max": round(float(np.max(box_heights)), 2),
                    "height_avg": round(float(np.mean(box_heights)), 2),
                    "area_min": round(float(np.min(wa)), 2),
                    "area_max": round(float(np.max(wa)), 2),
                    "area_avg": round(float(np.mean(wa)), 2),
                    "area_median": round(float(np.median(wa)), 2),
                }
                result["avg_boxes_per_sample"] = round(len(box_widths) / total_samples, 2)
            else:
                result["box_size_stats"] = {}
                result["avg_boxes_per_sample"] = 0.0

            return result

        loop = asyncio.get_event_loop()
        stats = await loop.run_in_executor(None, _compute)
        stats["status"] = "succeeded"
        return stats

    except ImportError:
        return {"status": "failed", "error": "Supervision or NumPy is not installed"}
    except Exception as e:
        logger.error(f"Dataset stats failed: {e}")
        return {"status": "failed", "error": str(e)}


async def handle_convert_to_yolo(payload: dict) -> dict:
    """
    数据集格式转换为 YOLO（P2-5）

    payload:
        source_format: 源格式 (coco / pascal_voc / yolo)
        image_dir: 源图片目录
        annotation_dir: 源标注目录（COCO 为 json 文件所在目录，Pascal VOC 为 xml 所在目录）
        output_image_dir: 输出图片目录
        output_label_dir: 输出标签目录
        coco_json_path: COCO 格式的 json 文件路径（source_format=coco 时必填）
        data_yaml_path: data.yaml 路径（source_format=yolo 时必填）
        class_names: 类别名称列表（可选，用于覆盖）
    """
    source_format = payload.get("source_format", "yolo").lower()
    image_dir = payload.get("image_dir", "")
    annotation_dir = payload.get("annotation_dir", "")
    output_image_dir = payload.get("output_image_dir", "")
    output_label_dir = payload.get("output_label_dir", "")
    coco_json_path = payload.get("coco_json_path", "")
    data_yaml_path = payload.get("data_yaml_path", "")

    if not image_dir or not os.path.isdir(image_dir):
        return {"status": "failed", "error": f"Invalid image_dir: {image_dir}"}
    if not output_image_dir or not output_label_dir:
        return {"status": "failed", "error": "Missing output dirs"}

    try:
        import supervision as sv

        def _convert():
            os.makedirs(output_image_dir, exist_ok=True)
            os.makedirs(output_label_dir, exist_ok=True)

            if source_format == "coco":
                if not coco_json_path or not os.path.isfile(coco_json_path):
                    return {"error": "coco_json_path required for COCO format"}
                dataset = sv.DetectionDataset.from_coco(
                    images_directory_path=image_dir,
                    annotations_path=coco_json_path,
                )
            elif source_format in ("pascal_voc", "voc", "pascal"):
                if not annotation_dir or not os.path.isdir(annotation_dir):
                    return {"error": "annotation_dir required for Pascal VOC format"}
                dataset = sv.DetectionDataset.from_pascal_voc(
                    images_directory_path=image_dir,
                    annotations_directory_path=annotation_dir,
                )
            elif source_format == "yolo":
                if not data_yaml_path or not os.path.isfile(data_yaml_path):
                    return {"error": "data_yaml_path required for YOLO format"}
                dataset = sv.DetectionDataset.from_yolo(
                    images_directory_path=image_dir,
                    annotations_directory_path=annotation_dir or image_dir,
                    data_yaml_path=data_yaml_path,
                )
            else:
                return {"error": f"Unsupported source_format: {source_format}"}

            if len(dataset) == 0:
                return {"error": "No samples loaded from source"}

            # 写出为 YOLO 格式
            dataset.as_yolo(
                images_directory_path=output_image_dir,
                annotations_directory_path=output_label_dir,
            )

            return {
                "converted_count": len(dataset),
                "classes": list(dataset.classes),
                "output_image_dir": output_image_dir,
                "output_label_dir": output_label_dir,
            }

        loop = asyncio.get_event_loop()
        result = await loop.run_in_executor(None, _convert)
        if "error" in result:
            return {"status": "failed", "error": result["error"]}
        result["status"] = "succeeded"
        return result

    except ImportError:
        return {"status": "failed", "error": "Supervision is not installed"}
    except Exception as e:
        logger.error(f"Dataset convert failed: {e}")
        return {"status": "failed", "error": str(e)}
