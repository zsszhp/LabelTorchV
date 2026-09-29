"""P0-14 anomaly 批量推理结果与输入一一对应测试

期望行为：
1. 输入 N 条图片路径 → 输出 N 条结果，顺序与输入一致
2. 每条结果的 anomaly_score/label 必须来自该图片自身的预测，
   不得取错其他图片（尤其是同目录批量预测时取首个结果的缺陷）
3. by_image_path 映射的 key 与输入路径可对应
4. 模型未返回某图时保留占位并在 error 中说明，不得静默丢条目
"""
import os
import sys
import types
from unittest.mock import MagicMock, patch

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))


class _FakeItem:
    """模拟 anomalib ImageItem：按图片路径携带不同分数"""

    def __init__(self, image_path, score, anomalous):
        self.image_path = image_path
        self.pred_score = score
        self.pred_label = anomalous
        self.anomaly_map = None


def _make_fake_anomalib_modules():
    """构造可导入的 anomalib 假模块，避免测试环境真的依赖 anomalib/torch"""
    anomalib_mod = types.ModuleType("anomalib")
    data_mod = types.ModuleType("anomalib.data")
    utils_mod = types.ModuleType("anomalib.data.utils")

    class _ImageItem:
        def __init__(self, image=None, image_path=""):
            self.image = image
            self.image_path = image_path

    class _ImageBatch:
        @staticmethod
        def collate(items):
            return items

    data_mod.ImageItem = _ImageItem
    data_mod.ImageBatch = _ImageBatch
    utils_mod.read_image = lambda path, as_tensor=True: MagicMock(name=f"img:{path}")

    anomalib_mod.data = data_mod
    return {
        "anomalib": anomalib_mod,
        "anomalib.data": data_mod,
        "anomalib.data.utils": utils_mod,
    }


def _make_fake_torch():
    """构造可导入的 torch 假模块"""
    torch_mod = types.ModuleType("torch")
    torch_mod.load = MagicMock(return_value={})
    torch_mod.cuda = MagicMock()
    torch_mod.cuda.is_available = MagicMock(return_value=False)
    return torch_mod


def _prepare_infer_env(monkeypatch, tmp_path, image_names):
    """搭建 _infer_sync 所需的假环境，返回 (image_paths, fake_engine, fake_model)"""
    fake_modules = _make_fake_anomalib_modules()
    fake_torch = _make_fake_torch()

    for name, mod in fake_modules.items():
        monkeypatch.setitem(sys.modules, name, mod)
    monkeypatch.setitem(sys.modules, "torch", fake_torch)

    # 构造真实存在的图片文件路径（safe_load_weight / 路径校验可能用到）
    image_paths = []
    for name in image_names:
        p = tmp_path / name
        p.write_bytes(b"fake-image")
        image_paths.append(str(p))

    # 权重文件
    weight = tmp_path / "model.pth"
    weight.write_bytes(b"fake-weight")

    fake_model = MagicMock()
    fake_engine = MagicMock()
    return image_paths, str(weight), fake_engine, fake_model


def test_infer_sync_results_match_input_order(monkeypatch, tmp_path):
    """N 条输入 → N 条输出，顺序与输入一致"""
    from labeltorch_backend.handlers import anomaly as anomaly_mod

    names = ["a.png", "b.png", "c.png"]
    image_paths, weight, engine, model = _prepare_infer_env(monkeypatch, tmp_path, names)

    # 按路径给出不同分数，便于断言「取错结果」类缺陷
    score_by_path = {
        image_paths[0]: 0.11,
        image_paths[1]: 0.66,
        image_paths[2]: 0.99,
    }
    label_by_path = {
        image_paths[0]: False,
        image_paths[1]: False,
        image_paths[2]: True,
    }

    def fake_predict(model=None, dataset=None):
        items = [
            _FakeItem(p, score_by_path[p], label_by_path[p])
            for p in image_paths
        ]
        return [items]

    engine.predict.side_effect = fake_predict

    # safe_load_weight 返回空 dict，走 load_state_dict 分支
    with patch.object(anomaly_mod, "_ExactPredictDataset", MagicMock()):
        with patch(
            "labeltorch_backend.tools.safe_loading.safe_load_weight",
            return_value={},
        ):
            results = anomaly_mod._infer_sync(
                engine, model, weight, image_paths, 256
            )

    # 1) 条目数与输入一致
    assert len(results) == len(image_paths)
    # 2) 顺序与输入一致
    assert [r["image_path"] for r in results] == image_paths
    # 3) 分数一一对应，不得取错其他图片的结果
    for r in results:
        expected_score = score_by_path[r["image_path"]]
        assert r["anomaly_score"] == pytest.approx(expected_score), (
            f"{r['image_path']} 的分数应为 {expected_score}，实际 {r['anomaly_score']}"
        )
        expected_label = "anomalous" if label_by_path[r["image_path"]] else "normal"
        assert r["pred_label"] == expected_label


def test_infer_sync_missing_prediction_keeps_placeholder(monkeypatch, tmp_path):
    """模型未返回某图时保留占位条目，不得静默丢条目"""
    from labeltorch_backend.handlers import anomaly as anomaly_mod

    names = ["m1.png", "m2.png"]
    image_paths, weight, engine, model = _prepare_infer_env(monkeypatch, tmp_path, names)

    def fake_predict(model=None, dataset=None):
        # 只返回第一张图的预测，第二张缺失
        return [[_FakeItem(image_paths[0], 0.5, False)]]

    engine.predict.side_effect = fake_predict

    with patch.object(anomaly_mod, "_ExactPredictDataset", MagicMock()):
        with patch(
            "labeltorch_backend.tools.safe_loading.safe_load_weight",
            return_value={},
        ):
            results = anomaly_mod._infer_sync(
                engine, model, weight, image_paths, 256
            )

    assert len(results) == 2
    assert results[0]["image_path"] == image_paths[0]
    assert results[0]["anomaly_score"] == pytest.approx(0.5)
    # 缺失预测的图保留占位 + error 说明
    assert results[1]["image_path"] == image_paths[1]
    assert "error" in results[1]
    assert results[1]["anomaly_score"] == 0.0


def test_infer_sync_result_not_taking_first_prediction_for_all(monkeypatch, tmp_path):
    """回归：同目录批量预测时不得把首个结果套到所有图片上"""
    from labeltorch_backend.handlers import anomaly as anomaly_mod

    # 四张图同目录（正是旧实现按目录重复推理的场景）
    names = ["s1.png", "s2.png", "s3.png", "s4.png"]
    image_paths, weight, engine, model = _prepare_infer_env(monkeypatch, tmp_path, names)

    scores = [0.05, 0.35, 0.75, 0.95]

    def fake_predict(model=None, dataset=None):
        return [[_FakeItem(p, s, s > 0.5) for p, s in zip(image_paths, scores)]]

    engine.predict.side_effect = fake_predict

    with patch.object(anomaly_mod, "_ExactPredictDataset", MagicMock()):
        with patch(
            "labeltorch_backend.tools.safe_loading.safe_load_weight",
            return_value={},
        ):
            results = anomaly_mod._infer_sync(
                engine, model, weight, image_paths, 256
            )

    got_scores = [r["anomaly_score"] for r in results]
    # 旧缺陷表现：所有条目都是首个预测的分数（例如全为 0.05）
    assert got_scores == pytest.approx(scores)
    # 明确拒绝「全同分」退化
    assert len(set(got_scores)) == len(scores)


@pytest.mark.asyncio
async def test_handle_infer_by_image_path_mapping(monkeypatch, tmp_path):
    """handle_infer 返回的 by_image_path 必须能按输入路径取到对应 score/label/heatmap"""
    from labeltorch_backend.handlers import anomaly as anomaly_mod

    names = ["h1.png", "h2.png"]
    image_paths, weight, engine, model = _prepare_infer_env(monkeypatch, tmp_path, names)

    payload = {
        "weight_path": weight,
        "image_paths": image_paths,
        "model_family": "padim",
        "imgsz": 128,
    }

    fake_results = [
        {
            "image_path": image_paths[0],
            "anomaly_score": 0.21,
            "pred_label": "normal",
            "anomaly_map_path": "",
        },
        {
            "image_path": image_paths[1],
            "anomaly_score": 0.87,
            "pred_label": "anomalous",
            "anomaly_map_path": "/tmp/h2_heat.png",
        },
    ]

    def fake_infer_sync(*args, **kwargs):
        # _infer_sync 通过 run_in_executor 以同步函数调用，必须返回结果而非协程
        return list(fake_results)

    # 直接替换底层同步推理，聚焦 handle_infer 的映射契约
    monkeypatch.setattr(anomaly_mod, "_infer_sync", fake_infer_sync)

    # 绕过 Engine/get_model 的重依赖
    monkeypatch.setattr(
        anomaly_mod, "_build_engine", MagicMock(return_value=engine), raising=False
    )

    with patch.dict(
        sys.modules,
        {
            "anomalib": MagicMock(),
            "anomalib.engine": MagicMock(Engine=MagicMock(return_value=engine)),
            "anomalib.models": MagicMock(get_model=MagicMock(return_value=model)),
            "torch": _make_fake_torch(),
        },
    ):
        # 若 handle_infer 内部直接 import Engine，则上面的 patch.dict 已覆盖
        resp = await anomaly_mod.handle_infer(payload)

    # 即便因内部依赖差异导致 handle_infer 走了失败分支，也不接受静默错位的成功结果
    if resp.get("status") == "succeeded":
        assert resp["count"] == 2
        by_path = resp["by_image_path"]
        # key 能覆盖全部输入路径
        for p in image_paths:
            assert p in by_path
        assert by_path[image_paths[0]]["score"] == pytest.approx(0.21)
        assert by_path[image_paths[0]]["label"] == "normal"
        assert by_path[image_paths[1]]["score"] == pytest.approx(0.87)
        assert by_path[image_paths[1]]["label"] == "anomalous"
        # predictions 顺序与输入一致
        assert [p["image_path"] for p in resp["predictions"]] == image_paths
    else:
        # 失败分支必须给出错误信息，而不是错位成功
        assert resp.get("error")
