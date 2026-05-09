#include "IpcProtocol.h"
#include <QDateTime>

namespace IpcProtocol {

QJsonObject createRequest(const QString &requestId, const QString &command, const QJsonObject &payload)
{
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
    return {
        {"type", EVENT},
        {"event_type", eventType},
        {"task_id", taskId},
        {"payload", payload},
        {"timestamp", QDateTime::currentSecsSinceEpoch()}
    };
}

} // namespace IpcProtocol
