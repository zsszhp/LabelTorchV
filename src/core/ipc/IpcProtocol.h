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
constexpr const char *CMD_TRAIN_START = "train.start";
constexpr const char *CMD_TRAIN_STOP = "train.stop";
constexpr const char *CMD_TRAIN_STATUS = "train.status";
constexpr const char *CMD_TRAIN_LIST_ADAPTERS = "train.list_adapters";
constexpr const char *CMD_TRAIN_DATA_SPLIT = "train.data_split";
constexpr const char *CMD_INFERENCE_RUN = "inference.run";
constexpr const char *CMD_EXPORT_RUN = "export.run";
constexpr const char *CMD_ARTIFACT_VERIFY = "artifact.verify";
constexpr const char *CMD_ANOMALY_INFER = "anomaly.infer";
constexpr const char *CMD_AL_COLLECT_LOW_CONF = "active_learning.collect_low_conf";
constexpr const char *CMD_AL_PRIORITIZE_QUEUE = "active_learning.prioritize_queue";
constexpr const char *CMD_AL_QUEUE_STATS = "active_learning.queue_stats";
constexpr const char *CMD_TESTING_START = "testing.start";
constexpr const char *CMD_TESTING_STOP = "testing.stop";
constexpr const char *CMD_TESTING_STATUS = "testing.status";
constexpr const char *CMD_SHUTDOWN = "shutdown";               ///< 优雅关闭后端
constexpr const char *CMD_INFERENCE_CANCEL = "inference.cancel"; ///< 取消推理批次
constexpr const char *CMD_ANOMALY_LIST_MODELS = "anomaly.list_models"; ///< 查询异常检测支持的模型列表
constexpr const char *CMD_INFERENCE_RUN_VIDEO = "inference.run_video"; ///< 视频流推理（P2-6）
constexpr const char *CMD_SNAPSHOT_PREVIEW = "snapshot.preview";       ///< 快照预览图生成（P0-2）
constexpr const char *CMD_DATASET_STATS = "dataset.stats";             ///< 数据集统计（P1-4）
constexpr const char *CMD_DATASET_CONVERT_TO_YOLO = "dataset.convert_to_yolo"; ///< 数据集格式转换（P2-5）

// 事件类型
constexpr const char *EVENT_TASK_STARTED = "task.started";
constexpr const char *EVENT_TASK_PROGRESS = "task.progress";
constexpr const char *EVENT_TASK_LOG = "task.log";
constexpr const char *EVENT_TASK_WARNING = "task.warning";
constexpr const char *EVENT_TASK_FAILED = "task.failed";
constexpr const char *EVENT_TASK_SUCCEEDED = "task.succeeded";
constexpr const char *EVENT_TASK_STOPPED = "task.stopped";

// 构建请求消息
QJsonObject createRequest(const QString &requestId, const QString &command, const QJsonObject &payload = {});

// 构建响应消息
QJsonObject createResponse(const QString &requestId, bool success, const QJsonObject &result = {}, const QJsonObject &error = {});

// 构建事件消息
QJsonObject createEvent(const QString &eventType, const QString &taskId, const QJsonObject &payload = {});

} // namespace IpcProtocol

#endif // IPCPROTOCOL_H
