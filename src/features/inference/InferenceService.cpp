#include "InferenceService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "ipc/IpcProtocol.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

InferenceService::InferenceService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_INFERENCE()) << "parent=" << parent;
}

void InferenceService::setIpcClient(IpcClient *client)
{
    ltTrace(LT_LOG_INFERENCE()) << "client=" << client;
    if (m_ipcClient) {
        disconnect(m_ipcClient, &IpcClient::responseReceived,
                   this, &InferenceService::onResponseReceived);
    }
    m_ipcClient = client;
    // A1：连接 responseReceived 信号，处理推理结果回传
    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::responseReceived,
                this, &InferenceService::onResponseReceived);
    }
}

QString InferenceService::runInference(const QString &modelVersionId,
                                        const QString &datasetId,
                                        const QString &sampleScope,
                                        double confThreshold,
                                        double iouThreshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "modelVersionId=" << modelVersionId
                                << "datasetId=" << datasetId
                                << "sampleScope=" << sampleScope
                                << "confThreshold=" << confThreshold
                                << "iouThreshold=" << iouThreshold;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // Validate model version exists and resolve weight path
    QSqlQuery checkVersion(db);
    checkVersion.prepare("SELECT id, best_weight_path, last_weight_path FROM model_versions WHERE id = ?");
    checkVersion.addBindValue(modelVersionId);
    if (!checkVersion.exec() || !checkVersion.next()) {
        ltError(LT_LOG_INFERENCE()) << "Model version not found:" << modelVersionId;
        return {};
    }
    QString weightPath = checkVersion.value(1).toString();
    if (weightPath.isEmpty()) {
        weightPath = checkVersion.value(2).toString();
    }
    if (weightPath.isEmpty()) {
        ltError(LT_LOG_INFERENCE()) << "No weight path for model version:" << modelVersionId;
        return {};
    }

    // Validate dataset exists and resolve image root
    QSqlQuery checkDataset(db);
    checkDataset.prepare("SELECT id, image_root FROM datasets WHERE id = ?");
    checkDataset.addBindValue(datasetId);
    if (!checkDataset.exec() || !checkDataset.next()) {
        ltError(LT_LOG_INFERENCE()) << "Dataset not found:" << datasetId;
        return {};
    }
    QString imageRoot = checkDataset.value(1).toString();

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
        ltError(LT_LOG_INFERENCE()) << "Failed to create inference batch:" << query.lastError().text();
        return {};
    }

    // Send inference.run via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["weight_path"] = weightPath;
        payload["source"] = imageRoot;
        payload["conf"] = confThreshold;
        payload["iou"] = iouThreshold;
        payload["save_annotated"] = true;
        // 可视化预览图输出目录：项目 cache/inference_annotated/{batchId}
        QSqlQuery projQuery(db);
        projQuery.prepare("SELECT p.root_path FROM projects p JOIN datasets d ON d.project_id = p.id WHERE d.id = ?");
        projQuery.addBindValue(datasetId);
        QString annotatedDir;
        if (projQuery.exec() && projQuery.next()) {
            annotatedDir = projQuery.value(0).toString() + "/cache/inference_annotated/" + batchId;
        }
        payload["annotated_dir"] = annotatedDir;
        // A1：记录 requestId → batchId 映射，用于关联响应
        QString requestId = m_ipcClient->sendRequest(IpcProtocol::CMD_INFERENCE_RUN, payload);
        if (!requestId.isEmpty()) {
            m_pendingBatches[requestId] = batchId;
        } else {
            // IPC 发送失败，标记批次为 failed
            QJsonObject failedMeta;
            failedMeta["status"] = "failed";
            failedMeta["error"] = "IPC 未连接，无法发送推理请求";
            failedMeta["candidates"] = QJsonArray();
            QString failedJson = QString::fromUtf8(
                QJsonDocument(failedMeta).toJson(QJsonDocument::Compact));
            QSqlQuery failQuery(db);
            failQuery.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
            failQuery.addBindValue(failedJson);
            failQuery.addBindValue(batchId);
            failQuery.exec();
            emit batchStatusChanged(batchId, "failed");
            emit batchCompleted(batchId, false, 0);
            ltError(LT_LOG_INFERENCE()) << "IPC send failed for batch:" << batchId;
            return batchId;
        }
    }

    ltInfo(LT_LOG_INFERENCE()) << "Created inference batch:" << batchId
                               << "modelVersion:" << modelVersionId
                               << "dataset:" << datasetId;
    emit batchStatusChanged(batchId, "pending");
    return batchId;
}

QVariantMap InferenceService::getBatchStatus(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

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
    ltTrace(LT_LOG_INFERENCE()) << "datasetId=" << datasetId;

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
        ltError(LT_LOG_INFERENCE()) << "Failed to list inference batches:" << query.lastError().text();
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

    ltDebug(LT_LOG_INFERENCE()) << "Listed" << result.size() << "batches for dataset:" << datasetId;
    return result;
}

bool InferenceService::cancelBatch(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

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
        ltWarning(LT_LOG_INFERENCE()) << "Cannot cancel batch in status:" << currentStatus;
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
        ltError(LT_LOG_INFERENCE()) << "Failed to cancel batch:" << updateQuery.lastError().text();
        return false;
    }

    // Send inference.cancel via IpcClient if available
    if (m_ipcClient) {
        QJsonObject payload;
        payload["batch_id"] = batchId;
        m_ipcClient->sendRequest(IpcProtocol::CMD_INFERENCE_CANCEL, payload);
    }

    ltInfo(LT_LOG_INFERENCE()) << "Cancelled inference batch:" << batchId;
    emit batchStatusChanged(batchId, "cancelled");
    return true;
}

void InferenceService::onResponseReceived(const QJsonObject &response)
{
    // A1：处理推理响应，将候选框数据写入数据库
    QString command = response.value("command").toString();

    // P2-6: 视频推理响应处理
    if (command == IpcProtocol::CMD_INFERENCE_RUN_VIDEO) {
        QString requestId = response.value("request_id").toString();
        if (!m_pendingVideoRequests.contains(requestId)) {
            return;
        }
        m_pendingVideoRequests.remove(requestId);

        bool success = response.value("success").toBool(false);
        QJsonObject result = response.value("result").toObject();
        QString error = response.value("error").toObject().value("message").toString();

        if (success) {
            QString outputPath = result.value("output_path").toString();
            int totalFrames = result.value("total_frames").toInt();
            int totalDetections = result.value("total_detections").toInt();
            ltInfo(LT_LOG_INFERENCE()) << "Video inference finished:"
                                       << "frames=" << totalFrames
                                       << "dets=" << totalDetections
                                       << "output=" << outputPath;
            emit videoInferenceFinished(outputPath, true, totalFrames, totalDetections, QString());
        } else {
            ltError(LT_LOG_INFERENCE()) << "Video inference failed:" << error;
            emit videoInferenceFinished(QString(), false, 0, 0,
                                        error.isEmpty() ? QStringLiteral("视频推理失败") : error);
        }
        return;
    }

    if (command != IpcProtocol::CMD_INFERENCE_RUN) {
        return; // 只处理 inference.run 响应
    }

    QString requestId = response.value("request_id").toString();
    bool success = response.value("success").toBool();
    QJsonObject result = response.value("result").toObject();

    // 通过 requestId 查找对应的 batchId
    if (!m_pendingBatches.contains(requestId)) {
        ltWarning(LT_LOG_INFERENCE()) << "Received response for unknown request:" << requestId;
        return;
    }

    QString batchId = m_pendingBatches.take(requestId);
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_INFERENCE()) << "Database not open for batch update:" << batchId;
        return;
    }

    // 构建更新后的 candidate_snapshot_json
    QJsonObject snapshotMeta;
    QJsonArray candidates;

    if (success) {
        // 解析推理结果，转换为候选框格式（含可视化预览图路径）
        QJsonArray predictions = result.value("predictions").toArray();
        int totalBoxes = 0;
        for (const QJsonValue &predVal : predictions) {
            QJsonObject pred = predVal.toObject();
            QString imagePath = pred.value("path").toString();
            QString annotatedPath = pred.value("annotated_path").toString();
            QJsonArray boxes = pred.value("boxes").toArray();
            for (const QJsonValue &boxVal : boxes) {
                QJsonObject box = boxVal.toObject();
                QJsonObject candidate;
                candidate["image_path"] = imagePath;
                candidate["annotated_path"] = annotatedPath;
                candidate["class_id"] = box.value("class_id").toInt();
                candidate["class_name"] = box.value("class_name").toString();
                candidate["confidence"] = box.value("confidence").toDouble();
                candidate["xyxy"] = box.value("xyxy").toArray();
                candidates.append(candidate);
                totalBoxes++;
            }
        }

        snapshotMeta["status"] = "completed";
        snapshotMeta["candidates"] = candidates;
        snapshotMeta["total_images"] = result.value("total_images").toInt();
        snapshotMeta["total_boxes"] = result.value("total_boxes").toInt();
        snapshotMeta["annotated_dir"] = result.value("annotated_dir").toString();

        ltInfo(LT_LOG_INFERENCE()) << "Inference batch completed:" << batchId
                                    << "candidates:" << totalBoxes;
        emit batchStatusChanged(batchId, "completed");
        emit batchCompleted(batchId, true, totalBoxes);
    } else {
        // 推理失败
        QJsonObject errorObj = response.value("error").toObject();
        QString errorMsg = errorObj.value("message").toString();
        if (errorMsg.isEmpty()) {
            errorMsg = result.value("error").toString("Unknown inference error");
        }

        snapshotMeta["status"] = "failed";
        snapshotMeta["error"] = errorMsg;
        snapshotMeta["candidates"] = QJsonArray();

        ltError(LT_LOG_INFERENCE()) << "Inference batch failed:" << batchId
                                     << "error:" << errorMsg;
        emit batchStatusChanged(batchId, "failed");
        emit batchCompleted(batchId, false, 0);
    }

    // 更新数据库中的 candidate_snapshot_json
    QString updatedJson = QString::fromUtf8(
        QJsonDocument(snapshotMeta).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    updateQuery.addBindValue(updatedJson);
    updateQuery.addBindValue(batchId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to update batch snapshot:"
                                     << updateQuery.lastError().text();
    }
}

// ============================================================================
// P2-6: 视频流推理（supervision 集成）
// ============================================================================

QString InferenceService::runVideoInference(const QString &modelVersionId,
                                              const QString &videoPath,
                                              const QString &outputPath,
                                              double confThreshold,
                                              double iouThreshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "modelVersionId=" << modelVersionId
                                << "videoPath=" << videoPath
                                << "conf=" << confThreshold
                                << "iou=" << iouThreshold;

    if (!m_ipcClient) {
        ltError(LT_LOG_INFERENCE()) << "IpcClient not injected";
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("IPC 客户端未注入"));
        return {};
    }

    if (videoPath.isEmpty() || !QFileInfo::exists(videoPath)) {
        ltError(LT_LOG_INFERENCE()) << "Invalid video path:" << videoPath;
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("视频文件不存在"));
        return {};
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) {
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("数据库未打开"));
        return {};
    }

    // 1. 解析模型权重路径
    QSqlQuery versionQuery(db);
    versionQuery.prepare("SELECT best_weight_path, last_weight_path FROM model_versions WHERE id = ?");
    versionQuery.addBindValue(modelVersionId);
    if (!versionQuery.exec() || !versionQuery.next()) {
        ltError(LT_LOG_INFERENCE()) << "Model version not found:" << modelVersionId;
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("模型版本不存在"));
        return {};
    }
    QString weightPath = versionQuery.value(0).toString();
    if (weightPath.isEmpty()) {
        weightPath = versionQuery.value(1).toString();
    }
    if (weightPath.isEmpty()) {
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("模型权重路径为空"));
        return {};
    }

    // 2. 派生输出路径：{projectRoot}/cache/video_inference/{timestamp}_{basename}.mp4
    QString finalOutputPath = outputPath;
    if (finalOutputPath.isEmpty()) {
        QSqlQuery projQuery(db);
        projQuery.prepare(
            "SELECT p.root_path FROM projects p "
            "JOIN model_versions mv ON mv.run_id IN "
            "(SELECT id FROM training_runs WHERE project_id = p.id) "
            "WHERE mv.id = ?");
        projQuery.addBindValue(modelVersionId);
        QString projectRoot;
        if (projQuery.exec() && projQuery.next()) {
            projectRoot = projQuery.value(0).toString();
        }
        if (projectRoot.isEmpty()) {
            // 回退到视频同目录
            QFileInfo vi(videoPath);
            finalOutputPath = vi.absolutePath() + QStringLiteral("/")
                              + vi.completeBaseName()
                              + QStringLiteral("_annotated.mp4");
        } else {
            QString cacheDir = projectRoot + QStringLiteral("/cache/video_inference");
            QDir().mkpath(cacheDir);
            QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            QFileInfo vi(videoPath);
            finalOutputPath = cacheDir + QStringLiteral("/") + timestamp
                              + QStringLiteral("_") + vi.completeBaseName()
                              + QStringLiteral(".mp4");
        }
    } else {
        // 确保父目录存在
        QDir().mkpath(QFileInfo(finalOutputPath).absolutePath());
    }

    // 3. 发起 IPC 请求
    QJsonObject payload;
    payload["weight_path"] = weightPath;
    payload["video_path"] = videoPath;
    payload["output_path"] = finalOutputPath;
    payload["conf"] = confThreshold;
    payload["iou"] = iouThreshold;

    QString requestId = m_ipcClient->sendRequest(IpcProtocol::CMD_INFERENCE_RUN_VIDEO, payload);
    if (requestId.isEmpty()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to send inference.run_video request";
        emit videoInferenceFinished(QString(), false, 0, 0, QStringLiteral("IPC 请求发送失败"));
        return {};
    }

    m_pendingVideoRequests.insert(requestId);
    ltInfo(LT_LOG_INFERENCE()) << "Video inference requested:" << videoPath
                               << "output=" << finalOutputPath
                               << "requestId=" << requestId;
    return requestId;
}
