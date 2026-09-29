# -*- coding: utf-8 -*-
"""P1-16 / P1-17 / P1-18：PR 曲线提取、阈值推荐、Go/No-Go、逐类指标单元测试"""
import math

import numpy as np
import pytest

from labeltorch_backend.handlers.testing import (
    GATE_AUROC,
    GATE_MAP50,
    _build_go_no_go,
    _extract_confidence_curves,
    _extract_per_class_metrics,
    _extract_pr_curve_from_results,
    _recommend_threshold,
    _sweep_threshold_curves,
    _anomaly_threshold_recommendation,
)


class FakeBox:
    """模拟 ultralytics Metric 对象"""

    def __init__(self):
        nc = 2
        self.p = np.array([0.9, 0.8])
        self.r = np.array([0.7, 0.6])
        self.f1 = np.array([0.79, 0.69])
        self.all_ap = np.array([[0.75, 0.7], [0.65, 0.6]])
        self.ap_class_index = [0, 1]
        self.nc = nc
        self.px = np.linspace(0, 1, 1000)
        self.prec_values = np.linspace(1, 0, 1000)[None, :].repeat(nc, axis=0)
        self.p_curve = np.linspace(1, 0.1, 1000)[None, :].repeat(nc, axis=0)
        self.r_curve = np.linspace(0, 1, 1000)[None, :].repeat(nc, axis=0)
        self.f1_curve = 2 * self.p_curve * self.r_curve / (self.p_curve + self.r_curve + 1e-16)

    @property
    def ap50(self):
        return self.all_ap[:, 0]

    @property
    def ap(self):
        return self.all_ap.mean(1)

    @property
    def curves_results(self):
        return [
            [self.px, self.prec_values, "Recall", "Precision"],
            [self.px, self.f1_curve, "Confidence", "F1"],
            [self.px, self.p_curve, "Confidence", "Precision"],
            [self.px, self.r_curve, "Confidence", "Recall"],
        ]


class FakeResults:
    def __init__(self):
        self.names = {0: "scratch", 1: "dent"}
        self.box = FakeBox()
        self.nt_per_class = np.array([10, 20])

    @property
    def curves_results(self):
        return self.box.curves_results


def test_extract_pr_curve_points():
    results = FakeResults()
    pr_curve = _extract_pr_curve_from_results(results)
    assert len(pr_curve) == 1000
    first = pr_curve[0]
    # 字段名稳定：recall / precision / confidence / f1
    assert "recall" in first and "precision" in first
    assert "confidence" in first and "f1" in first
    assert 0.0 <= first["recall"] <= 1.0
    assert 0.0 <= first["precision"] <= 1.0
    # recall 轴应递增
    assert pr_curve[0]["recall"] < pr_curve[-1]["recall"]


def test_extract_pr_curve_empty_when_no_results():
    class Empty:
        pass

    assert _extract_pr_curve_from_results(Empty()) == []


def test_extract_confidence_curves():
    results = FakeResults()
    f1_curve, p_curve, r_curve = _extract_confidence_curves(results)
    assert len(f1_curve) == 1000
    assert len(p_curve) == 1000
    assert len(r_curve) == 1000
    assert "f1" in f1_curve[0]
    assert "precision" in p_curve[0]
    assert "recall" in r_curve[0]
    # 置信度轴应从 0 到 1
    assert f1_curve[0]["confidence"] == pytest.approx(0.0)
    assert f1_curve[-1]["confidence"] == pytest.approx(1.0)


def test_recommend_threshold_max_f1():
    # 构造单峰 F1 曲线，峰值在 conf=0.4
    f1_curve = [{"confidence": c / 10.0, "f1": 1.0 - abs(c / 10.0 - 0.4)} for c in range(11)]
    p_curve = [{"confidence": p["confidence"], "precision": 0.8} for p in f1_curve]
    r_curve = [{"confidence": p["confidence"], "recall": 0.7} for p in f1_curve]

    rec = _recommend_threshold(f1_curve, p_curve, r_curve, {})
    assert rec["method"] == "max_f1"
    assert rec["recommended_conf"] == pytest.approx(0.4, abs=1e-9)
    assert rec["f1_at_threshold"] == pytest.approx(1.0, abs=1e-9)
    assert rec["expected_miss_rate"] == pytest.approx(0.3, abs=1e-9)
    assert rec["expected_false_alarm_rate"] == pytest.approx(0.2, abs=1e-9)


def test_recommend_threshold_cost_weighted():
    # 高漏检代价应把阈值压低（偏向高召回）
    f1_curve = [
        {"confidence": 0.2, "f1": 0.5},
        {"confidence": 0.5, "f1": 0.8},
        {"confidence": 0.8, "f1": 0.6},
    ]
    p_curve = [
        {"confidence": 0.2, "precision": 0.4},
        {"confidence": 0.5, "precision": 0.7},
        {"confidence": 0.8, "precision": 0.95},
    ]
    r_curve = [
        {"confidence": 0.2, "recall": 0.95},
        {"confidence": 0.5, "recall": 0.7},
        {"confidence": 0.8, "recall": 0.3},
    ]

    rec = _recommend_threshold(f1_curve, p_curve, r_curve, {
        "miss_cost": 10.0,
        "false_alarm_cost": 1.0,
    })
    assert rec["method"] == "cost_weighted"
    assert rec["recommended_conf"] == pytest.approx(0.2)


def test_recommend_threshold_empty():
    rec = _recommend_threshold([], [], [], {})
    assert rec["recommended_conf"] == pytest.approx(0.25)
    assert rec["method"] == "max_f1"


def test_build_go_no_go_detect_pass():
    metrics = {"mAP50": 0.9, "mAP50-95": 0.75, "precision": 0.9, "recall": 0.85}
    go = _build_go_no_go(metrics, {})
    assert go["decision"] == "go"
    assert go["gate_metric"] == "mAP50"
    assert go["gate_threshold"] == pytest.approx(GATE_MAP50)
    assert go["gate_passed"] is True
    assert go["regression_vs_baseline"] is None


def test_build_go_no_go_detect_fail():
    metrics = {"mAP50": 0.5}
    go = _build_go_no_go(metrics, {})
    assert go["decision"] == "no_go"
    assert go["gate_passed"] is False


def test_build_go_no_go_anomaly_gate():
    metrics = {"auroc": 0.97, "f1": 0.9}
    go = _build_go_no_go(metrics, {})
    assert go["gate_metric"] == "AUROC"
    assert go["gate_threshold"] == pytest.approx(GATE_AUROC)
    assert go["decision"] == "go"

    metrics_fail = {"auroc": 0.9}
    go_fail = _build_go_no_go(metrics_fail, {})
    assert go_fail["decision"] == "no_go"


def test_build_go_no_go_baseline_regression():
    metrics = {"mAP50": 0.88}
    go = _build_go_no_go(metrics, {"baseline_metrics": {"mAP50": 0.95}})
    assert go["gate_passed"] is True  # 0.88 > 0.85
    assert go["regression_vs_baseline"] is True
    assert go["decision"] == "no_go"
    assert go["baseline_value"] == pytest.approx(0.95)
    assert any("回归" in r for r in go["reasons"])


def test_build_go_no_go_baseline_no_regression():
    metrics = {"mAP50": 0.96}
    go = _build_go_no_go(metrics, {"baseline_metrics": {"mAP50": 0.95}})
    assert go["regression_vs_baseline"] is False
    assert go["decision"] == "go"


def test_per_class_metrics_sorted_by_fn():
    results = FakeResults()
    per_class = _extract_per_class_metrics(results)
    assert len(per_class) == 2
    # class0: support=10, r=0.7 → fn=3; class1: support=20, r=0.6 → fn=8
    assert per_class[0]["className"] == "dent"
    assert per_class[0]["fn_count"] == 8
    assert per_class[1]["className"] == "scratch"
    assert per_class[1]["fn_count"] == 3
    for item in per_class:
        assert set(item.keys()) >= {
            "classIndex", "className", "precision", "recall",
            "f1", "ap50", "ap", "support", "fn_count", "fp_count",
        }


def test_sweep_threshold_curves_shapes():
    # 可分数据：低分=正常，高分=异常
    scores = [0.1, 0.2, 0.3, 0.8, 0.9, 1.0]
    labels = [0, 0, 0, 1, 1, 1]
    f1_curve, pr_curve = _sweep_threshold_curves(scores, labels)
    assert len(f1_curve) == 1000
    assert len(pr_curve) == 1000
    # 存在某个阈值使得 F1 达到较高值
    best_f1 = max(p["f1"] for p in f1_curve)
    assert best_f1 > 0.8
    # PR 点字段稳定
    assert {"recall", "precision", "confidence", "f1"} <= set(pr_curve[0].keys())


def test_sweep_threshold_curves_empty():
    f1_curve, pr_curve = _sweep_threshold_curves([], [])
    assert f1_curve == [] and pr_curve == []


def test_anomaly_threshold_recommendation():
    scores = [0.1, 0.15, 0.2, 0.85, 0.9, 0.95]
    labels = [0, 0, 0, 1, 1, 1]
    f1_curve, pr_curve = _sweep_threshold_curves(scores, labels)
    rec = _anomaly_threshold_recommendation(f1_curve, pr_curve, {})
    assert rec["method"] == "max_f1"
    assert 0.0 < rec["recommended_conf"] < 1.0
    assert rec["f1_at_threshold"] > 0.5
    assert rec["expected_miss_rate"] >= 0.0
    assert rec["expected_false_alarm_rate"] >= 0.0


def test_curve_points_finite():
    """所有曲线点必须为有限浮点，防止 NaN 进入 UI。"""
    results = FakeResults()
    pr_curve = _extract_pr_curve_from_results(results)
    f1_curve, p_curve, r_curve = _extract_confidence_curves(results)
    for pt in pr_curve:
        for v in pt.values():
            assert math.isfinite(float(v))
    for curve, key in ((f1_curve, "f1"), (p_curve, "precision"), (r_curve, "recall")):
        for pt in curve:
            assert math.isfinite(float(pt["confidence"]))
            assert math.isfinite(float(pt[key]))
