#include "MetricService.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MetricService::MetricService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_MODEL()) << "parent=" << parent;
}

QVariantMap MetricService::getMetrics(const QString &versionId)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId;

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

    // Extract key metrics if available.
    // For OBB models, Ultralytics reports metrics like "mAP50(OBB)" and "mAP50-95(OBB)".
    // These are handled automatically by the key iteration below (obj.keys()) since
    // class mapping only affects class IDs, not metric key names.
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
    ltTrace(LT_LOG_MODEL()) << "runId=" << runId;

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
        ltError(LT_LOG_MODEL()) << "Failed to get metric history:" << query.lastError().text();
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

    ltDebug(LT_LOG_MODEL()) << "Retrieved" << result.size() << "metric history entries for run:" << runId;
    return result;
}

QVariantList MetricService::compareMultipleVersions(const QVariantList &versionIds)
{
    ltTrace(LT_LOG_MODEL()) << "versionIds.count=" << versionIds.size();

    QVariantList result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    for (const QVariant &vid : versionIds) {
        QString versionId = vid.toString();
        if (versionId.isEmpty()) continue;

        QVariantMap entry;
        entry["versionId"] = versionId;

        // Get metrics for this version
        QVariantMap metrics = getMetrics(versionId);
        entry["metrics"] = metrics;

        // Get snapshot_id and parent_version_id via join with training_runs
        QSqlQuery query(db);
        query.prepare(
            "SELECT tr.snapshot_id, mv.parent_model_version_id "
            "FROM model_versions mv "
            "JOIN training_runs tr ON mv.run_id = tr.id "
            "WHERE mv.id = ?"
        );
        query.addBindValue(versionId);

        if (query.exec() && query.next()) {
            entry["snapshotId"] = query.value(0).toString();
            entry["parentVersionId"] = query.value(1).toString();
        } else {
            entry["snapshotId"] = "";
            entry["parentVersionId"] = "";
        }

        result.append(entry);
    }

    return result;
}

QVariantList MetricService::getVersionsBySnapshot(const QString &snapshotId)
{
    ltTrace(LT_LOG_MODEL()) << "snapshotId=" << snapshotId;

    QVariantList result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT mv.id, mv.run_id, mv.parent_model_version_id, "
        "mv.best_weight_path, mv.last_weight_path, "
        "mv.metrics_snapshot_json, mv.created_at, "
        "tr.snapshot_id "
        "FROM model_versions mv "
        "JOIN training_runs tr ON mv.run_id = tr.id "
        "WHERE tr.snapshot_id = ? "
        "ORDER BY mv.created_at DESC"
    );
    query.addBindValue(snapshotId);

    if (!query.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to get versions by snapshot:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap version;
        version["versionId"] = query.value(0).toString();
        version["runId"] = query.value(1).toString();
        version["parentVersionId"] = query.value(2).toString();
        version["bestWeightPath"] = query.value(3).toString();
        version["lastWeightPath"] = query.value(4).toString();
        version["metricsJson"] = query.value(5).toString();
        version["createdAt"] = query.value(6).toString();
        version["snapshotId"] = query.value(7).toString();

        // Also include parsed metrics
        QVariantMap metrics = getMetrics(version["versionId"].toString());
        version["metrics"] = metrics;

        result.append(version);
    }

    ltDebug(LT_LOG_MODEL()) << "Retrieved" << result.size() << "versions for snapshot:" << snapshotId;
    return result;
}

QVariantMap MetricService::compareVersions(const QString &versionId1, const QString &versionId2)
{
    ltTrace(LT_LOG_MODEL()) << "versionId1=" << versionId1 << "versionId2=" << versionId2;

    QVariantMap result;

    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // Get metrics for both versions
    QVariantMap metrics1 = getMetrics(versionId1);
    QVariantMap metrics2 = getMetrics(versionId2);

    if (metrics1.isEmpty() || metrics2.isEmpty()) {
        ltWarning(LT_LOG_MODEL()) << "One or both versions not found for comparison";
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

    ltInfo(LT_LOG_MODEL()) << "Compared versions:" << versionId1 << "vs" << versionId2;
    return result;
}
