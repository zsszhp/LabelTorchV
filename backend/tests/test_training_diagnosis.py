# -*- coding: utf-8 -*-
"""P1-19：训练失败诊断（日志尾部 + 结构化建议）单元测试"""

from labeltorch_backend.handlers.training import (
    LOG_TAIL_LINES,
    _append_log_tail,
    _log_tails,
    _take_log_tail,
    diagnose_failure,
)


def test_diagnose_oom():
    diag = diagnose_failure("CUDA out of memory. Tried to allocate 2.00 GiB", {"batch": 16, "imgsz": 640})
    assert diag["code"] == "OOM"
    assert "OOM" in diag["message"] or "显存" in diag["message"]
    assert len(diag["suggestions"]) >= 2
    # 建议应包含降 batch / imgsz
    joined = " ".join(diag["suggestions"])
    assert "batch" in joined
    assert "imgsz" in joined


def test_diagnose_data_error():
    diag = diagnose_failure("No labels found in /data/labels, can not start training")
    assert diag["code"] == "DATA_ERROR"
    assert any("标签" in s for s in diag["suggestions"])


def test_diagnose_config_error():
    diag = diagnose_failure("Error(s) in loading state_dict: size mismatch for head.weight")
    assert diag["code"] == "CONFIG_ERROR"
    assert any("类别" in s or "配置" in s for s in diag["suggestions"])


def test_diagnose_env_error():
    diag = diagnose_failure("No module named 'ultralytics'")
    assert diag["code"] == "ENV_ERROR"


def test_diagnose_unknown_fallback():
    diag = diagnose_failure("something exploded")
    assert diag["code"] == "UNKNOWN"
    assert len(diag["suggestions"]) >= 1


def test_log_tail_ring_buffer():
    task_id = "run-test-tail"
    for i in range(LOG_TAIL_LINES + 10):
        _append_log_tail(task_id, f"line-{i}")
    tail = _take_log_tail(task_id)
    assert len(tail) == LOG_TAIL_LINES
    # 环形缓冲应保留最后 N 行
    assert tail[-1] == f"line-{LOG_TAIL_LINES + 9}"
    assert tail[0] == f"line-{10}"
    # 取走后应清空
    assert _take_log_tail(task_id) == []


def test_log_tail_take_clears():
    task_id = "run-take-clear"
    _append_log_tail(task_id, "a")
    _append_log_tail(task_id, "b")
    assert _take_log_tail(task_id) == ["a", "b"]
    assert task_id not in _log_tails


def test_diagnose_empty_error():
    diag = diagnose_failure("")
    assert diag["code"] == "UNKNOWN"
    assert diag["message"]
