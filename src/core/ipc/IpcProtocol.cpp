#include "IpcProtocol.h"
#include "utils/Log.h"
#include <QDateTime>

namespace IpcProtocol {

QJsonObject createRequest(const QString &requestId, const QString &command, const QJsonObject &payload)
{
    ltTrace(LT_LOG_IPC()) << "createRequest id=" << requestId << "command=" << command;
    return {
        {"type", REQUEST},
        {"request_id", requestId},
        {"command", command},
        {"payload", payload},
        {"timestamp", QDateTime::currentSecsSinceEpoch()}
    };
}

QJsonObject createResponse(const QString &requestId, bool success, const QJsonObject &result, const QJsonObject &error)
{
    ltTrace(LT_LOG_IPC()) << "createResponse id=" << requestId << "success=" << success;
    return {
        {"type", RESPONSE},
        {"request_id", requestId},
        {"success", success},
        {"result", result},
        {"error", error},
        {"timestamp", QDateTime::currentSecsSinceEpoch()}
    };
}

QJsonObject createEvent(const QString &eventType, const QString &taskId, const QJsonObject &payload)
{
    ltTrace(LT_LOG_IPC()) << "createEvent type=" << eventType << "taskId=" << taskId;
    return {
        {"type", EVENT},
        {"event_type", eventType},
        {"task_id", taskId},
        {"payload", payload},
        {"timestamp", QDateTime::currentSecsSinceEpoch()}
    };
}

} // namespace IpcProtocol
