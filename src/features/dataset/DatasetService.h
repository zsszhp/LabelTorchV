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

private:
    bool updateImportStatus(const QString &datasetId, const QString &status);
    bool insertSamples(const QString &datasetId, const QVariantList &samples);
    bool extractAndStoreSchema(const QString &datasetId, const QVariantList &samples);

    ImportScanner *m_scanner;
};

#endif // DATASETSERVICE_H
