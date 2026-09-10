"""
数据集划分工具

使用 supervision.DetectionDataset 划分为 train/val 子集，并生成规范的 data.yaml 供训练使用
"""
import os
import logging
import tempfile
import yaml
from pathlib import Path
import supervision as sv

logger = logging.getLogger(__name__)


def _extract_classes_from_labels(label_dir: Path) -> list:
    """扫描所有标签文件，提取类别数量并生成 class_n 的映射列表"""
    max_class_id = -1
    if not label_dir.exists():
        return []
    
    for txt_file in label_dir.glob("*.txt"):
        try:
            with open(txt_file, "r", encoding="utf-8", errors="ignore") as f:
                for line in f:
                    parts = line.strip().split()
                    if len(parts) >= 5:
                        try:
                            cid = int(parts[0])
                            if cid > max_class_id:
                                max_class_id = cid
                        except ValueError:
                            continue
        except Exception as e:
            logger.warning(f"Error scanning label file {txt_file}: {e}")
            
    if max_class_id < 0:
        return ["class_0"]
        
    return [f"class_{i}" for i in range(max_class_id + 1)]


def split_dataset(image_dir: str, label_dir: str, output_dir: str,
                  val_ratio: float = 0.2, seed: int = 42,
                  copy_files: bool = True) -> dict:
    """
    使用 supervision.DetectionDataset 将数据集划分为 train/val，生成YOLO格式的data.yaml

    Args:
        image_dir: 图片目录
        label_dir: 标签目录
        output_dir: 输出目录（快照根目录）
        val_ratio: 验证集比例 (0.0-1.0)
        seed: 随机种子
        copy_files: 是否复制文件（注：supervision 会默认复制图像和标签文件）

    Returns:
        dict with keys: train_count, val_count, data_yaml_path, classes, class_count
    """
    img_dir = Path(image_dir)
    lbl_dir = Path(label_dir)
    out_dir = Path(output_dir)

    if not img_dir.exists():
        return {"error": f"Image directory does not exist: {image_dir}"}
    if not lbl_dir.exists():
        return {"error": f"Label directory does not exist: {label_dir}"}

    # 1. 尝试寻找现有的 data.yaml，如果没有则生成一个临时的 data.yaml 供 supervision 读取
    yaml_path = None
    for parent in [lbl_dir.parent, img_dir.parent]:
        possible_yaml = parent / "data.yaml"
        if possible_yaml.exists():
            yaml_path = possible_yaml
            break

    temp_dir_obj = None
    try:
        if yaml_path is None:
            # 自动提取类别并写入临时 data.yaml
            classes = _extract_classes_from_labels(lbl_dir)
            temp_dir_obj = tempfile.TemporaryDirectory()
            temp_yaml_path = Path(temp_dir_obj.name) / "data.yaml"
            
            yaml_data = {
                "names": classes,
                "nc": len(classes),
                "train": "images",
                "val": "images"
            }
            with open(temp_yaml_path, "w", encoding="utf-8") as f:
                yaml.dump(yaml_data, f)
            yaml_path = temp_yaml_path
        else:
            # 如果存在 data.yaml，读取它的 classes
            try:
                with open(yaml_path, "r", encoding="utf-8") as f:
                    yaml_data = yaml.safe_load(f)
                    classes = yaml_data.get("names", [])
                    if isinstance(classes, dict):
                        classes = [classes[i] for i in sorted(classes.keys())]
            except Exception:
                classes = _extract_classes_from_labels(lbl_dir)

        # 2. 使用 supervision 加载整个 YOLO 数据集
        dataset = sv.DetectionDataset.from_yolo(
            images_directory_path=str(img_dir),
            annotations_directory_path=str(lbl_dir),
            data_yaml_path=str(yaml_path)
        )

        if len(dataset) == 0:
            return {"error": "No matched image-label pairs found"}

        # 3. 划分数据集为 train 和 val
        train_ratio = 1.0 - val_ratio
        train_dataset, val_dataset = dataset.split(split_ratio=train_ratio, random_state=seed)

        train_img_dir = out_dir / "images" / "train"
        train_lbl_dir = out_dir / "labels" / "train"
        val_img_dir = out_dir / "images" / "val"
        val_lbl_dir = out_dir / "labels" / "val"

        for d in [train_img_dir, train_lbl_dir, val_img_dir, val_lbl_dir]:
            d.mkdir(parents=True, exist_ok=True)

        # 4. 分别写回训练集和验证集
        train_dataset.as_yolo(
            images_directory_path=str(train_img_dir),
            annotations_directory_path=str(train_lbl_dir)
        )
        val_dataset.as_yolo(
            images_directory_path=str(val_img_dir),
            annotations_directory_path=str(val_lbl_dir)
        )

        # 5. 生成 data.yaml
        data_yaml_path = out_dir / "data.yaml"
        yaml_content = f"path: {out_dir.resolve().as_posix()}\ntrain: images/train\nval: images/val\nnc: {len(classes)}\nnames: {classes}\n"
        with open(data_yaml_path, "w", encoding="utf-8") as f:
            f.write(yaml_content)

        logger.info(f"Dataset split complete: train={len(train_dataset)}, val={len(val_dataset)}, "
                    f"classes={len(classes)}, data_yaml={data_yaml_path}")

        return {
            "train_count": len(train_dataset),
            "val_count": len(val_dataset),
            "total_count": len(dataset),
            "data_yaml_path": str(data_yaml_path),
            "classes": classes,
            "class_count": len(classes),
        }

    except Exception as e:
        logger.error(f"Dataset split failed: {e}")
        return {"error": str(e)}
    finally:
        if temp_dir_obj is not None:
            try:
                temp_dir_obj.cleanup()
            except Exception:
                pass
