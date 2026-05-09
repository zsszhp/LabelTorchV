#include "InferenceService.h"
#include "Database.h"
#include "ipc/IpcClient.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

InferenceService::InferenceService(QObject *parent) : QObject(parent) {}

void InferenceService::setIpcClient(IpcClient *client)
{
    m_ipcClient = client;
}

QString InferenceService::runInference(const QString &modelVersionId,
                                        const QString &datasetId,
                                        const QString &sampleScope,
                                        double confThreshold,
                                        double iouThreshold)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate model version exists
    QSqlQuery checkVersion(db);
    checkVersion.prepare("SELECT id FROM model_versions WHERE id = ?");
    checkVersion.addBindValue(modelVersionId);
    if (!checkVersion.exec() || !checkVersion.next()) {
        qWarning() << "Model version not found:" << modelVersionId;
        return {};
    }

    // Validate dataset exists
    QSqlQuery checkDataset(db);
    checkDataset.prepare("SELECT id FROM datasets WHERE id = ?");
    checkDataset.addBindValue(datasetId);
    if (!checkDataset.exec() || !checkDataset.next()) {
        qWarning() << "Dataset not found:" << datasetId;
        return {};
    }

    QString batchId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Build initial candidate_snapshot_json with status metadata
    QJsonObject snapshotMeta;
    snapshotMeta["status"] = "pending";
    snapshotMeta["candidates"] = QJsonArray();
    QString snapshotJson = QString::fromUtf8(
        QJsonDocument(snapshotMeta).toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO assisted_label_batches "
        "(id, model_version_id, dataset_id, target_sample_scope, conf_threshold, iou_threshold, candidate_snapshot_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    query.addBindValue(batchId);
    query.addBindValue(modelVersionId);
    query.addBindValue(datasetId);
    query.addBindValue(sampleScope);
    query.addBindValue(confThreshold);
    query.addBindValue(iouThreshold);
    query.addBindValue(snapshotJson);

    if (!query.exec()) {
        qWarning() << "Failed to create inference batch:" << query.lastError().text();
        return {};
    }

    // Send inference.run via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["batch_id"] = batchId;
        payload["model_version_id"] = modelVersionId;
        payload["dataset_id"] = datasetId;
        payload["sample_scope"] = sampleScope;
        payload["conf_threshold"] = confThreshold;
        payload["iou_threshold"] = iouThreshold;
        m_ipcClient->sendRequest("inference.run", payload);
    }

    emit batchStatusChanged(batchId, "pending");
    return batchId;
}

QVariantMap InferenceService::getBatchStatus(const QString &batchId)
{
    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, dataset_id, target_sample_scope, "
        "conf_threshold, iou_threshold, candidate_snapshot_json, created_at "
        "FROM assisted_label_batches WHERE id = ?"
    );
    query.addBindValue(batchId);

    if (!query.exec() || !query.next()) return result;

    result["id"] = query.value(0).toString();
    result["modelVersionId"] = query.value(1).toString();
    result["datasetId"] = query.value(2).toString();
    result["targetSampleScope"] = query.value(3).toString();
    result["confThreshold"] = query.value(4).toDouble();
    result["iouThreshold"] = query.value(5).toDouble();
    result["candidateSnapshotJson"] = query.value(6).toString();
    result["createdAt"] = query.value(7).toString();

    // Extract status from candidate_snapshot_json
    QString snapshotJson = query.value(6).toString();
    if (!snapshotJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(snapshotJson.toUtf8());
        if (doc.isObject()) {
            result["status"] = doc.object().value("status").toString("pending");
        } else {
            result["status"] = "pending";
        }
    } else {
        result["status"] = "pending";
    }

    return result;
}

QVariantList InferenceService::listBatches(const QString &datasetId)
{
    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare(
        "SELECT id, model_version_id, dataset_id, target_sample_scope, "
        "conf_threshold, iou_threshold, candidate_snapshot_json, created_at "
        "FROM assisted_label_batches WHERE dataset_id = ? "
        "ORDER BY created_at DESC"
    );
    query.addBindValue(datasetId);

    if (!query.exec()) {
        qWarning() << "Failed to list inference batches:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap batch;
        batch["id"] = query.value(0).toString();
        batch["modelVersionId"] = query.value(1).toString();
        batch["datasetId"] = query.value(2).toString();
        batch["targetSampleScope"] = query.value(3).toString();
        batch["confThreshold"] = query.value(4).toDouble();
        batch["iouThreshold"] = query.value(5).toDouble();
        batch["candidateSnapshotJson"] = query.value(6).toString();
        batch["createdAt"] = query.value(7).toString();

        // Extract status from candidate_snapshot_json
        QString snapshotJson = query.value(6).toString();
        if (!snapshotJson.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(snapshotJson.toUtf8());
            if (doc.isObject()) {
                batch["status"] = doc.object().value("status").toString("pending");
            } else {
                batch["status"] = "pending";
            }
        } else {
            batch["status"] = "pending";
        }

        result.append(batch);
    }

    return result;
}

bool InferenceService::cancelBatch(const QString &batchId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // Get current snapshot json
    QSqlQuery getQuery(db);
    getQuery.prepare("SELECT candidate_snapshot_json FROM assisted_label_batches WHERE id = ?");
    getQuery.addBindValue(batchId);
    if (!getQuery.exec() || !getQuery.next()) return false;

    QString snapshotJsonStr = getQuery.value(0).toString();

    QJsonObject snapshotObj;
    if (!snapshotJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(snapshotJsonStr.toUtf8());
        if (doc.isObject()) {
            snapshotObj = doc.object();
        }
    }

    // Check if already in a terminal state
    QString currentStatus = snapshotObj.value("status").toString("pending");
    if (currentStatus == "completed" || currentStatus == "cancelled") {
        qWarning() << "Cannot cancel batch in status:" << currentStatus;
        return false;
    }

    // Update status to cancelled
    snapshotObj["status"] = "cancelled";
    QString updatedJson = QString::fromUtf8(
        QJsonDocument(snapshotObj).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    updateQuery.addBindValue(updatedJson);
    updateQuery.addBindValue(batchId);

    if (!updateQuery.exec()) {
        qWarning() << "Failed to cancel batch:" << updateQuery.lastError().text();
        return false;
    }

    // Send inference.cancel via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["batch_id"] = batchId;
        m_ipcClient->sendRequest("inference.cancel", payload);
    }

    emit batchStatusChanged(batchId, "cancelled");
    return true;
}
