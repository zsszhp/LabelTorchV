#ifndef DATASETSERVICE_H
#define DATASETSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class ImportScanner;

class DatasetService : public QObject
{
    Q_OBJECT

public:
    explicit DatasetService(QObject *parent = nullptr);

    /**
     * @brief Import a dataset from YOLO txt format directories.
     *
     * Creates a dataset record, runs scan, inserts matched samples,
     * and extracts class schema from all label files.
     * Import status transitions: idle -> scanning -> importing -> completed/failed
     *
     * @param projectId The project this dataset belongs to.
     * @param name Human-readable dataset name.
     * @param imageDir Directory containing image files.
     * @param labelDir Directory containing YOLO txt label files.
     * @return Dataset ID on success, empty string on failure.
     */
    Q_INVOKABLE QString importDataset(const QString &projectId, const QString &name,
                                      const QString &imageDir, const QString &labelDir);

    /**
     * @brief List all datasets for a project.
     * @param projectId The project to list datasets for.
     * @return QVariantList of QVariantMap entries with dataset fields.
     */
    Q_INVOKABLE QVariantList listDatasets(const QString &projectId);

    /**
     * @brief Get details of a specific dataset.
     * @param datasetId The dataset ID.
     * @return QVariantMap with dataset fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getDataset(const QString &datasetId);

    /**
     * @brief Delete a dataset and all its samples.
     * @param datasetId The dataset ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool deleteDataset(const QString &datasetId);

    /**
     * @brief Get sample statistics for a dataset.
     * Returns QVariantMap with:
     * - "totalSamples": total count
     * - "validSamples": count with validation_status='valid'
     * - "invalidSamples": count with validation_status != 'valid'
     * - "labeledSamples": count with non-null label_path
     * - "unlabeledSamples": count with null label_path
     * - "classDistribution": QVariantMap of class_id -> count
     * - "annotationDensity": QVariantMap with min/max/avg/median annotations per sample
     */
    Q_INVOKABLE QVariantMap getSampleStats(const QString &datasetId);

    /**
     * @brief Detect anomalies in the dataset.
     * Returns QVariantMap with:
     * - "emptyLabels": QVariantList of sample IDs with empty label files
     * - "classErrors": QVariantList of sample IDs with class_id outside valid range
     * - "sizeAnomalies": QVariantList of sample IDs with unusual image dimensions (optional, may be empty if width/height not populated)
     * - "duplicateImages": QVariantList of sample IDs with duplicate hash values
     * - "totalAnomalies": total count of all anomaly items
     */
    Q_INVOKABLE QVariantMap detectAnomalies(const QString &datasetId);

    /**
     * @brief Get class distribution for a dataset.
     * Returns QVariantList of QVariantMap entries, each with "classId" and "count".
     * Ordered by count descending.
     */
    Q_INVOKABLE QVariantList getClassDistribution(const QString &datasetId);

private:
    bool updateImportStatus(const QString &datasetId, const QString &status);
    bool insertSamples(const QString &datasetId, const QVariantList &samples);
    bool extractAndStoreSchema(const QString &datasetId, const QVariantList &samples);

    ImportScanner *m_scanner;
};

#endif // DATASETSERVICE_H
