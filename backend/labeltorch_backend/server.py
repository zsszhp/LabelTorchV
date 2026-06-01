"""
IPC服务端主循环

通过stdin/stdout JSON-RPC与Qt主进程通信
"""
import sys
import asyncio
import json
import logging

from .protocol import create_response, create_event

logger = logging.getLogger(__name__)

# 延迟导入以避免循环依赖
environment = None
training = None
inference = None
export = None


def _import_handlers():
    """延迟导入处理器模块"""
    global environment, training, inference, export, anomaly, active_learning
    from .handlers import environment as env_module
    from .handlers import training as train_module
    from .handlers import inference as inf_module
    from .handlers import export as exp_module
    from .handlers import anomaly as anomaly_module
    from .handlers import active_learning as al_module
    environment = env_module
    training = train_module
    inference = inf_module
    export = exp_module
    anomaly = anomaly_module
    active_learning = al_module


class IpcServer:
    """JSON-RPC IPC服务端"""

    def __init__(self):
        _import_handlers()
        # 启动时注册所有内置训练适配器（仅注册一次）
        from .adapters.registry import register_builtin_adapters
        register_builtin_adapters()
        self.handlers = {
            "environment.check": environment.handle_check,
            "train.start": training.handle_start,
            "train.stop": training.handle_stop,
            "train.status": training.handle_status,
            "train.list_adapters": training.handle_list_adapters,
            "train.data_split": training.handle_data_split,
            "inference.run": inference.handle_run,
            "export.run": export.handle_run,
            "artifact.verify": export.handle_verify,
            "anomaly.infer": anomaly.handle_infer,
            "active_learning.collect_low_conf": active_learning.handle_collect_low_conf,
            "active_learning.prioritize_queue": active_learning.handle_prioritize_queue,
            "active_learning.queue_stats": active_learning.handle_queue_stats,
            "shutdown": self._handle_shutdown,
        }
        self.running = True

    async def start(self):
        """启动服务端主循环"""
        logger.info("LabelTorch Python backend started")

        # Create an asyncio queue to hold lines read from stdin
        queue = asyncio.Queue()
        loop = asyncio.get_event_loop()

        def read_stdin():
            while self.running:
                try:
                    line = sys.stdin.readline()
                    if not line:
                        # EOF reached
                        loop.call_soon_threadsafe(queue.put_nowait, b"")
                        break
                    # Put line as bytes into the queue
                    loop.call_soon_threadsafe(queue.put_nowait, line.encode('utf-8'))
                except Exception as e:
                    logger.error(f"Error reading from stdin: {e}")
                    loop.call_soon_threadsafe(queue.put_nowait, b"")
                    break

        # Start the background thread
        import threading
        stdin_thread = threading.Thread(target=read_stdin, daemon=True)
        stdin_thread.start()

        while self.running:
            try:
                line = await asyncio.wait_for(queue.get(), timeout=30.0)
                if not line:
                    break

                message = json.loads(line.decode("utf-8").strip())
                await self._handle_message(message)

            except asyncio.TimeoutError:
                continue
            except json.JSONDecodeError as e:
                logger.error(f"JSON parse error: {e}")
            except Exception as e:
                logger.error(f"Error handling message: {e}")

        logger.info("LabelTorch Python backend shutting down")

    async def _handle_message(self, message: dict):
        """处理收到的IPC消息"""
        request_id = message.get("request_id", "")
        command = message.get("command", "")
        payload = message.get("payload", {})

        handler = self.handlers.get(command)
        if handler is None:
            response = create_response(
                request_id, False,
                error={"code": "UNKNOWN_COMMAND", "message": f"Unknown command: {command}"},
                command=command
            )
            self._send(response)
            return

        try:
            result = await handler(payload)
            # 检查handler返回的status字段，将业务层错误转换为IPC层失败响应
            if isinstance(result, dict) and result.get("status") == "failed":
                response = create_response(
                    request_id, False,
                    error={"code": "HANDLER_ERROR", "message": result.get("error", "Unknown error"), "recoverable": True},
                    command=command
                )
            else:
                response = create_response(request_id, True, result=result, command=command)
            self._send(response)
        except Exception as e:
            logger.error(f"Handler error for {command}: {e}")
            response = create_response(
                request_id, False,
                error={"code": "HANDLER_ERROR", "message": str(e), "recoverable": True},
                command=command
            )
            self._send(response)

    async def _handle_shutdown(self, payload: dict):
        """处理关闭命令"""
        self.running = False
        return {"status": "shutting_down"}

    def _send(self, message: dict):
        """发送消息到stdout"""
        sys.stdout.write(json.dumps(message, ensure_ascii=False) + "\n")
        sys.stdout.flush()

    def send_event(self, event_type: str, task_id: str, payload: dict = None):
        """发送事件到Qt前端"""
        event = create_event(event_type, task_id, payload)
        self._send(event)


_server_instance = None


def get_server():
    global _server_instance
    return _server_instance


def main():
    """入口点"""
    global _server_instance

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(name)s] %(levelname)s: %(message)s",
        stream=sys.stderr,
    )

    server = IpcServer()
    _server_instance = server
    asyncio.run(server.start())


if __name__ == "__main__":
    main()
