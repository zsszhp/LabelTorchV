#include "ActiveLearningService.h"
#include <QFile>
#include <QJsonDocument>
#include <QDebug>

ActiveLearningService::ActiveLearningService(QObject* parent)
    : QObject(parent)
{
}

void ActiveLearningService::collectLowConfSamples(const QString& weightPath,
                                                    const QString& source,
                                                    double confThreshold,
                                                    double iou,
                                                    int imgsz,
                                                    const QString& device)
{
    // 注意：实际实现需要通过 IPC 调用 Python 后端的 active_learning.collect_low_conf
    // 这里提供框架实现，实际调用需要集成到 IpcClient

    QJsonObject payload;
    payload["weight_path"] = weightPath;
    payload["source"] = source;
    payload["conf_threshold"] = confThreshold;
    payload["iou"] = iou;
    payload["imgsz"] = imgsz;
    payload["device"] = device;

    // TODO: 通过 IPC 发送命令到 Python 后端
    // emit commandRequested("active_learning.collect_low_conf", payload);

    qDebug() << "Collecting low confidence samples from:" << source
             << "with threshold:" << confThreshold;

    // 模拟实现：实际应由后端返回结果
    emit samplesCollected(QJsonArray(), 0);
}

void ActiveLearningService::prioritizeQueue(const QJsonArray& samples,
                                             const QString& queueType,
                                             const QVariantMap& classWeights,
                                             const QString& strategy)
{
    if (samples.isEmpty()) {
        emit queuePrioritized(QJsonArray(), 0);
        return;
    }

    QJsonObject payload;
    payload["queue_type"] = queueType;
    payload["samples"] = samples;
    payload["strategy"] = strategy;

    // 转换 classWeights 为 QJsonObject
    QJsonObject weightsObj;
    for (auto it = classWeights.constBegin(); it != classWeights.constEnd(); ++it) {
        weightsObj[it.key()] = QJsonValue::fromVariant(it.value());
    }
    payload["class_weights"] = weightsObj;

    // TODO: 通过 IPC 发送命令到 Python 后端
    // emit commandRequested("active_learning.prioritize_queue", payload);

    qDebug() << "Prioritizing queue:" << queueType
             << "with strategy:" << strategy
             << "sample count:" << samples.size();

    // 模拟实现
    emit queuePrioritized(samples, samples.size());
}

void ActiveLearningService::getQueueStats(const QJsonArray& samples,
                                           const QString& queueType)
{
    QJsonObject payload;
    payload["queue_type"] = queueType;
    payload["samples"] = samples;

    // TODO: 通过 IPC 发送命令到 Python 后端
    // emit commandRequested("active_learning.queue_stats", payload);

    // 本地统计实现
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

void ActiveLearningService::addSampleToQueue(const QString& queueType,
                                              const QJsonObject& sample)
{
    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        queue->append(sample);
        qDebug() << "Added sample to queue:" << queueType
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
        qDebug() << "Removed sample from queue:" << queueType
                 << "path:" << samplePath
                 << "queue size:" << queue->size();
    }
}

void ActiveLearningService::clearQueue(const QString& queueType)
{
    QJsonArray* queue = getQueueByType(queueType);
    if (queue) {
        queue->erase(queue->begin(), queue->end());
        qDebug() << "Cleared queue:" << queueType;
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
