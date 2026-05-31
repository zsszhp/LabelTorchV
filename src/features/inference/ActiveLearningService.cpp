#include "ActiveLearningService.h"
#include "ipc/IpcClient.h"
#include "utils/Log.h"

#include <QJsonDocument>

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
                m_lowConfQueue.append(collectedSamples[i]);
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
        queue->append(sample);
        ltInfo(LT_LOG_INFERENCE()) << "Added sample to queue:" << queueType
                                   << "path:" << sample["path"].toString()
                                   << "queue size:" << queue->size();
    }
}

void ActiveLearningService::removeSampleFromQueue(const QString& queueType,
                                                    const QString& samplePath)
{
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
    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        *queue = QJsonArray();
        ltInfo(LT_LOG_INFERENCE()) << "Cleared queue:" << queueType;
    }
}

QJsonArray ActiveLearningService::getQueueSamples(const QString& queueType) const
{
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
    QVariantMap stats;
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
