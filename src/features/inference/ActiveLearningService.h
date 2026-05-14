#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

/// <summary>
/// 主动学习服务
/// 负责低置信样本回流、队列管理、优先级排序
/// </summary>
class ActiveLearningService : public QObject
{
    Q_OBJECT

public:
    explicit ActiveLearningService(QObject* parent = nullptr);
    ~ActiveLearningService() = default;

    // 队列类型枚举
    enum class QueueType {
        LowConfidence,
        FalsePositive,
        FalseNegative,
        HardCase
    };
    Q_ENUM(QueueType)

public slots:
    /// <summary>
    /// 收集低置信度样本
    /// </summary>
    void collectLowConfSamples(const QString& weightPath,
                                const QString& source,
                                double confThreshold = 0.3,
                                double iou = 0.45,
                                int imgsz = 640,
                                const QString& device = "auto");

    /// <summary>
    /// 对队列进行优先级排序
    /// </summary>
    void prioritizeQueue(const QJsonArray& samples,
                          const QString& queueType = "low-confidence",
                          const QVariantMap& classWeights = {},
                          const QString& strategy = "default");

    /// <summary>
    /// 获取队列统计信息
    /// </summary>
    void getQueueStats(const QJsonArray& samples,
                        const QString& queueType = "all");

    /// <summary>
    /// 添加样本到指定队列
    /// </summary>
    void addSampleToQueue(const QString& queueType,
                           const QJsonObject& sample);

    /// <summary>
    /// 从队列移除样本
    /// </summary>
    void removeSampleFromQueue(const QString& queueType,
                                const QString& samplePath);

    /// <summary>
    /// 清空指定队列
    /// </summary>
    void clearQueue(const QString& queueType);

    /// <summary>
    /// 获取队列中的所有样本
    /// </summary>
    QJsonArray getQueueSamples(const QString& queueType) const;

    /// <summary>
    /// 获取所有队列的总统计
    /// </summary>
    QVariantMap getAllQueueStats() const;

signals:
    /// <summary>
    /// 样本收集完成信号
    /// </summary>
    void samplesCollected(const QJsonArray& samples, int totalSamples);

    /// <summary>
    /// 队列排序完成信号
    /// </summary>
    void queuePrioritized(const QJsonArray& sortedSamples, int total);

    /// <summary>
    /// 队列统计信号
    /// </summary>
    void queueStatsReady(const QVariantMap& stats);

    /// <summary>
    /// 错误信号
    /// </summary>
    void error(const QString& message);

private:
    // 队列数据存储
    QJsonArray m_lowConfQueue;
    QJsonArray m_falsePositiveQueue;
    QJsonArray m_falseNegativeQueue;
    QJsonArray m_hardCaseQueue;

    /// <summary>
    /// 根据队列类型获取对应的队列引用
    /// </summary>
    QJsonArray* getQueueByType(const QString& queueType);
};
