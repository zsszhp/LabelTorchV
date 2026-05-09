#ifndef METRICSERVICE_H
#define METRICSERVICE_H

#include <QObject>
#include <QString>
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
};

#endif // METRICSERVICE_H
