#include "ModelRegistry.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>

ModelRegistry::ModelRegistry(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_MODEL()) << "parent=" << parent;
}

QString ModelRegistry::registerModelVersion(const QString &runId,
                                              const QString &bestWeightPath,
                                              const QString &lastWeightPath,
                                              const QString &metricsJson)
{
    ltTrace(LT_LOG_MODEL()) << "runId=" << runId << "bestWeightPath=" << bestWeightPath << "lastWeightPath=" << lastWeightPath;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate metricsJson is valid JSON (or empty)
    if (!metricsJson.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument::fromJson(metricsJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            ltWarning(LT_LOG_MODEL()) << "Invalid metrics JSON:" << parseError.errorString();
            return {};
        }
    }

    QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO model_versions (id, run_id, best_weight_path, last_weight_path, metrics_snapshot_json) "
        "VALUES (?, ?, ?, ?, ?)"
    );
    query.addBindValue(versionId);
    query.addBindValue(runId);
    query.addBindValue(bestWeightPath);
    query.addBindValue(lastWeightPath);
    query.addBindValue(metricsJson.isEmpty() ? "{}" : metricsJson);

    if (!query.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to register model version:" << query.lastError().text();
        return {};
    }

    ltInfo(LT_LOG_MODEL()) << "Registered model version:" << versionId << "for run:" << runId;
    return versionId;
}

QVariantList ModelRegistry::listModelVersions(const QString &projectId)
{
    ltTrace(LT_LOG_MODEL()) << "projectId=" << projectId;

    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT mv.id, mv.run_id, mv.parent_model_version_id, "
        "mv.best_weight_path, mv.last_weight_path, "
        "mv.metrics_snapshot_json, mv.export_registry_json, mv.created_at "
        "FROM model_versions mv "
        "JOIN training_runs tr ON mv.run_id = tr.id "
        "WHERE tr.project_id = ? "
        "ORDER BY mv.created_at DESC"
    );
    query.addBindValue(projectId);

    if (!query.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to list model versions:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap version;
        version["id"] = query.value(0).toString();
        version["runId"] = query.value(1).toString();
        version["parentVersionId"] = query.value(2).toString();
        version["bestWeightPath"] = query.value(3).toString();
        version["lastWeightPath"] = query.value(4).toString();
        version["metricsJson"] = query.value(5).toString();
        version["exportRegistryJson"] = query.value(6).toString();
        version["createdAt"] = query.value(7).toString();
        result.append(version);
    }

    ltDebug(LT_LOG_MODEL()) << "Listed" << result.size() << "model versions for project:" << projectId;
    return result;
}

QVariantMap ModelRegistry::getModelVersion(const QString &versionId)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId;

    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT mv.id, mv.run_id, mv.parent_model_version_id, "
        "mv.best_weight_path, mv.last_weight_path, "
        "mv.metrics_snapshot_json, mv.export_registry_json, mv.created_at "
        "FROM model_versions mv "
        "WHERE mv.id = ?"
    );
    query.addBindValue(versionId);

    if (!query.exec() || !query.next()) return result;

    result["id"] = query.value(0).toString();
    result["runId"] = query.value(1).toString();
    result["parentVersionId"] = query.value(2).toString();
    result["bestWeightPath"] = query.value(3).toString();
    result["lastWeightPath"] = query.value(4).toString();
    result["metricsJson"] = query.value(5).toString();
    result["exportRegistryJson"] = query.value(6).toString();
    result["createdAt"] = query.value(7).toString();

    return result;
}

bool ModelRegistry::deleteModelVersion(const QString &versionId)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Check if any exports reference this version
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM export_artifacts WHERE model_version_id = ?");
    checkQuery.addBindValue(versionId);
    if (!checkQuery.exec() || !checkQuery.next()) return false;

    int exportCount = checkQuery.value(0).toInt();
    if (exportCount > 0) {
        ltWarning(LT_LOG_MODEL()) << "Cannot delete model version:" << exportCount << "exports reference it";
        return false;
    }

    // Also check assisted_label_batches
    QSqlQuery checkAssist(db);
    checkAssist.prepare("SELECT COUNT(*) FROM assisted_label_batches WHERE model_version_id = ?");
    checkAssist.addBindValue(versionId);
    if (checkAssist.exec() && checkAssist.next()) {
        int assistCount = checkAssist.value(0).toInt();
        if (assistCount > 0) {
            ltWarning(LT_LOG_MODEL()) << "Cannot delete model version:" << assistCount << "assisted label batches reference it";
            return false;
        }
    }

    // Clear any child versions' parent reference
    QSqlQuery clearParent(db);
    clearParent.prepare("UPDATE model_versions SET parent_model_version_id = NULL WHERE parent_model_version_id = ?");
    clearParent.addBindValue(versionId);
    clearParent.exec();

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM model_versions WHERE id = ?");
    deleteQuery.addBindValue(versionId);

    if (deleteQuery.exec()) {
        ltInfo(LT_LOG_MODEL()) << "Deleted model version:" << versionId;
        return true;
    }
    return false;
}

bool ModelRegistry::setParentVersion(const QString &versionId, const QString &parentVersionId)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId << "parentVersionId=" << parentVersionId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Validate parent version exists
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT id FROM model_versions WHERE id = ?");
    checkQuery.addBindValue(parentVersionId);
    if (!checkQuery.exec() || !checkQuery.next()) {
        ltWarning(LT_LOG_MODEL()) << "Parent version not found:" << parentVersionId;
        return false;
    }

    // Prevent circular: a version cannot be its own parent
    if (versionId == parentVersionId) {
        ltWarning(LT_LOG_MODEL()) << "Cannot set a version as its own parent";
        return false;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE model_versions SET parent_model_version_id = ? WHERE id = ?");
    query.addBindValue(parentVersionId);
    query.addBindValue(versionId);

    if (!query.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to set parent version:" << query.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_MODEL()) << "Set parent version:" << versionId << "->" << parentVersionId;
    return true;
}

bool ModelRegistry::setTag(const QString &versionId, const QString &tag)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId << "tag=" << tag;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Get current metrics_snapshot_json
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT metrics_snapshot_json FROM model_versions WHERE id = ?");
    getQuery.addBindValue(versionId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString metricsJsonStr = getQuery.value(0).toString();

    QJsonObject metricsObj;
    if (!metricsJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(metricsJsonStr.toUtf8());
        if (doc.isObject()) {
            metricsObj = doc.object();
        }
    }

    // Get or create tags array
    QJsonArray tagsArray = metricsObj.value("tags").toArray();

    // Don't add duplicate tags
    for (const auto &t : tagsArray) {
        if (t.toString() == tag) return true;
    }

    tagsArray.append(tag);
    metricsObj["tags"] = tagsArray;

    QString updatedJson = QString::fromUtf8(
        QJsonDocument(metricsObj).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE model_versions SET metrics_snapshot_json = ? WHERE id = ?");
    updateQuery.addBindValue(updatedJson);
    updateQuery.addBindValue(versionId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to set tag:" << updateQuery.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_MODEL()) << "Set tag:" << tag << "on version:" << versionId;
    return true;
}

bool ModelRegistry::removeTag(const QString &versionId, const QString &tag)
{
    ltTrace(LT_LOG_MODEL()) << "versionId=" << versionId << "tag=" << tag;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Get current metrics_snapshot_json
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT metrics_snapshot_json FROM model_versions WHERE id = ?");
    getQuery.addBindValue(versionId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString metricsJsonStr = getQuery.value(0).toString();

    QJsonObject metricsObj;
    if (!metricsJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(metricsJsonStr.toUtf8());
        if (doc.isObject()) {
            metricsObj = doc.object();
        }
    }

    QJsonArray tagsArray = metricsObj.value("tags").toArray();
    QJsonArray newArray;
    bool found = false;

    for (const auto &t : tagsArray) {
        if (t.toString() != tag) {
            newArray.append(t);
        } else {
            found = true;
        }
    }

    if (!found) return true; // Tag wasn't there, nothing to do

    metricsObj["tags"] = newArray;

    QString updatedJson = QString::fromUtf8(
        QJsonDocument(metricsObj).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE model_versions SET metrics_snapshot_json = ? WHERE id = ?");
    updateQuery.addBindValue(updatedJson);
    updateQuery.addBindValue(versionId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_MODEL()) << "Failed to remove tag:" << updateQuery.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_MODEL()) << "Removed tag:" << tag << "from version:" << versionId;
    return true;
}
