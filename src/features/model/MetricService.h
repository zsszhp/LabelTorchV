#ifndef METRICSERVICE_H
#define METRICSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class MetricService : public QObject
{
    Q_OBJECT

public:
    explicit MetricService(QObject *parent = nullptr);

    /**
     * @brief Get metrics for a version (parse metrics_snapshot_json).
     * Returns QVariantMap with parsed metrics fields.
     */
    Q_INVOKABLE QVariantMap getMetrics(const QString &versionId);

    /**
     * @brief Get per-epoch metrics history from training log.
     * Returns QVariantList of per-epoch QVariantMap entries.
     */
    Q_INVOKABLE QVariantList getMetricHistory(const QString &runId);

    /**
     * @brief Compare metrics between two versions.
     * Returns QVariantMap with side-by-side comparison.
     */
    Q_INVOKABLE QVariantMap compareVersions(const QString &versionId1, const QString &versionId2);

    /**
     * @brief Compare multiple versions side-by-side.
     * Returns QVariantList where each item is a QVariantMap containing:
     *   - versionId: the version ID
     *   - metrics: the parsed metrics map for that version
     *   - snapshotId: the snapshot_id from the training run (horizontal comparison)
     *   - parentVersionId: the parent version ID (vertical/incremental chain comparison)
     */
    Q_INVOKABLE QVariantList compareMultipleVersions(const QVariantList &versionIds);

    /**
     * @brief Get all model versions whose training run used the given snapshot.
     * Enables horizontal comparison (same dataset snapshot, different configs/epochs).
     * Returns QVariantList of QVariantMap with version details + snapshotId.
     */
    Q_INVOKABLE QVariantList getVersionsBySnapshot(const QString &snapshotId);

    /**
     * @brief 存储单个训练指标到 run_metrics 表
     * @param runId 训练运行 ID
     * @param epoch 当前 epoch
     * @param metricName 指标名称（如 "loss", "mAP50" 等）
     * @param metricValue 指标值
     * @return true 成功，false 失败
     */
    Q_INVOKABLE bool storeMetric(const QString &runId, int epoch,
                                  const QString &metricName, double metricValue);

    /**
     * @brief 批量存储一个 epoch 的所有指标
     * @param runId 训练运行 ID
     * @param epoch 当前 epoch
     * @param metrics 指标键值对 QVariantMap
     * @return true 成功，false 失败
     */
    Q_INVOKABLE bool storeEpochMetrics(const QString &runId, int epoch,
                                        const QVariantMap &metrics);

    /**
     * @brief 获取指定训练运行的实时指标历史（从 run_metrics 表）
     * @param runId 训练运行 ID
     * @param metricName 指标名称（可选，为空则返回所有指标）
     * @return QVariantList of QVariantMap with epoch, metric_name, metric_value
     */
    Q_INVOKABLE QVariantList getRunMetrics(const QString &runId,
                                            const QString &metricName = QString());
};

#endif // METRICSERVICE_H
