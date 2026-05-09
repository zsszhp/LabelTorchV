#ifndef ANNOTATIONSERVICE_H
#define ANNOTATIONSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief Service layer for annotation persistence and revision tracking.
 *
 * Reads/writes YOLO txt label files via YoloTxtReader/YoloTxtWriter,
 * and records annotation_revisions in the database for undo/audit.
 */
class AnnotationService : public QObject
{
    Q_OBJECT

public:
    explicit AnnotationService(QObject *parent = nullptr);

    /**
     * @brief Load annotations from a YOLO txt label file.
     *
     * Uses YoloTxtReader to parse the file, then converts each AxisAlignedBox
     * to a QVariantMap with keys: id, classIndex, className, cx, cy, w, h,
     * confidence, sourceType, isConfirmed.
     *
     * @param labelPath  Absolute path to the .txt label file.
     * @return QVariantList of QVariantMap entries.
     */
    Q_INVOKABLE QVariantList loadAnnotations(const QString &labelPath);

    /**
     * @brief Save annotations to a YOLO txt label file and record a revision.
     *
     * Converts the QVariantList to AxisAlignedBox objects, writes atomically
     * via YoloTxtWriter, then creates an annotation_revision record.
     *
     * @param labelPath    Destination file path.
     * @param datasetId    Dataset ID for the revision record.
     * @param sampleId     Sample ID for the revision record.
     * @param annotations  QVariantList of annotation maps (same format as loadAnnotations output).
     * @return true on success, false on any I/O or database error.
     */
    Q_INVOKABLE bool saveAnnotations(const QString &labelPath, const QString &datasetId,
                                     const QString &sampleId, const QVariantList &annotations);

    /**
     * @brief Create an annotation_revision record.
     *
     * Serializes before/after snapshots as JSON arrays and inserts them
     * into the annotation_revisions table.
     *
     * @param datasetId       Dataset ID.
     * @param sampleId        Sample ID.
     * @param sourceType      Source type (e.g. "manual", "assisted").
     * @param beforeSnapshot  QVariantList of annotation maps before the change.
     * @param afterSnapshot   QVariantList of annotation maps after the change.
     * @return Revision ID on success, empty string on failure.
     */
    Q_INVOKABLE QString createRevision(const QString &datasetId, const QString &sampleId,
                                       const QString &sourceType,
                                       const QVariantList &beforeSnapshot,
                                       const QVariantList &afterSnapshot);

    /**
     * @brief List all samples for a dataset.
     * @param datasetId The dataset ID.
     * @return QVariantList of QVariantMap entries with sample fields.
     */
    Q_INVOKABLE QVariantList listSamples(const QString &datasetId);

    /**
     * @brief Get details of a specific sample.
     * @param sampleId The sample ID.
     * @return QVariantMap with sample fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getSample(const QString &sampleId);
};

#endif // ANNOTATIONSERVICE_H
