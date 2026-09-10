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
    global environment, training, inference, export, anomaly, active_learning, testing, snapshot, dataset
    from .handlers import environment as env_module
    from .handlers import training as train_module
    from .handlers import inference as inf_module
    from .handlers import export as exp_module
    from .handlers import anomaly as anomaly_module
    from .handlers import active_learning as al_module
    from .handlers import testing as testing_module
    from .handlers import snapshot as snapshot_module
    from .handlers import dataset as dataset_module
    environment = env_module
    training = train_module
    inference = inf_module
    export = exp_module
    anomaly = anomaly_module
    active_learning = al_module
    testing = testing_module
    snapshot = snapshot_module
    dataset = dataset_module


class IpcServer:
    """JSON-RPC IPC服务端"""

    def __init__(self):
        global _server_instance
        _server_instance = self
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
            "anomaly.list_models": anomaly.handle_list_models,  # A9：模型列表查询
            "active_learning.collect_low_conf": active_learning.handle_collect_low_conf,
            "active_learning.prioritize_queue": active_learning.handle_prioritize_queue,
            "active_learning.queue_stats": active_learning.handle_queue_stats,
            "testing.start": testing.handle_start,
            "testing.stop": testing.handle_stop,
            "testing.status": testing.handle_status,
            "snapshot.preview": snapshot.handle_preview,
            "dataset.stats": dataset.handle_stats,
            "dataset.convert_to_yolo": dataset.handle_convert_to_yolo,
            "inference.run_video": inference.handle_run_video,
            "shutdown": self._handle_shutdown,
        }
        self.running = True
        # A14：活跃任务集合，用于 shutdown 时等待完成
        self._active_tasks = set()

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

        # A14：创建任务并加入活跃集合（shutdown 命令除外）
        if command == "shutdown":
            result = await handler(payload)
            self._send(create_response(request_id, True, result=result, command=command))
            return

        task = asyncio.create_task(self._execute_handler(handler, payload, request_id, command))
        self._active_tasks.add(task)
        task.add_done_callback(self._active_tasks.discard)

    async def _execute_handler(self, handler, payload, request_id, command):
        """A14：执行 handler 并发送响应，异常时返回失败响应"""
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
        """A14：处理关闭命令，等待活跃任务完成（最多 30 秒）"""
        logger.info("Shutdown requested, waiting for active tasks to complete")
        self.running = False

        # 等待活跃任务完成（最多 30 秒）
        if self._active_tasks:
            logger.info(f"Waiting for {len(self._active_tasks)} active task(s) to complete")
            try:
                await asyncio.wait_for(
                    asyncio.gather(*self._active_tasks, return_exceptions=True),
                    timeout=30.0
                )
                logger.info("All active tasks completed")
            except asyncio.TimeoutError:
                # 超时后强制取消剩余任务
                logger.warning(f"Timeout waiting for tasks, cancelling {len(self._active_tasks)} task(s)")
                for task in list(self._active_tasks):
                    task.cancel()
                # 等待取消完成
                if self._active_tasks:
                    await asyncio.gather(*self._active_tasks, return_exceptions=True)
                logger.warning("All pending tasks cancelled")

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
    if _server_instance is None:
        import sys
        main_mod = sys.modules.get('__main__')
        if main_mod and hasattr(main_mod, '_server_instance') and main_mod._server_instance is not None:
            return main_mod._server_instance
        # Also check other loaded names for safety
        for mod_name in ['labeltorch_backend.server', 'server']:
            mod = sys.modules.get(mod_name)
            if mod and hasattr(mod, '_server_instance') and mod._server_instance is not None:
                return mod._server_instance
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
