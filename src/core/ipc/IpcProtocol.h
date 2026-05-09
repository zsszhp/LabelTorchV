#ifndef IPCPROTOCOL_H
#define IPCPROTOCOL_H

#include <QString>
#include <QJsonObject>

/**
 * @brief IPC消息协议定义
 *
 * 定义主进程与Python后端之间的JSON-RPC消息结构
 * 参照 10-ipc-contract-spec-v2.md
 */
namespace IpcProtocol {

// 消息类型
constexpr const char *REQUEST = "request";
constexpr const char *RESPONSE = "response";
constexpr const char *EVENT = "event";

// 命令集合
constexpr const char *CMD_ENV_CHECK = "environment.check";
constexpr const char *CMD_DATASET_VALIDATE = "dataset.validate";
constexpr const char *CMD_TRAIN_START = "train.start";
constexpr const char *CMD_TRAIN_STOP = "train.stop";
constexpr const char *CMD_TRAIN_STATUS = "train.status";
constexpr const char *CMD_INFERENCE_RUN = "inference.run";
constexpr const char *CMD_EXPORT_RUN = "export.run";
constexpr const char *CMD_ARTIFACT_VERIFY = "artifact.verify";

// 事件类型
constexpr const char *EVENT_TASK_STARTED = "task.started";
constexpr const char *EVENT_TASK_PROGRESS = "task.progress";
constexpr const char *EVENT_TASK_LOG = "task.log";
constexpr const char *EVENT_TASK_WARNING = "task.warning";
constexpr const char *EVENT_TASK_FAILED = "task.failed";
constexpr const char *EVENT_TASK_SUCCEEDED = "task.succeeded";

// 构建请求消息
QJsonObject createRequest(const QString &requestId, const QString &command, const QJsonObject &payload = {});

// 构建响应消息
QJsonObject createResponse(const QString &requestId, bool success, const QJsonObject &result = {}, const QJsonObject &error = {});

// 构建事件消息
QJsonObject createEvent(const QString &eventType, const QString &taskId, const QJsonObject &payload = {});

} // namespace IpcProtocol

#endif // IPCPROTOCOL_H
