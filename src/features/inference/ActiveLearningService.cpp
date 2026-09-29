#include "ActiveLearningService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "utils/Log.h"

#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QUuid>

ActiveLearningService::ActiveLearningService(QObject* parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_INFERENCE()) << "parent=" << parent;
}

void ActiveLearningService::setIpcClient(IpcClient* client)
{
    ltTrace(LT_LOG_INFERENCE()) << "client=" << client;
    m_ipcClient = client;
    if (m_ipcClient) {
        connect(m_ipcClient, &IpcClient::responseReceived,
                this, &ActiveLearningService::onResponseReceived);
    }
}

void ActiveLearningService::collectLowConfSamples(const QString& weightPath,
                                                    const QString& source,
                                                    double confThreshold,
                                                    double iou,
                                                    int imgsz,
                                                    const QString& device)
{
    ltInfo(LT_LOG_INFERENCE()) << "Collecting low confidence samples from:" << source
                               << "threshold:" << confThreshold
                               << "iou:" << iou
                               << "device:" << device;

    QJsonObject payload;
    payload["weight_path"] = weightPath;
    payload["source"] = source;
    payload["conf_threshold"] = confThreshold;
    payload["iou"] = iou;
    payload["imgsz"] = imgsz;
    payload["device"] = device;

    if (m_ipcClient && m_ipcClient->connected()) {
        m_ipcClient->sendRequest("active_learning.collect_low_conf", payload);
    } else {
        ltWarning(LT_LOG_INFERENCE()) << "IPC not connected, cannot collect low conf samples";
        emit error(tr("Python后端未连接，无法收集低置信样本"));
    }
}

void ActiveLearningService::prioritizeQueue(const QJsonArray& samples,
                                             const QString& queueType,
                                             const QVariantMap& classWeights,
                                             const QString& strategy)
{
    ltInfo(LT_LOG_INFERENCE()) << "Prioritizing queue:" << queueType
                               << "strategy:" << strategy
                               << "sample count:" << samples.size();

    if (samples.isEmpty()) {
        emit queuePrioritized(QJsonArray(), 0);
        return;
    }

    QJsonObject payload;
    payload["queue_type"] = queueType;
    payload["samples"] = samples;
    payload["strategy"] = strategy;

    QJsonObject weightsObj;
    for (auto it = classWeights.constBegin(); it != classWeights.constEnd(); ++it) {
        weightsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    payload["class_weights"] = weightsObj;

    if (m_ipcClient && m_ipcClient->connected()) {
        m_ipcClient->sendRequest("active_learning.prioritize_queue", payload);
    } else {
        ltWarning(LT_LOG_INFERENCE()) << "IPC not connected, returning unsorted samples";
        emit queuePrioritized(samples, samples.size());
    }
}

void ActiveLearningService::getQueueStats(const QJsonArray& samples,
                                           const QString& queueType)
{
    ltInfo(LT_LOG_INFERENCE()) << "Getting queue stats for:" << queueType
                               << "sample count:" << samples.size();

    QJsonObject payload;
    payload["queue_type"] = queueType;
    payload["samples"] = samples;

    if (m_ipcClient && m_ipcClient->connected()) {
        m_ipcClient->sendRequest("active_learning.queue_stats", payload);
    } else {
        ltWarning(LT_LOG_INFERENCE()) << "IPC not connected, computing local stats";

        QVariantMap stats;
        stats["total_samples"] = samples.size();
        stats["queue_type"] = queueType;

        if (!samples.isEmpty()) {
            QJsonObject classDistribution;
            double minConf = 1.0;
            double maxConf = 0.0;
            double totalConf = 0.0;
            int totalBoxes = 0;

            for (int i = 0; i < samples.size(); ++i) {
                QJsonObject sample = samples[i].toObject();
                QJsonArray boxes = sample["boxes"].toArray();
                totalBoxes += boxes.size();

                for (int j = 0; j < boxes.size(); ++j) {
                    QJsonObject box = boxes[j].toObject();
                    QString classId = QString::number(box["class_id"].toInt(0));
                    classDistribution[classId] = classDistribution.value(classId).toInt(0) + 1;

                    double conf = box["confidence"].toDouble(0.0);
                    totalConf += conf;
                    minConf = qMin(minConf, conf);
                    maxConf = qMax(maxConf, conf);
                }
            }

            if (totalBoxes > 0) {
                stats["avg_confidence"] = totalConf / totalBoxes;
            }
            stats["min_confidence"] = minConf;
            stats["max_confidence"] = maxConf;
            stats["total_boxes"] = totalBoxes;
            stats["avg_boxes_per_sample"] = static_cast<double>(totalBoxes) / samples.size();
            stats["class_distribution"] = classDistribution;
        }

        emit queueStatsReady(stats);
    }
}

void ActiveLearningService::onResponseReceived(const QJsonObject& response)
{
    QString command = response["command"].toString();
    bool success = response["success"].toBool();

    if (command.startsWith("active_learning.")) {
        if (!success) {
            QString errorMsg = response["error"].toObject()["message"].toString(tr("未知错误"));
            ltError(LT_LOG_INFERENCE()) << "IPC command failed:" << command << "error:" << errorMsg;
            emit error(errorMsg);
            return;
        }

        QJsonObject result = response["result"].toObject();

        if (command == "active_learning.collect_low_conf") {
            QJsonArray collectedSamples = result["samples"].toArray();
            int totalSamples = result["total"].toInt(collectedSamples.size());
            ltInfo(LT_LOG_INFERENCE()) << "Low conf samples collected:" << totalSamples;

            for (int i = 0; i < collectedSamples.size(); ++i) {
                QJsonObject sample = collectedSamples[i].toObject();
                // P1-14：收集结果持久化入库，防止重启丢失
                persistItem(QStringLiteral("low-confidence"), sample);
                m_lowConfQueue.append(sample);
            }

            emit samplesCollected(collectedSamples, totalSamples);
        } else if (command == "active_learning.prioritize_queue") {
            QJsonArray sortedSamples = result["sorted_samples"].toArray();
            int total = result["total"].toInt(sortedSamples.size());
            ltInfo(LT_LOG_INFERENCE()) << "Queue prioritized:" << total << "samples";
            emit queuePrioritized(sortedSamples, total);
        } else if (command == "active_learning.queue_stats") {
            QVariantMap stats;
            stats["total_samples"] = result["total_samples"].toInt();
            stats["queue_type"] = result["queue_type"].toString();
            stats["avg_confidence"] = result["avg_confidence"].toDouble();
            stats["min_confidence"] = result["min_confidence"].toDouble();
            stats["max_confidence"] = result["max_confidence"].toDouble();
            stats["total_boxes"] = result["total_boxes"].toInt();
            stats["avg_boxes_per_sample"] = result["avg_boxes_per_sample"].toDouble();
            stats["class_distribution"] = result["class_distribution"].toObject().toVariantMap();
            ltInfo(LT_LOG_INFERENCE()) << "Queue stats received:" << stats["total_samples"].toInt() << "samples";
            emit queueStatsReady(stats);
        }
    }
}

void ActiveLearningService::addSampleToQueue(const QString& queueType,
                                              const QJsonObject& sample)
{
    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        // P1-14：先落库再进内存，失败时不上内存，保证两边一致
        if (!persistItem(queueType, sample)) {
            ltError(LT_LOG_INFERENCE()) << "Failed to persist queue item:" << queueType
                                        << "path:" << sample["path"].toString();
            emit error(tr("主动学习队列条目写入数据库失败"));
            return;
        }
        queue->append(sample);
        ltInfo(LT_LOG_INFERENCE()) << "Added sample to queue:" << queueType
                                   << "path:" << sample["path"].toString()
                                   << "queue size:" << queue->size();
    }
}

void ActiveLearningService::removeSampleFromQueue(const QString& queueType,
                                                    const QString& samplePath)
{
    auto db = Database::instance().database();
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.prepare("UPDATE active_learning_items SET status = 'discarded', updated_at = CURRENT_TIMESTAMP "
                      "WHERE queue_type = ? AND sample_path = ? AND status = 'queued'");
        query.addBindValue(queueType);
        query.addBindValue(samplePath);
        if (!query.exec()) {
            ltError(LT_LOG_INFERENCE()) << "Failed to discard queue item:" << query.lastError().text();
        }
    }

    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        QJsonArray newQueue;
        for (int i = 0; i < queue->size(); ++i) {
            QJsonObject sample = queue->at(i).toObject();
            if (sample["path"].toString() != samplePath) {
                newQueue.append(sample);
            }
        }
        *queue = newQueue;
        ltInfo(LT_LOG_INFERENCE()) << "Removed sample from queue:" << queueType
                                   << "path:" << samplePath
                                   << "queue size:" << queue->size();
    }
}

void ActiveLearningService::clearQueue(const QString& queueType)
{
    auto db = Database::instance().database();
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.prepare("UPDATE active_learning_items SET status = 'discarded', updated_at = CURRENT_TIMESTAMP "
                      "WHERE queue_type = ? AND status = 'queued'");
        query.addBindValue(queueType);
        if (!query.exec()) {
            ltError(LT_LOG_INFERENCE()) << "Failed to clear queue in db:" << query.lastError().text();
        }
    }

    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        *queue = QJsonArray();
        ltInfo(LT_LOG_INFERENCE()) << "Cleared queue:" << queueType;
    }
}

QJsonArray ActiveLearningService::getQueueSamples(const QString& queueType) const
{
    // P1-14：数据库是权威数据源，重启后从此恢复
    const_cast<ActiveLearningService*>(this)->refreshQueueFromDb(queueType);

    const QJsonArray* queue = nullptr;

    if (queueType == "low-confidence") {
        queue = &m_lowConfQueue;
    } else if (queueType == "false-positive") {
        queue = &m_falsePositiveQueue;
    } else if (queueType == "false-negative") {
        queue = &m_falseNegativeQueue;
    } else if (queueType == "hard-case") {
        queue = &m_hardCaseQueue;
    }

    return queue ? *queue : QJsonArray();
}

QVariantMap ActiveLearningService::getAllQueueStats() const
{
    // 统计以数据库为准，保证重启后数量正确
    QVariantMap stats;
    auto db = Database::instance().database();
    if (db.isOpen()) {
        QSqlQuery query(db);
        query.prepare("SELECT queue_type, COUNT(*) FROM active_learning_items "
                      "WHERE status = 'queued' GROUP BY queue_type");
        if (query.exec()) {
            int total = 0;
            while (query.next()) {
                int count = query.value(1).toInt();
                stats[query.value(0).toString()] = count;
                total += count;
            }
            stats["total"] = total;
            return stats;
        }
    }

    // 数据库不可用时退回内存统计
    stats["low-confidence"] = m_lowConfQueue.size();
    stats["false-positive"] = m_falsePositiveQueue.size();
    stats["false-negative"] = m_falseNegativeQueue.size();
    stats["hard-case"] = m_hardCaseQueue.size();
    stats["total"] = m_lowConfQueue.size() + m_falsePositiveQueue.size() +
                     m_falseNegativeQueue.size() + m_hardCaseQueue.size();
    return stats;
}

QJsonArray* ActiveLearningService::getQueueByType(const QString& queueType)
{
    if (queueType == "low-confidence") {
        return &m_lowConfQueue;
    } else if (queueType == "false-positive") {
        return &m_falsePositiveQueue;
    } else if (queueType == "false-negative") {
        return &m_falseNegativeQueue;
    } else if (queueType == "hard-case") {
        return &m_hardCaseQueue;
    }
    return nullptr;
}

bool ActiveLearningService::persistItem(const QString& queueType, const QJsonObject& sample)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_INFERENCE()) << "Cannot persist active learning item: database not open";
        return false;
    }

    QString samplePath = sample.value("path").toString();
    if (samplePath.isEmpty()) {
        ltWarning(LT_LOG_INFERENCE()) << "Skip persisting active learning item without path";
        return false;
    }

    // 同路径同队列复用确定性主键，重复入队走 UPSERT，避免重复条目堆积
    // 用 SHA-256 派生稳定 ID，保证同一 (queueType, samplePath) 恒等
    const QString itemId = QString::fromLatin1(
        QCryptographicHash::hash((queueType + QChar('|') + samplePath).toUtf8(),
                                 QCryptographicHash::Sha256).toHex().left(32));

    QSqlQuery query(db);
    query.prepare(
        "INSERT INTO active_learning_items "
        "(id, queue_type, sample_path, sample_id, dataset_id, project_id, reason, priority, "
        " confidence, class_index, class_name, payload_json, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'queued') "
        "ON CONFLICT(id) DO UPDATE SET "
        "  queue_type = excluded.queue_type, payload_json = excluded.payload_json, "
        "  confidence = excluded.confidence, priority = excluded.priority, "
        "  status = 'queued', updated_at = CURRENT_TIMESTAMP"
    );
    query.addBindValue(itemId);
    query.addBindValue(queueType);
    query.addBindValue(samplePath);
    query.addBindValue(sample.value("sampleId").toString());
    query.addBindValue(sample.value("datasetId").toString());
    query.addBindValue(sample.value("projectId").toString());
    query.addBindValue(sample.value("reason").toString());
    // QVariant::toInt/toDouble 无默认值参数，缺失时显式回退
    query.addBindValue(sample.contains("priority") ? sample.value("priority").toInt() : 0);
    {
        double conf = 0.0;
        if (sample.contains("confidence")) {
            conf = sample.value("confidence").toDouble();
        } else if (sample.contains("max_confidence")) {
            conf = sample.value("max_confidence").toDouble();
        }
        query.addBindValue(conf);
    }
    {
        int clsIdx = -1;
        if (sample.contains("classIndex")) {
            clsIdx = sample.value("classIndex").toInt();
        } else if (sample.contains("class_id")) {
            clsIdx = sample.value("class_id").toInt();
        }
        query.addBindValue(clsIdx);
    }
    query.addBindValue(sample.value("className").toString());
    query.addBindValue(QString::fromUtf8(QJsonDocument(sample).toJson(QJsonDocument::Compact)));

    if (!query.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to insert active learning item:"
                                    << query.lastError().text();
        return false;
    }
    return true;
}

QJsonArray ActiveLearningService::loadQueueFromDb(const QString& queueType)
{
    QJsonArray items;
    auto db = Database::instance().database();
    if (!db.isOpen()) return items;

    QSqlQuery query(db);
    query.prepare("SELECT payload_json FROM active_learning_items "
                  "WHERE queue_type = ? AND status = 'queued' "
                  "ORDER BY priority DESC, confidence ASC, created_at ASC");
    query.addBindValue(queueType);
    if (!query.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to load active learning queue:"
                                    << query.lastError().text();
        return items;
    }

    while (query.next()) {
        QString payloadJson = query.value(0).toString();
        if (payloadJson.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(payloadJson.toUtf8());
        if (doc.isObject()) {
            items.append(doc.object());
        }
    }
    return items;
}

void ActiveLearningService::refreshQueueFromDb(const QString& queueType)
{
    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        *queue = loadQueueFromDb(queueType);
    }
}
