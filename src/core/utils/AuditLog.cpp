#include "AuditLog.h"
#include "database/Database.h"
#include "utils/Id.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>

namespace AuditLog {

bool record(const QString &taskType, const QString &taskId,
            const QString &eventType, const QVariantMap &payload)
{
    if (taskType.isEmpty() || taskId.isEmpty() || eventType.isEmpty()) {
        ltWarning(LT_LOG_DB()) << "record: 缺少必要参数"
                               << "taskType=" << taskType << "eventType=" << eventType;
        return false;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_DB()) << "record: 数据库未打开，审计事件丢失:"
                             << taskType << taskId << eventType;
        return false;
    }

    QString payloadJson;
    if (!payload.isEmpty()) {
        payloadJson = QString::fromUtf8(
            QJsonDocument(QJsonObject::fromVariantMap(payload)).toJson(QJsonDocument::Compact));
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO task_events (id, task_type, task_id, event_type, payload_json) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(Id::generate());
    query.addBindValue(taskType);
    query.addBindValue(taskId);
    query.addBindValue(eventType);
    query.addBindValue(payloadJson);

    if (!query.exec()) {
        ltError(LT_LOG_DB()) << "record: 写入审计事件失败:" << query.lastError().text()
                             << "taskType=" << taskType << "taskId=" << taskId
                             << "eventType=" << eventType;
        return false;
    }

    ltInfo(LT_LOG_DB()) << "Audit event recorded:" << taskType << taskId << eventType;
    return true;
}

QVariantList listEvents(const QString &taskType, const QString &taskId, int limit)
{
    QVariantList result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    if (limit <= 0) limit = 100;

    QSqlQuery query(db);
    QString sql = QStringLiteral(
        "SELECT id, task_type, task_id, event_type, payload_json, created_at "
        "FROM task_events");
    QStringList conditions;
    if (!taskType.isEmpty()) conditions << QStringLiteral("task_type = ?");
    if (!taskId.isEmpty()) conditions << QStringLiteral("task_id = ?");
    if (!conditions.isEmpty()) sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    sql += QStringLiteral(" ORDER BY created_at DESC, rowid DESC LIMIT ?");

    query.prepare(sql);
    if (!taskType.isEmpty()) query.addBindValue(taskType);
    if (!taskId.isEmpty()) query.addBindValue(taskId);
    query.addBindValue(limit);

    if (!query.exec()) {
        ltError(LT_LOG_DB()) << "listEvents: 查询失败:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap item;
        item[QStringLiteral("id")] = query.value(0).toString();
        item[QStringLiteral("taskType")] = query.value(1).toString();
        item[QStringLiteral("taskId")] = query.value(2).toString();
        item[QStringLiteral("eventType")] = query.value(3).toString();

        QString payloadStr = query.value(4).toString();
        if (!payloadStr.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(payloadStr.toUtf8());
            if (doc.isObject()) {
                item[QStringLiteral("payload")] = doc.object().toVariantMap();
            } else {
                item[QStringLiteral("payload")] = payloadStr;
            }
        } else {
            item[QStringLiteral("payload")] = QVariantMap();
        }
        item[QStringLiteral("createdAt")] = query.value(5).toString();
        result.append(item);
    }

    ltDebug(LT_LOG_DB()) << "listEvents: 返回" << result.size() << "条审计事件";
    return result;
}

} // namespace AuditLog
