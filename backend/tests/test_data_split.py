import sys
import os
import tempfile
from pathlib import Path

# 将后端目录加入 Python 路径
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.tools.data_split import split_dataset

def test_split_dataset():
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = Path(tmpdir)
        images_dir = tmp_path / "images"
        labels_dir = tmp_path / "labels"
        output_dir = tmp_path / "output"
        
        images_dir.mkdir()
        labels_dir.mkdir()
        
        from PIL import Image
        # 创建 5 个模拟图像和标签
        for i in range(5):
            img = Image.new('RGB', (1, 1), color='white')
            img.save(images_dir / f"img_{i}.png")
            (labels_dir / f"img_{i}.txt").write_text("0 0.5 0.5 0.2 0.2\n")
        
        result = split_dataset(
            image_dir=str(images_dir),
            label_dir=str(labels_dir),
            output_dir=str(output_dir),
            val_ratio=0.4,
            seed=42,
            copy_files=True
        )
        
        assert "error" not in result
        assert result["total_count"] == 5
        # 5 * 0.4 = 2.0
        assert result["val_count"] == 2
        assert result["train_count"] == 3
        assert os.path.exists(output_dir / "data.yaml")
