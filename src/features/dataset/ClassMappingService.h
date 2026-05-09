#ifndef CLASSMAPPINGSERVICE_H
#define CLASSMAPPINGSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>

class ClassMappingService : public QObject
{
    Q_OBJECT

public:
    explicit ClassMappingService(QObject *parent = nullptr);

    /**
     * @brief Get the imported label schema for a dataset.
     *
     * Queries the imported_label_schemas table for the given dataset.
     *
     * @param datasetId The dataset ID.
     * @return QVariantMap with schema fields (id, datasetId, rawClassNamesJson,
     *         rawClassOrderJson, sourceFormat), or empty on not found.
     */
    Q_INVOKABLE QVariantMap getSourceSchema(const QString &datasetId);

    /**
     * @brief Create a mapping revision between source schema and target taxonomy.
     *
     * Stores the mapping rules as JSON in the class_mapping_revisions table.
     *
     * @param datasetId The dataset this mapping belongs to.
     * @param sourceSchemaId The source imported label schema ID.
     * @param targetTaxonomyId The target taxonomy ID.
     * @param mappingRules Map of {source_class_name: target_class_name}.
     * @return Mapping revision ID on success, empty string on failure.
     */
    Q_INVOKABLE QString createMapping(const QString &datasetId, const QString &sourceSchemaId,
                                       const QString &targetTaxonomyId, const QVariantMap &mappingRules);

    /**
     * @brief List all mapping revisions for a dataset.
     * @param datasetId The dataset ID.
     * @return QVariantList of QVariantMap entries with mapping revision fields.
     */
    Q_INVOKABLE QVariantList listMappings(const QString &datasetId);

    /**
     * @brief Preview what the mapping would produce without applying it.
     *
     * Reads all label files for the dataset and simulates the mapping to
     * compute total labels, affected labels, and the new class distribution.
     *
     * @param datasetId The dataset ID.
     * @param mappingRules Map of {source_class_name: target_class_name}.
     * @return QVariantMap with keys: totalLabels, affectedLabels, newClassDistribution.
     */
    Q_INVOKABLE QVariantMap previewMapping(const QString &datasetId, const QVariantMap &mappingRules);

    /**
     * @brief Apply a mapping revision: rewrite label files and update taxonomy.
     *
     * Reads the mapping revision, builds source->target class index remapping,
     * rewrites each YOLO txt label file in-place (updates class_id per line),
     * and adds new classes to the target taxonomy if needed.
     *
     * @param mappingRevisionId The mapping revision ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool applyMapping(const QString &mappingRevisionId);

private:
    /**
     * @brief Build source class index -> source class name map from imported schema.
     * @param sourceSchemaId The imported label schema ID.
     * @return QMap from class index to class name.
     */
    QMap<int, QString> buildSourceIndexNameMap(const QString &sourceSchemaId);

    /**
     * @brief Build target class name -> target class index map from taxonomy.
     * @param taxonomyId The target taxonomy ID.
     * @return QMap from class name to class index.
     */
    QMap<QString, int> buildTargetNameIndexMap(const QString &taxonomyId);

    /**
     * @brief Remap class IDs in a single YOLO txt label file.
     *
     * Reads the file, replaces the class_id on each line according to indexRemap,
     * and writes the file back. Also collects statistics.
     *
     * @param filePath Absolute path to the label file.
     * @param indexRemap Map from old class index to new class index.
     * @param affectedCount Output: number of lines where class_id changed.
     * @param totalCount Output: total number of non-empty label lines.
     * @param newClassCounts Output: counts per new class index after mapping.
     * @return true on success, false on I/O or parse failure.
     */
    bool remapLabelFile(const QString &filePath, const QMap<int, int> &indexRemap,
                        int &affectedCount, int &totalCount, QMap<int, int> &newClassCounts);
};

#endif // CLASSMAPPINGSERVICE_H
