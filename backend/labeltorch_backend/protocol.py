"""
JSON-RPC协议编解码
"""
import time


def create_request(request_id: str, command: str, payload: dict = None) -> dict:
    return {
        "type": "request",
        "request_id": request_id,
        "command": command,
        "payload": payload or {},
        "timestamp": time.time(),
    }


def create_response(request_id: str, success: bool, result: dict = None, error: dict = None) -> dict:
    return {
        "type": "response",
        "request_id": request_id,
        "success": success,
        "result": result or {},
        "error": error or {},
        "timestamp": time.time(),
    }


def create_event(event_type: str, task_id: str, payload: dict = None) -> dict:
    return {
        "type": "event",
        "event_type": event_type,
        "task_id": task_id,
        "payload": payload or {},
        "timestamp": time.time(),
    }
