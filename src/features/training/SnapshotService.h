#ifndef SNAPSHOTSERVICE_H
#define SNAPSHOTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class SnapshotService : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotService(QObject *parent = nullptr);

    /**
     * @brief Create an immutable snapshot of a dataset.
     *
     * Freezes the current sample list, taxonomy version, annotation revision
     * boundary, and generates train/val split.
     *
     * @param datasetId The dataset to snapshot.
     * @param trainRatio Train split ratio (0.0-1.0, default 0.8).
     * @param splitStrategy "sequential" or "random" (default "random").
     * @return Snapshot ID on success, empty string on failure.
     */
    Q_INVOKABLE QString createSnapshot(const QString &datasetId,
                                        double trainRatio = 0.8,
                                        const QString &splitStrategy = "random");

    /**
     * @brief List all snapshots for a dataset.
     * @param datasetId The dataset ID.
     * @return QVariantList of QVariantMap with snapshot fields.
     */
    Q_INVOKABLE QVariantList listSnapshots(const QString &datasetId);

    /**
     * @brief Get details of a specific snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantMap with snapshot fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getSnapshot(const QString &snapshotId);

    /**
     * @brief Delete a snapshot. Only allowed if no training runs reference it.
     * @param snapshotId The snapshot ID.
     * @return true on success, false on failure (e.g. in use).
     */
    Q_INVOKABLE bool deleteSnapshot(const QString &snapshotId);

    /**
     * @brief Get the sample manifest for a snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantList of sample IDs in the manifest.
     */
    Q_INVOKABLE QVariantList getSampleManifest(const QString &snapshotId);

    /**
     * @brief Get the split manifest for a snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantMap with "train" and "val" keys, each containing QVariantList of sample IDs.
     */
    Q_INVOKABLE QVariantMap getSplitManifest(const QString &snapshotId);

    /**
     * @brief Check if a snapshot is immutable (always true after creation).
     * @param snapshotId The snapshot ID.
     * @return true if snapshot exists and is immutable.
     */
    Q_INVOKABLE bool isImmutable(const QString &snapshotId);
};

#endif // SNAPSHOTSERVICE_H
