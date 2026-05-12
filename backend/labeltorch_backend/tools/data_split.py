"""
数据集划分工具

将YOLO格式的数据集划分为train/val子集，生成data.yaml供训练使用
"""
import os
import random
import shutil
import logging
from pathlib import Path

logger = logging.getLogger(__name__)


def split_dataset(image_dir: str, label_dir: str, output_dir: str,
                  val_ratio: float = 0.2, seed: int = 42,
                  copy_files: bool = True) -> dict:
    """
    将数据集划分为train/val，生成YOLO格式的data.yaml

    Args:
        image_dir: 图片目录
        label_dir: 标签目录
        output_dir: 输出目录（快照根目录）
        val_ratio: 验证集比例 (0.0-1.0)
        seed: 随机种子
        copy_files: 是否复制文件（True=复制，False=软链接）

    Returns:
        dict with keys: train_count, val_count, data_yaml_path, classes
    """
    img_dir = Path(image_dir)
    lbl_dir = Path(label_dir)
    out_dir = Path(output_dir)

    if not img_dir.exists():
        return {"error": f"Image directory does not exist: {image_dir}"}
    if not lbl_dir.exists():
        return {"error": f"Label directory does not exist: {label_dir}"}

    image_exts = {".jpg", ".jpeg", ".png", ".bmp"}

    img_files = sorted([
        f for f in img_dir.iterdir()
        if f.is_file() and f.suffix.lower() in image_exts
    ])

    matched = []
    for img_path in img_files:
        stem = img_path.stem
        label_path = lbl_dir / f"{stem}.txt"
        if label_path.exists():
            matched.append((img_path, label_path))

    if not matched:
        return {"error": "No matched image-label pairs found"}

    random.seed(seed)
    random.shuffle(matched)

    val_count = max(1, int(len(matched) * val_ratio))
    train_count = len(matched) - val_count

    train_pairs = matched[:train_count]
    val_pairs = matched[train_count:]

    train_img_dir = out_dir / "images" / "train"
    train_lbl_dir = out_dir / "labels" / "train"
    val_img_dir = out_dir / "images" / "val"
    val_lbl_dir = out_dir / "labels" / "val"

    for d in [train_img_dir, train_lbl_dir, val_img_dir, val_lbl_dir]:
        d.mkdir(parents=True, exist_ok=True)

    def _copy_or_link(src: Path, dst: Path):
        if dst.exists():
            dst.unlink()
        if copy_files:
            shutil.copy2(str(src), str(dst))
        else:
            dst.symlink_to(src.resolve())

    for img_path, lbl_path in train_pairs:
        _copy_or_link(img_path, train_img_dir / img_path.name)
        _copy_or_link(lbl_path, train_lbl_dir / lbl_path.name)

    for img_path, lbl_path in val_pairs:
        _copy_or_link(img_path, val_img_dir / img_path.name)
        _copy_or_link(lbl_path, val_lbl_dir / lbl_path.name)

    classes = _extract_classes(matched)

    data_yaml_path = out_dir / "data.yaml"
    _write_data_yaml(data_yaml_path, out_dir, classes)

    logger.info(f"Dataset split complete: train={train_count}, val={val_count}, "
                f"classes={len(classes)}, data_yaml={data_yaml_path}")

    return {
        "train_count": train_count,
        "val_count": val_count,
        "total_count": len(matched),
        "data_yaml_path": str(data_yaml_path),
        "classes": classes,
        "class_count": len(classes),
    }


def _extract_classes(pairs: list) -> list:
    """从所有标签文件中提取类别列表"""
    max_class_id = -1
    found_ids = set()

    for _, lbl_path in pairs:
        try:
            with open(lbl_path, "r") as f:
                for line in f:
                    line = line.strip()
                    if not line:
                        continue
                    parts = line.split()
                    if len(parts) >= 5:
                        try:
                            cid = int(parts[0])
                            found_ids.add(cid)
                            if cid > max_class_id:
                                max_class_id = cid
                        except ValueError:
                            continue
        except Exception as e:
            logger.warning(f"Error reading label file {lbl_path}: {e}")

    if max_class_id < 0:
        return []

    classes = []
    for i in range(max_class_id + 1):
        classes.append(f"class_{i}")

    return classes


def _write_data_yaml(yaml_path: Path, dataset_dir: Path, classes: list):
    """生成YOLO格式的data.yaml"""
    content = [
        f"path: {dataset_dir.resolve()}",
        "train: images/train",
        "val: images/val",
        f"nc: {len(classes)}",
        f"names: {classes}",
    ]

    with open(yaml_path, "w", encoding="utf-8") as f:
        f.write("\n".join(content) + "\n")

    logger.info(f"data.yaml written to {yaml_path}")
