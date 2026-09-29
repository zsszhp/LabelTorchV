import sys
import os
import pytest
from unittest.mock import MagicMock, patch

# 将后端目录加入 Python 路径
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.handlers.active_learning import handle_collect_low_conf


def _prepare_project(tmp_path):
    """构造带 project.json 标记的项目目录，返回 (权重路径, 图片路径)"""
    project = tmp_path / "proj"
    project.mkdir()
    (project / "project.json").write_text("{}", encoding="utf-8")
    weight = project / "model.pt"
    weight.write_bytes(b"fake")
    image = project / "img.jpg"
    image.write_bytes(b"fake")
    return str(weight), str(image)


@pytest.mark.asyncio
@patch('ultralytics.YOLO')
async def test_handle_collect_low_conf(mock_yolo, tmp_path):
    # 两个框：一个低置信度（0.35），一个高置信度（0.85）
    import numpy as np
    mock_boxes = MagicMock()
    mock_boxes.cls.cpu().numpy.return_value = np.array([0, 1])
    mock_boxes.conf.cpu().numpy.return_value = np.array([0.35, 0.85])
    mock_boxes.xyxy.cpu().numpy.return_value = np.array([[10, 20, 100, 200], [30, 40, 300, 400]])
    mock_boxes.id = None
    mock_boxes.__len__.return_value = 2

    mock_result = MagicMock()
    mock_result.path = "test.jpg"
    mock_result.boxes = mock_boxes
    mock_result.obb = None
    mock_result.masks = None
    mock_result.probs = None

    mock_model = MagicMock()
    mock_model.predict.return_value = [mock_result]
    mock_yolo.return_value = mock_model

    weight_path, image_path = _prepare_project(tmp_path)
    payload = {
        "weight_path": weight_path,
        "source": image_path,
        "conf_threshold": 0.3
    }

    response = await handle_collect_low_conf(payload)

    assert response["status"] == "succeeded"
    assert len(response["samples"]) == 1
    sample = response["samples"][0]
    # 应该只有 0.35 的那个框进入，因为 0.85 已经高于 conf_threshold + 0.2 (0.5) 了
    assert sample["box_count"] == 1
    assert sample["boxes"][0]["confidence"] == 0.35
