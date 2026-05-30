---
description: LabelTorchV 项目根规则（6段式）
globs:
alwaysApply: true
---

# LabelTorchV 项目规范

## 1. Project Overview

标炬（LabelTorch）- 面向工业缺陷检测的本地离线视觉数据治理与模型闭环平台。

- **核心工作流**：导入数据集 → 数据快照 → 训练 → 推理/辅助标注 → 导出模型（pt/onnx）
- **支持任务类型**：detect / obb / classify / anomaly
- **技术栈**：Qt 6.11 + QML + C++17 + Python 3.11 + Ultralytics
- **详细架构**：参见 `CLAUDE.md`

## 2. Commands

### 构建命令
- **配置**：`cmake --preset x64-release`
- **构建**：`cmake --build --preset x64-release`
- **Debug 配置**：`cmake --preset x64-debug`
- **Debug 构建**：`cmake --build --preset x64-debug`

### 测试命令
- **C++ 测试**：`ctest --preset x64-release`
- **Python 测试**：`cd backend ; python -m pytest tests/`
- **Python Lint**：`cd backend ; python -m ruff check .`

### 环境路径
- **Qt**：`C:/Qt/6.11.1/msvc2022_64`
- **Python**：`C:/A/anaconda/envs/labeltorch/python.exe`
- **数据库**：`QStandardPaths::AppDataLocation/labeltorch.db`

## 3. Architecture

### 项目结构
- **Core**：`src/core/` - 基础设施（数据库、IPC、日志、缓存）
- **Features**：`src/features/` - 业务功能模块（项目、数据集、标注、训练、模型、推理、导出）
- **Shell**：`src/shell/` - QML 界面层
- **Backend**：`backend/` - Python 后端（训练适配器、IPC 服务）
- **Tests**：`tests/` - C++ 测试

### 架构模式
- **Service 层**：业务逻辑封装为 Service 类，通过 `setContextProperty()` 注入 QML
- **IPC 通信**：QProcess 管理 Python 后端，JSON-RPC 协议
- **数据库**：SQLite 单例，WAL 模式，14 张核心表
- **UI 层**：QML + Theme.qml 设计系统

## 4. Conventions

### 代码规范
- **C++/Python/QML 必须加注释**（除非用户明确要求不加）
- **界面语言仅中文**，禁止占位符
- **文件命名**：C++ PascalCase / QML PascalCase / Python snake_case
- **C++**：禁止裸 new/delete，用 QObject 父子树/智能指针
- **C++**：日志用 `ltInfo(LT_LOG_XXX())`，不用 qDebug()
- **C++**：所有 ID 用 UUID（`Id::generate()`），Service 间依赖用 `setXxx()` 注入
- **QML**：颜色/字体/间距/圆角必须用 `Theme.xxx`，禁止硬编码
- **Python**：async/await 模式，Handler 签名 `async def handle_xxx(payload: dict) -> dict`

### 开发规范
- **工作前后必须 git pull**：动手前 `git pull origin main` 同步云端解决冲突，工作结束后再次 pull 确保同步
- **编译运行→用户测试→确认后提交**：每次修改后必须编译运行启动应用，等待用户手动测试确认无误后才能提交 git
- **细粒度提交**：每完成一个小目标就提交一次，多提交没关系，每次 commit 必须有详细中文描述
- **业务参数配置驱动**：训练超参/阈值/分页大小等通过 AppSettings 或配置文件管理，禁止源码硬编码
- **原子写入**：文件写入必须先写临时文件再 rename（防止崩溃半写损坏）
- **数据库事务**：多表关联操作必须在同一事务内完成
- **主线程保护**：禁止在 UI 主线程执行耗时操作（>16ms），耗时操作必须用 QtConcurrent/QThreadPool
- **修 Bug 必须先写失败测试**：先写能复现 Bug 的测试用例，确认测试失败后再修复

## 5. Hard Constraints（绝不能违反）

1. **坚持 Qt+C++ + Python 混合架构**，不使用纯 Python 方案（项目定位是桌面工业软件）
2. **训练永远依赖数据快照**，不直接依赖数据集实时状态（数据一致性保障）
3. **模型版本必须可追溯**到训练任务和数据快照（工业审计要求）
4. **不可逆操作必须可审计**（`annotation_revisions` / `task_events`）
5. **离线优先**，不依赖云端服务（工业现场可能无网络）
6. **IPC 进程隔离**：Python 崩溃不影响主进程（稳定性保障）
7. **数据快照不可变**：创建后不可修改
8. **标注修订追踪**：每次保存自动创建修订记录
9. **导出产物验证**：必须经过 `artifact.verify` 验证
10. **QObject 线程安全**：QObject 必须在创建它的线程中使用，跨线程用信号槽

## 6. Gotchas

- **Qt 6.11 NaN 问题**：Debug 模式下 NaN 会触发 qFatal，main.cpp 已有降级处理，不要删除
- **QML 模块输出目录**：启用 QTP0004 NEW 策略，不要手动修改
- **Python 可选依赖**：Anomalib 为可选依赖，未安装时自动跳过注册
- **IPC 超时**：所有 IPC 请求必须设置超时（默认 30 秒），超时请求从 pendingCommands 移除
- **Python 自动重启**：Python 崩溃后 IpcClient 自动重启（最多 5 次），重启后发 `environment.check` 确认就绪

## 7. 必须记录日志的操作

- **项目管理**：项目创建/删除/打开
- **数据集**：导入开始/完成/失败
- **训练**：训练启动/停止/完成/失败
- **模型**：模型导出开始/完成/失败
- **IPC**：连接建立/断开/重连
- **数据库**：迁移执行、写入失败
- **文件系统**：文件写入失败
