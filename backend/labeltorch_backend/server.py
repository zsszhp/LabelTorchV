"""
IPC服务端主循环

通过stdin/stdout JSON-RPC与Qt主进程通信
"""
import sys
import asyncio
import json
import logging
import platform

from .protocol import create_response, create_event
from .handlers import environment, training, inference, export

logger = logging.getLogger(__name__)


class IpcServer:
    """JSON-RPC IPC服务端"""

    def __init__(self):
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
            "shutdown": self._handle_shutdown,
        }
        self.running = True

    async def start(self):
        """启动服务端主循环"""
        logger.info("LabelTorch Python backend started")

        if platform.system() == "Windows":
            loop = asyncio.get_event_loop()
            reader = asyncio.StreamReader()
            protocol = asyncio.StreamReaderProtocol(reader)
            try:
                await loop.connect_read_pipe(lambda: protocol, sys.stdin)
            except Exception as e:
                logger.warning(f"connect_read_pipe failed: {e}, falling back to thread reader")
                asyncio.create_task(self._threaded_stdin_reader(reader))
        else:
            reader = asyncio.StreamReader()
            protocol = asyncio.StreamReaderProtocol(reader)
            await asyncio.get_event_loop().connect_read_pipe(
                lambda: protocol, sys.stdin
            )

        while self.running:
            try:
                line = await asyncio.wait_for(reader.readline(), timeout=30.0)
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

    async def _threaded_stdin_reader(self, reader: asyncio.StreamReader):
        """Windows fallback: read stdin in a thread and feed into StreamReader"""
        loop = asyncio.get_event_loop()
        while self.running:
            try:
                line = await loop.run_in_executor(None, sys.stdin.readline)
                if not line:
                    reader.feed_eof()
                    break
                reader.feed_data(line.encode("utf-8"))
            except Exception as e:
                logger.error(f"Stdin read error: {e}")
                reader.feed_eof()
                break

    async def _handle_message(self, message: dict):
        """处理收到的IPC消息"""
        msg_type = message.get("type", "")
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

    if platform.system() == "Windows":
        asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

    server = IpcServer()
    _server_instance = server
    asyncio.run(server.start())


if __name__ == "__main__":
    main()
