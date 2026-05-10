#include "ClassMappingService.h"
#include "database/Database.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDebug>

ClassMappingService::ClassMappingService(QObject *parent)
    : QObject(parent)
{
}

QVariantMap ClassMappingService::getSourceSchema(const QString &datasetId)
{
    if (datasetId.isEmpty()) {
        qWarning() << "ClassMappingService::getSourceSchema: datasetId is empty";
        return {};
    }

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, dataset_id, raw_class_names_json, raw_class_order_json, source_format "
                  "FROM imported_label_schemas WHERE dataset_id = ?");
    query.addBindValue(datasetId);

    if (!query.exec()) {
        qWarning() << "ClassMappingService::getSourceSchema: query failed:" << query.lastError().text();
        return {};
    }

    if (query.next()) {
        QVariantMap schema;
        schema["id"] = query.value(0);
        schema["datasetId"] = query.value(1);
        schema["rawClassNamesJson"] = query.value(2);
        schema["rawClassOrderJson"] = query.value(3);
        schema["sourceFormat"] = query.value(4);
        return schema;
    }

    qDebug() << "ClassMappingService::getSourceSchema: no schema found for dataset" << datasetId;
    return {};
}

QString ClassMappingService::createMapping(const QString &datasetId, const QString &sourceSchemaId,
                                            const QString &targetTaxonomyId,
                                            const QVariantMap &mappingRules)
{
    if (datasetId.isEmpty() || sourceSchemaId.isEmpty() || targetTaxonomyId.isEmpty()) {
        qWarning() << "ClassMappingService::createMapping: missing required parameters";
        return {};
    }

    if (mappingRules.isEmpty()) {
        qWarning() << "ClassMappingService::createMapping: mappingRules is empty";
        return {};
    }

    // Serialize mappingRules to JSON object
    QJsonObject rulesObj;
    for (auto it = mappingRules.constBegin(); it != mappingRules.constEnd(); ++it) {
        rulesObj[it.key()] = it.value().toString();
    }
    QString rulesJson = QJsonDocument(rulesObj).toJson(QJsonDocument::Compact);

    QString revisionId = Id::generate();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO class_mapping_revisions "
                  "(id, dataset_id, source_schema_id, target_taxonomy_id, mapping_rules_json) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(revisionId);
    query.addBindValue(datasetId);
    query.addBindValue(sourceSchemaId);
    query.addBindValue(targetTaxonomyId);
    query.addBindValue(rulesJson);

    if (!query.exec()) {
        qWarning() << "ClassMappingService::createMapping: insert failed:" << query.lastError().text();
        return {};
    }

    qDebug() << "ClassMappingService: Created mapping revision" << revisionId
             << "for dataset" << datasetId
             << "schema" << sourceSchemaId << "-> taxonomy" << targetTaxonomyId;
    return revisionId;
}

QVariantList ClassMappingService::listMappings(const QString &datasetId)
{
    QVariantList result;

    if (datasetId.isEmpty()) {
        qWarning() << "ClassMappingService::listMappings: datasetId is empty";
        return result;
    }

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, dataset_id, source_schema_id, target_taxonomy_id, "
                  "mapping_rules_json, created_at "
                  "FROM class_mapping_revisions WHERE dataset_id = ? ORDER BY created_at DESC");
    query.addBindValue(datasetId);

    if (!query.exec()) {
        qWarning() << "ClassMappingService::listMappings: query failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap rev;
        rev["id"] = query.value(0);
        rev["datasetId"] = query.value(1);
        rev["sourceSchemaId"] = query.value(2);
        rev["targetTaxonomyId"] = query.value(3);
        rev["mappingRulesJson"] = query.value(4);
        rev["createdAt"] = query.value(5);
        result.append(rev);
    }

    return result;
}

QVariantMap ClassMappingService::previewMapping(const QString &datasetId, const QVariantMap &mappingRules)
{
    QVariantMap result;
    result["totalLabels"] = 0;
    result["affectedLabels"] = 0;

    if (datasetId.isEmpty()) {
        qWarning() << "ClassMappingService::previewMapping: datasetId is empty";
        return result;
    }

    if (mappingRules.isEmpty()) {
        qWarning() << "ClassMappingService::previewMapping: mappingRules is empty";
        return result;
    }

    // Step 1: Get the source schema for this dataset
    QVariantMap schema = getSourceSchema(datasetId);
    if (schema.isEmpty()) {
        qWarning() << "ClassMappingService::previewMapping: no source schema found for dataset" << datasetId;
        return result;
    }

    QString sourceSchemaId = schema["id"].toString();

    // Step 2: Build source index -> name map
    QMap<int, QString> sourceIndexName = buildSourceIndexNameMap(sourceSchemaId);
    if (sourceIndexName.isEmpty()) {
        qWarning() << "ClassMappingService::previewMapping: empty source class index map";
        return result;
    }

    // Step 3: Build source index -> target class name via mapping rules
    // For each source class index, look up its name, then look up the mapped target name
    QMap<int, QString> sourceIndexToTargetName;
    for (auto it = sourceIndexName.constBegin(); it != sourceIndexName.constEnd(); ++it) {
        QString sourceName = it.value();
        if (mappingRules.contains(sourceName)) {
            sourceIndexToTargetName[it.key()] = mappingRules[sourceName].toString();
        } else {
            // Unmapped source classes keep their original name
            sourceIndexToTargetName[it.key()] = sourceName;
        }
    }

    // Step 4: Read all label files for this dataset and simulate
    QSqlQuery sampleQuery(Database::instance().database());
    sampleQuery.prepare("SELECT label_path FROM dataset_samples WHERE dataset_id = ? AND label_path IS NOT NULL");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        qWarning() << "ClassMappingService::previewMapping: sample query failed:" << sampleQuery.lastError().text();
        return result;
    }

    int totalLabels = 0;
    int affectedLabels = 0;
    QMap<QString, int> newClassDistribution;

    while (sampleQuery.next()) {
        QString labelPath = sampleQuery.value(0).toString();
        if (labelPath.isEmpty()) continue;

        QFile file(labelPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "ClassMappingService::previewMapping: cannot open label file:" << labelPath;
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() < 1) continue;

            bool ok = false;
            int classId = parts[0].toInt(&ok);
            if (!ok || classId < 0) continue;

            totalLabels++;

            // Determine the target class name
            QString targetName;
            if (sourceIndexToTargetName.contains(classId)) {
                targetName = sourceIndexToTargetName[classId];
            } else {
                // Unknown class_id, treat as unmapped
                targetName = QStringLiteral("class_%1").arg(classId);
            }

            // Check if this label is affected (class changed)
            QString sourceName;
            if (sourceIndexName.contains(classId)) {
                sourceName = sourceIndexName[classId];
            }
            if (targetName != sourceName) {
                affectedLabels++;
            }

            // Count distribution
            newClassDistribution[targetName]++;
        }

        file.close();
    }

    // Build the newClassDistribution as QVariantMap
    QVariantMap distributionVariant;
    for (auto it = newClassDistribution.constBegin(); it != newClassDistribution.constEnd(); ++it) {
        distributionVariant[it.key()] = it.value();
    }

    result["totalLabels"] = totalLabels;
    result["affectedLabels"] = affectedLabels;
    result["newClassDistribution"] = distributionVariant;

    qDebug() << "ClassMappingService::previewMapping: totalLabels" << totalLabels
             << "affectedLabels" << affectedLabels
             << "distinct target classes" << newClassDistribution.size();

    return result;
}

bool ClassMappingService::applyMapping(const QString &mappingRevisionId)
{
    if (mappingRevisionId.isEmpty()) {
        qWarning() << "ClassMappingService::applyMapping: mappingRevisionId is empty";
        return false;
    }

    // Step 1: Read the mapping revision
    QSqlQuery revQuery(Database::instance().database());
    revQuery.prepare("SELECT dataset_id, source_schema_id, target_taxonomy_id, mapping_rules_json "
                     "FROM class_mapping_revisions WHERE id = ?");
    revQuery.addBindValue(mappingRevisionId);

    if (!revQuery.exec()) {
        qWarning() << "ClassMappingService::applyMapping: revision query failed:" << revQuery.lastError().text();
        return false;
    }

    if (!revQuery.next()) {
        qWarning() << "ClassMappingService::applyMapping: revision not found:" << mappingRevisionId;
        return false;
    }

    QString datasetId = revQuery.value(0).toString();
    QString sourceSchemaId = revQuery.value(1).toString();
    QString targetTaxonomyId = revQuery.value(2).toString();
    QString mappingRulesJson = revQuery.value(3).toString();

    // Step 2: Parse the mapping rules JSON
    QJsonDocument rulesDoc = QJsonDocument::fromJson(mappingRulesJson.toUtf8());
    if (!rulesDoc.isObject()) {
        qWarning() << "ClassMappingService::applyMapping: invalid mapping_rules_json";
        return false;
    }
    QJsonObject rulesObj = rulesDoc.object();

    // Convert to QVariantMap for convenience
    QVariantMap mappingRules = rulesObj.toVariantMap();

    // Step 3: Build source index -> name map
    QMap<int, QString> sourceIndexName = buildSourceIndexNameMap(sourceSchemaId);
    if (sourceIndexName.isEmpty()) {
        qWarning() << "ClassMappingService::applyMapping: empty source class index map";
        return false;
    }

    // Step 4: Build target name -> index map from taxonomy
    QMap<QString, int> targetNameIndex = buildTargetNameIndexMap(targetTaxonomyId);

    // Step 5: Check for target class names not yet in taxonomy, add them
    bool taxonomyUpdated = false;
    QVariantList taxonomyClasses;

    // Load current taxonomy classes
    {
        QSqlQuery taxQuery(Database::instance().database());
        taxQuery.prepare("SELECT class_definitions_json FROM taxonomies WHERE id = ?");
        taxQuery.addBindValue(targetTaxonomyId);
        if (taxQuery.exec() && taxQuery.next()) {
            QJsonDocument taxDoc = QJsonDocument::fromJson(taxQuery.value(0).toString().toUtf8());
            for (const auto &v : taxDoc.array()) {
                taxonomyClasses.append(v.toString());
            }
        }
    }

    // For each mapped target class name, ensure it exists in the taxonomy
    for (auto it = mappingRules.constBegin(); it != mappingRules.constEnd(); ++it) {
        QString targetName = it.value().toString();
        if (!targetNameIndex.contains(targetName)) {
            // Add new class to taxonomy
            int newIndex = taxonomyClasses.size();
            taxonomyClasses.append(targetName);
            targetNameIndex[targetName] = newIndex;
            taxonomyUpdated = true;

            qDebug() << "ClassMappingService::applyMapping: adding new class to taxonomy:" << targetName
                     << "at index" << newIndex;
        }
    }

    // Update taxonomy in database if new classes were added
    if (taxonomyUpdated) {
        QJsonArray taxArray;
        for (const auto &c : taxonomyClasses) {
            taxArray.append(c.toString());
        }
        QString updatedClassesJson = QJsonDocument(taxArray).toJson(QJsonDocument::Compact);

        QSqlQuery updateTaxQuery(Database::instance().database());
        updateTaxQuery.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
        updateTaxQuery.addBindValue(updatedClassesJson);
        updateTaxQuery.addBindValue(targetTaxonomyId);

        if (!updateTaxQuery.exec()) {
            qWarning() << "ClassMappingService::applyMapping: failed to update taxonomy:" << updateTaxQuery.lastError().text();
            return false;
        }

        qDebug() << "ClassMappingService::applyMapping: taxonomy updated with new classes, now"
                 << taxonomyClasses.size() << "classes";
    }

    // Step 6: Build source class index -> target class index remapping
    QMap<int, int> indexRemap;
    for (auto it = sourceIndexName.constBegin(); it != sourceIndexName.constEnd(); ++it) {
        int sourceIndex = it.key();
        QString sourceName = it.value();

        // Look up the mapped target class name
        QString targetName;
        if (mappingRules.contains(sourceName)) {
            targetName = mappingRules[sourceName].toString();
        } else {
            // Unmapped source class: try to keep same index if possible, or use same name
            targetName = sourceName;
        }

        // Look up the target class index
        if (targetNameIndex.contains(targetName)) {
            indexRemap[sourceIndex] = targetNameIndex[targetName];
        } else {
            // This should not happen since we added all missing classes above
            qWarning() << "ClassMappingService::applyMapping: target class name not found in taxonomy:" << targetName;
            // Fall back: keep original index
            indexRemap[sourceIndex] = sourceIndex;
        }
    }

    qDebug() << "ClassMappingService::applyMapping: built index remap with" << indexRemap.size() << "entries";
    for (auto it = indexRemap.constBegin(); it != indexRemap.constEnd(); ++it) {
        qDebug() << "  class" << it.key() << "->" << it.value();
    }

    // Step 7: Remap all label files for this dataset
    QSqlQuery sampleQuery(Database::instance().database());
    sampleQuery.prepare("SELECT label_path FROM dataset_samples WHERE dataset_id = ? AND label_path IS NOT NULL");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        qWarning() << "ClassMappingService::applyMapping: sample query failed:" << sampleQuery.lastError().text();
        return false;
    }

    int totalFiles = 0;
    int totalLabelsRemapped = 0;
    int totalLabelsAffected = 0;

    while (sampleQuery.next()) {
        QString labelPath = sampleQuery.value(0).toString();
        if (labelPath.isEmpty()) continue;

        int affectedCount = 0;
        int totalCount = 0;
        QMap<int, int> newClassCounts;

        if (!remapLabelFile(labelPath, indexRemap, affectedCount, totalCount, newClassCounts)) {
            qWarning() << "ClassMappingService::applyMapping: failed to remap label file:" << labelPath;
            // Continue processing other files rather than aborting entirely
            continue;
        }

        totalFiles++;
        totalLabelsRemapped += totalCount;
        totalLabelsAffected += affectedCount;
    }

    qDebug() << "ClassMappingService::applyMapping: completed for revision" << mappingRevisionId
             << "- processed" << totalFiles << "label files,"
             << totalLabelsRemapped << "total labels,"
             << totalLabelsAffected << "labels affected by remapping";

    return true;
}

QMap<int, QString> ClassMappingService::buildSourceIndexNameMap(const QString &sourceSchemaId)
{
    QMap<int, QString> result;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT raw_class_names_json FROM imported_label_schemas WHERE id = ?");
    query.addBindValue(sourceSchemaId);

    if (!query.exec()) {
        qWarning() << "ClassMappingService::buildSourceIndexNameMap: query failed:" << query.lastError().text();
        return result;
    }

    if (!query.next()) {
        qWarning() << "ClassMappingService::buildSourceIndexNameMap: schema not found:" << sourceSchemaId;
        return result;
    }

    QString classNamesJson = query.value(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(classNamesJson.toUtf8());

    if (!doc.isArray()) {
        qWarning() << "ClassMappingService::buildSourceIndexNameMap: raw_class_names_json is not an array";
        return result;
    }

    QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        result[i] = arr[i].toString();
    }

    return result;
}

QMap<QString, int> ClassMappingService::buildTargetNameIndexMap(const QString &taxonomyId)
{
    QMap<QString, int> result;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT class_definitions_json FROM taxonomies WHERE id = ?");
    query.addBindValue(taxonomyId);

    if (!query.exec()) {
        qWarning() << "ClassMappingService::buildTargetNameIndexMap: query failed:" << query.lastError().text();
        return result;
    }

    if (!query.next()) {
        qWarning() << "ClassMappingService::buildTargetNameIndexMap: taxonomy not found:" << taxonomyId;
        return result;
    }

    QString classesJson = query.value(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(classesJson.toUtf8());

    if (!doc.isArray()) {
        qWarning() << "ClassMappingService::buildTargetNameIndexMap: class_definitions_json is not an array";
        return result;
    }

    QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        result[arr[i].toString()] = i;
    }

    return result;
}

bool ClassMappingService::remapLabelFile(const QString &filePath, const QMap<int, int> &indexRemap,
                                          int &affectedCount, int &totalCount, QMap<int, int> &newClassCounts)
{
    affectedCount = 0;
    totalCount = 0;
    newClassCounts.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ClassMappingService::remapLabelFile: cannot open file:" << filePath;
        return false;
    }

    QStringList outputLines;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine();

        // Preserve trailing whitespace structure by only trimming for parsing
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            // Preserve empty lines
            outputLines.append(line);
            continue;
        }

        QStringList parts = trimmed.split(QChar(' '), Qt::SkipEmptyParts);
        if (parts.size() != 5 && parts.size() != 9) {
            // Not a valid YOLO label line (HBB=5 or OBB=9); keep as-is
            outputLines.append(line);
            continue;
        }

        bool ok = false;
        int classId = parts[0].toInt(&ok);
        if (!ok || classId < 0) {
            // Invalid class_id; keep as-is
            outputLines.append(line);
            continue;
        }

        totalCount++;

        // Remap the class ID
        int newClassId = classId;
        if (indexRemap.contains(classId)) {
            newClassId = indexRemap[classId];
        }

        if (newClassId != classId) {
            affectedCount++;
        }

        newClassCounts[newClassId]++;

        // Reconstruct the line with the new class ID
        parts[0] = QString::number(newClassId);
        outputLines.append(parts.join(QChar(' ')));
    }

    file.close();

    // Write the remapped content to a temp file first, then rename (atomic write)
    QString tempPath = filePath + QStringLiteral(".tmp");
    QFile tempFile(tempPath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "ClassMappingService::remapLabelFile: cannot write temp file:" << tempPath;
        return false;
    }

    QTextStream out(&tempFile);
    for (int i = 0; i < outputLines.size(); ++i) {
        out << outputLines[i];
        if (i < outputLines.size() - 1) {
            out << "\n";
        }
    }
    out.flush();
    tempFile.close();

    // Remove original and rename temp to original
    QFile::remove(filePath);
    if (!QFile::rename(tempPath, filePath)) {
        qWarning() << "ClassMappingService::remapLabelFile: failed to rename temp file to:" << filePath;
        // Try to restore from temp if rename failed
        QFile::rename(tempPath, filePath);
        return false;
    }

    return true;
}
