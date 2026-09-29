"""P0-13 增量训练 resume / P0-17 validate_config 测试"""
import os
import sys

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.adapters.ultralytics_adapter import UltralyticsAdapter


def test_locate_resume_checkpoint_finds_last_pt(tmp_path):
    """project_dir/run_name/weights/last.pt 应被定位到"""
    run_dir = tmp_path / "runs" / "train"
    weights = run_dir / "weights"
    weights.mkdir(parents=True)
    last_pt = weights / "last.pt"
    last_pt.write_bytes(b"x")

    found = UltralyticsAdapter._locate_resume_checkpoint(
        {}, str(tmp_path / "runs"), "train"
    )
    assert found is not None
    assert os.path.samefile(found, str(last_pt))


def test_locate_resume_checkpoint_prefers_explicit_path(tmp_path):
    """显式 resume_weight 优先于默认布局"""
    explicit = tmp_path / "explicit_last.pt"
    explicit.write_bytes(b"x")

    found = UltralyticsAdapter._locate_resume_checkpoint(
        {"resume_weight": str(explicit)}, str(tmp_path), "train"
    )
    assert found is not None
    assert os.path.samefile(found, str(explicit))


def test_locate_resume_checkpoint_missing_returns_none(tmp_path):
    """无断点权重时返回 None，由调用方报错"""
    found = UltralyticsAdapter._locate_resume_checkpoint(
        {}, str(tmp_path / "empty"), "train"
    )
    assert found is None


@pytest.mark.asyncio
async def test_start_training_resume_without_last_pt_fails(tmp_path):
    """P0-13：resume=true 且无 last.pt 必须返回「无断点权重，无法增量训练」"""
    adapter = UltralyticsAdapter()
    config = {
        "model_family": "yolov8",
        "model_variant": "n",
        "data_yaml": str(tmp_path / "data.yaml"),
        "epochs": 1,
        "batch": 1,
        "resume": True,
        "project_dir": str(tmp_path / "no_runs"),
        "run_name": "train",
    }
    result = await adapter.start_training(config)
    assert result["status"] == "failed"
    assert result["error"] == "无断点权重，无法增量训练"


@pytest.mark.asyncio
async def test_handle_start_rejects_invalid_config():
    """P0-17：validate_config 失败必须返回结构化错误且不启动训练"""
    from labeltorch_backend.adapters.registry import register_builtin_adapters
    from labeltorch_backend.handlers.training import handle_start

    register_builtin_adapters()

    payload = {
        "run_id": "test-run-invalid",
        "config": {
            "adapter": "ultralytics",
            "model_family": "not_a_real_model",
            "epochs": 0,
            "batch": 0,
        },
    }
    result = await handle_start(payload)
    assert result["status"] == "failed"
    assert isinstance(result["error"], dict)
    assert result["error"]["code"] == "CONFIG_INVALID"
    assert result["error"]["details"]
    # 校验失败不应登记活跃任务
    from labeltorch_backend.handlers import training as training_mod
    assert "test-run-invalid" not in training_mod._active_tasks
