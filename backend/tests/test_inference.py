import sys
import os
import pytest
from unittest.mock import MagicMock, patch

# 将后端目录加入 Python 路径
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.handlers.inference import handle_run

@pytest.mark.asyncio
@patch('ultralytics.YOLO')
async def test_handle_run_output_structure(mock_yolo):
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

    payload = {
        "weight_path": "dummy.pt",
        "source": "dummy.jpg",
        "conf": 0.25
    }
    
    with patch('os.path.isfile', return_value=True):
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
