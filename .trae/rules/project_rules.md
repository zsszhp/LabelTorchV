# LabelTorchV 项目规范

## 项目概述

标炬（LabelTorch）- 面向工业缺陷检测的本地离线视觉数据治理与模型闭环平台。

## 技术栈

- 前端：Qt 6.11 + QML + C++17
- 后端：Python 3.11 + Ultralytics
- IPC：stdin/stdout JSON-RPC
- 数据库：SQLite 3（WAL 模式）
- 构建：CMake 3.21+ / Ninja / MSVC 2022
- CUDA：12.1

## 代码规范

- C++ 代码必须加注释（除非用户要求）
- Python 代码必须加注释（除非用户要求）
- QML 代码必须加注释（除非用户要求）
- 界面语言：仅中文
- 提交信息：详细中文注释

## 构建命令

```bash
cmake --preset msvc2022-release
cmake --build --preset msvc2022-release
```

## 测试命令

```bash
# C++ 测试
ctest --preset msvc2022-release

# Python 测试
cd backend && python -m pytest tests/
```

## Lint 命令

```bash
# Python lint
cd backend && python -m ruff check .
```

## Git 规范

- 每次修改必须提交并推送到双平台（GitHub + Gitee）
- 提交说明使用详细中文注释
- 版本发布使用 git tag + release
- tag 格式：v{major}.{minor}.{patch}，如 v0.1.0

## 版本号规范

- 主版本号：重大架构变更或里程碑
- 次版本号：新增功能
- 修订号：Bug 修复

## 架构约束

- 坚持Qt+C++ + Python混合架构，不使用纯Python方案
- 训练永远依赖数据快照，不直接依赖数据集实时状态
- 模型版本必须可追溯到训练任务和数据快照
- 不可逆操作必须可审计
- 离线优先，不依赖云端服务

## MVP 边界

MVP 仅包含：导入数据集（图片+txt标签）→ 训练 → 导出模型（pt/onnx）
MVP 不包含：标注功能、辅助标注、数据治理、多模型支持

## 文件路径

- Qt 安装路径：F:/A/QT/6.11.0/msvc2022_64
- Python 环境：conda labeltorch
- 项目数据库：QStandardPaths::AppDataLocation
