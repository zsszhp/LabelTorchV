"""路径白名单工具测试（P0-19）"""
import os
import sys

import pytest

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))

from labeltorch_backend.tools.path_security import (
    PathSecurityError,
    collect_allowed_roots,
    ensure_within,
    require_path_within,
)


def test_require_path_within_accepts_child(tmp_path):
    """白名单根内的子路径应通过"""
    root = tmp_path / "proj"
    root.mkdir()
    target = root / "models" / "a.pt"
    target.parent.mkdir()
    target.write_bytes(b"x")

    resolved = require_path_within(str(target), [str(root)])
    assert os.path.samefile(resolved, str(target))


def test_require_path_within_rejects_outside(tmp_path):
    """白名单外路径必须拒绝"""
    root = tmp_path / "proj"
    root.mkdir()
    outside = tmp_path / "evil.pt"
    outside.write_bytes(b"x")

    with pytest.raises(PathSecurityError) as exc:
        require_path_within(str(outside), [str(root)])
    assert exc.value.code == "PATH_DENIED"


def test_require_path_within_rejects_traversal(tmp_path):
    """相对段 ../ 逃逸必须在 realpath 后被拒绝"""
    root = tmp_path / "proj"
    (root / "sub").mkdir(parents=True)
    outside = tmp_path / "secret.txt"
    outside.write_text("s", encoding="utf-8")

    traversal = str(root / "sub" / ".." / ".." / "secret.txt")
    with pytest.raises(PathSecurityError):
        require_path_within(traversal, [str(root)])


def test_require_path_within_rejects_empty():
    with pytest.raises(PathSecurityError) as exc:
        require_path_within("", [os.getcwd()])
    assert exc.value.code == "PATH_EMPTY"


def test_require_path_within_requires_suffix_via_safe_loading(tmp_path):
    """权重加载拒绝非法扩展名"""
    from labeltorch_backend.tools.safe_loading import _check_weight_path

    root = tmp_path / "proj"
    root.mkdir()
    bad = root / "evil.pkl"
    bad.write_bytes(b"x")

    with pytest.raises(PathSecurityError) as exc:
        _check_weight_path(str(bad), [str(root)])
    assert exc.value.code == "WEIGHT_BAD_SUFFIX"


def test_collect_allowed_roots_discovers_project_marker(tmp_path):
    """从子路径向上发现 project.json 应加入白名单"""
    root = tmp_path / "proj"
    (root / "models").mkdir(parents=True)
    (root / "project.json").write_text("{}", encoding="utf-8")
    weight = root / "models" / "m.pt"
    weight.write_bytes(b"x")

    roots = collect_allowed_roots({"weight_path": str(weight)})
    assert os.path.samefile(str(root), str(root))
    # 项目根必须出现在白名单
    assert any(os.path.samefile(r, str(root)) for r in roots)


def test_collect_allowed_roots_includes_temp(tmp_path):
    """系统临时目录始终在白名单内"""
    import tempfile

    roots = collect_allowed_roots({})
    assert any(os.path.samefile(r, tempfile.gettempdir()) for r in roots)


def test_ensure_within_returns_structured_error(tmp_path):
    """越界时返回结构化错误 dict，不抛裸异常"""
    root = tmp_path / "proj"
    root.mkdir()
    outside = tmp_path / "evil.pt"
    outside.write_bytes(b"x")

    err = ensure_within(str(outside), [str(root)], field="weight_path")
    assert isinstance(err, dict)
    assert err["status"] == "failed"
    assert err["error"]["code"] == "PATH_DENIED"
    assert "weight_path" in err["error"]["message"]
