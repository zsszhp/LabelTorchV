import sys
import os
import pytest
from unittest.mock import MagicMock, patch

# 将后端目录加入 Python 路径
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.handlers.inference import handle_run


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
async def test_handle_run_output_structure(mock_yolo, tmp_path):
    # Mock YOLO 结果
    import numpy as np
    mock_boxes = MagicMock()
    mock_boxes.cls.cpu().numpy.return_value = np.array([0])
    mock_boxes.conf.cpu().numpy.return_value = np.array([0.85])
    mock_boxes.xyxy.cpu().numpy.return_value = np.array([[10, 20, 100, 200]])
    mock_boxes.id = None
    mock_boxes.__len__.return_value = 1

    mock_result = MagicMock()
    mock_result.path = "test.jpg"
    mock_result.names = {0: "defect"}
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
        "conf": 0.25
    }

    response = await handle_run(payload)

    assert response["status"] == "succeeded"
    assert len(response["predictions"]) == 1
    pred = response["predictions"][0]
    assert pred["path"] == "test.jpg"
    assert len(pred["boxes"]) == 1
    assert pred["boxes"][0]["class_id"] == 0
    assert pred["boxes"][0]["class_name"] == "defect"
    assert pred["boxes"][0]["confidence"] == 0.85
    assert pred["boxes"][0]["xyxy"] == [10, 20, 100, 200]


@pytest.mark.asyncio
@patch('ultralytics.YOLO')
@patch('labeltorch_backend.tools.path_security.collect_allowed_roots')
async def test_handle_run_rejects_path_outside_roots(mock_roots, mock_yolo, tmp_path):
    """P0-19：越界权重路径必须返回结构化错误，不得启动推理"""
    project = tmp_path / "proj"
    project.mkdir()
    (project / "project.json").write_text("{}", encoding="utf-8")
    weight = project / "model.pt"
    weight.write_bytes(b"fake")
    image = project / "img.jpg"
    image.write_bytes(b"fake")

    outside = tmp_path / "outside"
    outside.mkdir()
    evil_weight = outside / "evil.pt"
    evil_weight.write_bytes(b"fake")

    # 白名单仅含项目目录，项目外权重必须被拒绝
    mock_roots.return_value = [str(project)]

    payload = {
        "weight_path": str(evil_weight),
        "source": str(image),
        "project_root": str(project),
    }
    response = await handle_run(payload)

    assert response["status"] == "failed"
    assert isinstance(response["error"], dict)
    assert response["error"]["code"] == "PATH_DENIED"
    mock_yolo.assert_not_called()
