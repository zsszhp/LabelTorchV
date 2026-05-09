#include "MetricService.h"
#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

MetricService::MetricService(QObject *parent) : QObject(parent) {}

QVariantMap MetricService::getMetrics(const QString &versionId)
{
    QVariantMap result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.prepare("SELECT metrics_snapshot_json FROM model_versions WHERE id = ?");
    query.addBindValue(versionId);

    if (!query.exec() || !query.next()) return result;

    QString metricsJsonStr = query.value(0).toString();
    if (metricsJsonStr.isEmpty()) {
        result["raw"] = "";
        return result;
    }

    QJsonDocument doc = QJsonDocument::fromJson(metricsJsonStr.toUtf8());
    if (!doc.isObject()) {
        result["raw"] = metricsJsonStr;
        return result;
    }

    QJsonObject obj = doc.object();

    // Extract key metrics if available
    const QStringList keyMetrics = {
        "mAP50", "mAP50-95", "precision", "recall", "fitness",
        "map50", "map50-95", "map", "Precision", "Recall", "Fitness"
    };

    for (const QString &key : obj.keys()) {
        result[key] = obj.value(key).toVariant();
    }

    // Ensure common key metrics are accessible with lowercase
    if (obj.contains("mAP50")) result["mAP50"] = obj.value("mAP50").toVariant();
    if (obj.contains("mAP50-95")) result["mAP50-95"] = obj.value("mAP50-95").toVariant();
    if (obj.contains("precision")) result["precision"] = obj.value("precision").toVariant();
    if (obj.contains("recall")) result["recall"] = obj.value("recall").toVariant();
    if (obj.contains("fitness")) result["fitness"] = obj.value("fitness").toVariant();

    result["raw"] = metricsJsonStr;

    return result;
}

QVariantList MetricService::getMetricHistory(const QString &runId)
{
    QVariantList result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // Query task_events for epoch-level metrics logged during training
    QSqlQuery query(db);
    query.prepare(
        "SELECT payload_json FROM task_events "
        "WHERE task_type = 'training' AND task_id = ? AND event_type = 'epoch_complete' "
        "ORDER BY created_at ASC"
    );
    query.addBindValue(runId);

    if (!query.exec()) {
        qWarning() << "Failed to get metric history:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QString payloadStr = query.value(0).toString();
        if (payloadStr.isEmpty()) continue;

        QJsonDocument doc = QJsonDocument::fromJson(payloadStr.toUtf8());
        if (doc.isObject()) {
            result.append(doc.object().toVariantMap());
        }
    }

    return result;
}

QVariantMap MetricService::compareVersions(const QString &versionId1, const QString &versionId2)
{
    QVariantMap result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // Get metrics for both versions
    QVariantMap metrics1 = getMetrics(versionId1);
    QVariantMap metrics2 = getMetrics(versionId2);

    if (metrics1.isEmpty() || metrics2.isEmpty()) {
        result["error"] = "One or both versions not found";
        return result;
    }

    result["version1"] = versionId1;
    result["version2"] = versionId2;
    result["metrics1"] = metrics1;
    result["metrics2"] = metrics2;

    // Build comparison for key metrics
    QVariantMap comparison;
    const QStringList keyMetrics = {"mAP50", "mAP50-95", "precision", "recall", "fitness"};

    for (const QString &metric : keyMetrics) {
        QVariantMap cmp;
        bool hasV1 = metrics1.contains(metric);
        bool hasV2 = metrics2.contains(metric);

        if (hasV1 && hasV2) {
            double val1 = metrics1[metric].toDouble();
            double val2 = metrics2[metric].toDouble();
            cmp["v1"] = val1;
            cmp["v2"] = val2;
            cmp["diff"] = val2 - val1;
            cmp["improved"] = (val2 >= val1);
        } else if (hasV1) {
            cmp["v1"] = metrics1[metric];
            cmp["v2"] = "N/A";
            cmp["diff"] = "N/A";
            cmp["improved"] = false;
        } else if (hasV2) {
            cmp["v1"] = "N/A";
            cmp["v2"] = metrics2[metric];
            cmp["diff"] = "N/A";
            cmp["improved"] = false;
        }
        comparison[metric] = cmp;
    }

    result["comparison"] = comparison;

    return result;
}
