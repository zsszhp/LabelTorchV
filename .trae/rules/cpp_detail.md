---
description: C++ 详细规范
globs: ["**/*.cpp", "**/*.h", "**/*.hpp", "**/CMakeLists.txt"]
alwaysApply: false
---

# C++ 详细规范

## 内存管理
- QObject派生类：用QObject父子树（new Xxx(parent)），父对象析构时自动释放
- 非QObject对象：用std::unique_ptr / QScopedPointer
- 共享所有权：用std::shared_ptr / QSharedPointer
- QML创建的对象：由QML引擎管理，不要手动delete
- 文件句柄用RAII（QFile析构自动关闭）
- 数据库连接通过Database单例管理，不手动开关

## 日志分类宏
LT_LOG_CORE / LT_LOG_DB / LT_LOG_FS / LT_LOG_IPC / LT_LOG_PROJECT
LT_LOG_TAXONOMY / LT_LOG_DATASET / LT_LOG_ANNOTATION / LT_LOG_TRAINING
LT_LOG_MODEL / LT_LOG_INFERENCE / LT_LOG_EXPORT / LT_LOG_APP

## 日志级别
- ltDebug：开发调试，Release不输出
- ltInfo：关键操作记录（项目创建、训练启动、导出完成）
- ltWarning：可恢复异常（配置缺失用默认值、文件不存在跳过）
- ltError：操作失败（IPC超时、数据库写入失败、文件IO错误）

## 必须记录日志的操作
项目创建/删除/打开、数据集导入开始/完成/失败、训练启动/停止/完成/失败
模型导出开始/完成/失败、IPC连接建立/断开/重连、数据库迁移执行、文件写入失败
