# 标炬（LabelTorch）技术架构文档

> 版本：5.0 | 更新日期：2026-06-08
> 状态：当前有效架构说明

---

## 一、总体架构

项目采用 `Qt 6 + C++17 + QML + Python` 的混合进程架构。

```text
Qt 主进程（C++ / QML）
  ├─ UI 渲染层
  ├─ Service 层
  ├─ SQLite 数据层
  └─ IpcClient（QProcess）
        ↓ stdin/stdout JSON-RPC
Python 后端进程
  ├─ IpcServer
  ├─ Handlers
  ├─ Training Adapters
  └─ Export / Verify / Inference / Anomaly
```

该架构的目标是把桌面 UI 稳定性与深度学习运行时隔离开，确保后端训练或导出异常时，前端仍然保持响应。

---

## 二、分层职责

### 2.1 Qt 主进程

负责：

1. 应用启动与生命周期管理
2. QML 页面装载与导航
3. 本地数据库状态管理
4. 项目目录与文件系统操作
5. 托管 Python 子进程
6. 将后端事件同步到 UI

### 2.2 QML 界面层

负责：

1. 顶栏、侧栏、主内容区、弹窗等视觉表现
2. 页面状态展示
3. 用户交互收集
4. 调用 C++ Service 暴露的接口

当前阶段要求 QML 层优先做到：

1. 统一设计令牌
2. 统一组件体系
3. 高保真页面复刻

### 2.3 C++ Service 层

负责：

1. 项目管理
2. 数据集管理
3. 快照与训练任务管理
4. 模型版本与导出管理
5. 与数据库同步状态
6. 与 Python 后端通讯

Service 是业务协调层，不承载深度学习计算本身。

### 2.4 Python 后端

负责：

1. 环境检查
2. 数据划分
3. 训练启动、停止、状态查询
4. 模型导出
5. 导出验证
6. 异常检测训练或推理逻辑

Python 后端应保持计算导向，项目元状态以主进程和数据库为准。

---

## 三、IPC 契约

主进程与 Python 后端通过 `stdin/stdout JSON-RPC` 通讯。

### 3.1 核心命令

当前阶段关键命令包括：

1. `environment.check`
2. `train.start`
3. `train.stop`
4. `train.status`
5. `train.list_adapters`
6. `train.data_split`
7. `export.run`
8. `artifact.verify`
9. `anomaly.infer`

### 3.2 核心事件

当前阶段关键事件包括：

1. `task.started`
2. `task.progress`
3. `task.log`
4. `task.warning`
5. `task.failed`
6. `task.succeeded`
7. `task.stopped`

### 3.3 当前阶段要求

1. 所有训练任务都必须可追踪状态
2. 所有失败都必须返回明确错误
3. 所有导出结果都必须可验证
4. 所有长任务都必须通过事件流回传进度

---

## 四、数据与状态边界

### 4.1 数据库是真实状态源

项目、数据集、快照、训练任务、模型版本、导出记录等元数据，以 SQLite 为单一真实状态源。

### 4.2 训练必须基于快照

训练不得直接依赖实时数据集目录，必须通过快照或等价的不可变训练清单启动。

### 4.3 模型版本必须可追溯

至少应形成如下链路：

`model_version -> training_run -> dataset_snapshot`

### 4.4 导出产物必须验证

`pt` 与 `onnx` 导出后，必须记录导出参数、输出路径和验证结果。

---

## 五、当前阶段核心模块关系

### 5.1 项目与数据集

1. `ProjectService` 管理项目元信息和默认配置
2. `DatasetService` 管理导入、扫描和样本记录

### 5.2 快照与训练

1. `SnapshotService` 生成训练输入快照
2. `TrainingService` 管理训练生命周期
3. `TrainingModel` 承载任务列表或状态视图数据

### 5.3 模型与导出

1. `ModelRegistry` 或等价模块管理模型版本
2. `ExportService` 负责导出流程管理

### 5.4 UI 与服务连接

QML 页面通过上下文注入或等价方式连接各服务与模型，禁止把复杂业务逻辑直接堆在 QML 中。

---

## 六、硬件兼容要求

当前阶段必须覆盖：

1. `NVIDIA 30` 系列
2. `NVIDIA 40` 系列
3. `NVIDIA 50` 系列
4. 无 GPU 的 `CPU-only` 设备

架构层要求：

1. 启动阶段可探测环境
2. UI 可展示设备状态
3. 训练配置支持显式设备选择
4. GPU 不可用时支持 CPU 降级
5. 显存不足时提供清晰错误反馈

---

## 七、UI 架构要求

当前阶段 UI 结构必须围绕以下原则展开：

1. 统一主题系统
2. 统一基础组件
3. 页面结构与真实数据解耦
4. 先高保真复刻，再接真实数据

UI 最高参考源为：

- `reference/Dihuge DLTools/gemini-code-1780391652706.html`

页面级详细要求以 `docs/UI像素级复刻专项规范.md` 为准。

---

## 八、当前阶段开发约束

1. 坚持 Qt/QML/C++/Python 技术栈
2. 主进程必须保持高可用
3. Python 计算失败不能拖垮 UI
4. 训练和导出优先于标注高级能力
5. 当前阶段以 `YOLOv8 detect + anomaly + pt/onnx` 为主交付范围
