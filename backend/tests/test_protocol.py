"""协议测试"""
import sys
sys.path.insert(0, ".")

from labeltorch_backend.protocol import create_request, create_response, create_event


def test_create_request():
    req = create_request("req_1", "environment.check", {"key": "val"})
    assert req["type"] == "request"
    assert req["request_id"] == "req_1"
    assert req["command"] == "environment.check"
    assert req["payload"]["key"] == "val"
    assert "timestamp" in req


def test_create_response():
    resp = create_response("req_1", True, result={"status": "ok"})
    assert resp["type"] == "response"
    assert resp["request_id"] == "req_1"
    assert resp["success"] is True
    assert resp["result"]["status"] == "ok"


def test_create_event():
    evt = create_event("task.started", "task_1", {"epoch": 1})
    assert evt["type"] == "event"
    assert evt["event_type"] == "task.started"
    assert evt["task_id"] == "task_1"


if __name__ == "__main__":
    test_create_request()
    test_create_response()
    test_create_event()
    print("All protocol tests passed!")
