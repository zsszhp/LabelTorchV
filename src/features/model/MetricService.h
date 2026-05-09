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
};

#endif // METRICSERVICE_H
