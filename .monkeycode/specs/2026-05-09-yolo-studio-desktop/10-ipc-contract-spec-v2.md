# IPC 契约规范 v2

## 1. 目标

定义桌面主程序与 Python 后端之间的稳定消息协议，确保进程隔离条件下仍可完成训练、推理、导出、日志流与状态同步。

## 2. 消息类型

1. Request
2. Response
3. Event

## 3. 统一消息结构

### 3.1 Request

1. `request_id`
2. `command`
3. `payload`
4. `timestamp`

### 3.2 Response

1. `request_id`
2. `success`
3. `result`
4. `error`
5. `timestamp`

### 3.3 Event

1. `event_type`
2. `task_id`
3. `payload`
4. `timestamp`

## 4. 必要命令集合

1. `environment.check`
2. `dataset.validate`
3. `train.start`
4. `train.stop`
5. `train.status`
6. `inference.run`
7. `export.run`
8. `artifact.verify`

## 5. 日志流

训练、推理、导出必须支持事件流：

1. `task.started`
2. `task.progress`
3. `task.log`
4. `task.warning`
5. `task.failed`
6. `task.succeeded`

## 6. 错误结构

1. `code`
2. `message`
3. `details`
4. `recoverable`

## 7. 约束

1. IPC 消息必须可序列化为 JSON。
2. 长任务结果不直接返回大对象，应返回路径或引用。
3. UI 不得依赖后端私有字段。
