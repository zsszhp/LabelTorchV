#include "DatasetService.h"
#include "ImportScanner.h"
#include "database/Database.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

DatasetService::DatasetService(QObject *parent)
    : QObject(parent)
    , m_scanner(new ImportScanner(this))
{
}

QString DatasetService::importDataset(const QString &projectId, const QString &name,
                                      const QString &imageDir, const QString &labelDir)
{
    if (projectId.isEmpty() || name.isEmpty() || imageDir.isEmpty() || labelDir.isEmpty()) {
        qWarning() << "DatasetService::importDataset: missing required parameters";
        return {};
    }

    // Step 1: Create the dataset record with 'scanning' status
    QString datasetId = Id::generate();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                  "VALUES (?, ?, ?, ?, ?, 'yolo_txt', 0, 'scanning')");
    query.addBindValue(datasetId);
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(imageDir);
    query.addBindValue(labelDir);

    if (!query.exec()) {
        qWarning() << "DatasetService: Failed to create dataset record:" << query.lastError().text();
        return {};
    }

    qDebug() << "DatasetService: Dataset record created:" << datasetId << name << "- scanning...";

    // Step 2: Run the scan
    QVariantMap scanResult = m_scanner->scan(imageDir, labelDir);

    if (scanResult.contains("error")) {
        qWarning() << "DatasetService: Scan failed:" << scanResult["error"].toString();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    int matched = scanResult["matched"].toInt();
    QVariantList samples = scanResult["samples"].toList();

    // Filter to only matched (valid) samples
    QVariantList matchedSamples;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString status = sample["status"].toString();
        if (status == QStringLiteral("matched") || status == QStringLiteral("invalid_label")) {
            matchedSamples.append(sample);
        }
    }

    // Step 3: Update status to 'importing'
    if (!updateImportStatus(datasetId, QStringLiteral("importing"))) {
        qWarning() << "DatasetService: Failed to update status to importing";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    // Step 4: Insert matched samples
    if (!insertSamples(datasetId, matchedSamples)) {
        qWarning() << "DatasetService: Failed to insert samples";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    // Step 5: Extract and store class schema
    if (!extractAndStoreSchema(datasetId, matchedSamples)) {
        qWarning() << "DatasetService: Failed to extract class schema";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    // Step 6: Update sample count and finalize status
    QSqlQuery updateQuery(Database::instance().database());
    updateQuery.prepare("UPDATE datasets SET sample_count = ?, import_status = 'completed' WHERE id = ?");
    updateQuery.addBindValue(matchedSamples.size());
    updateQuery.addBindValue(datasetId);

    if (!updateQuery.exec()) {
        qWarning() << "DatasetService: Failed to finalize dataset:" << updateQuery.lastError().text();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    qDebug() << "DatasetService: Import completed:" << datasetId
             << "with" << matchedSamples.size() << "samples";
    return datasetId;
}

QVariantList DatasetService::listDatasets(const QString &projectId)
{
    QVariantList result;
    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, project_id, name, image_root, label_root, format, "
                  "sample_count, import_status, created_at "
                  "FROM datasets WHERE project_id = ? ORDER BY created_at DESC");
    query.addBindValue(projectId);

    if (!query.exec()) {
        qWarning() << "DatasetService::listDatasets failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap d;
        d["id"] = query.value(0);
        d["projectId"] = query.value(1);
        d["name"] = query.value(2);
        d["imageRoot"] = query.value(3);
        d["labelRoot"] = query.value(4);
        d["format"] = query.value(5);
        d["sampleCount"] = query.value(6);
        d["importStatus"] = query.value(7);
        d["createdAt"] = query.value(8);
        result.append(d);
    }
    return result;
}

QVariantMap DatasetService::getDataset(const QString &datasetId)
{
    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, project_id, name, image_root, label_root, format, "
                  "sample_count, import_status, created_at "
                  "FROM datasets WHERE id = ?");
    query.addBindValue(datasetId);

    if (query.exec() && query.next()) {
        QVariantMap d;
        d["id"] = query.value(0);
        d["projectId"] = query.value(1);
        d["name"] = query.value(2);
        d["imageRoot"] = query.value(3);
        d["labelRoot"] = query.value(4);
        d["format"] = query.value(5);
        d["sampleCount"] = query.value(6);
        d["importStatus"] = query.value(7);
        d["createdAt"] = query.value(8);
        return d;
    }
    return {};
}

bool DatasetService::deleteDataset(const QString &datasetId)
{
    QSqlDatabase db = Database::instance().database();

    // Delete imported_label_schemas first (depends on dataset)
    QSqlQuery schemaQuery(db);
    schemaQuery.prepare("DELETE FROM imported_label_schemas WHERE dataset_id = ?");
    schemaQuery.addBindValue(datasetId);
    if (!schemaQuery.exec()) {
        qWarning() << "DatasetService: Failed to delete label schemas:" << schemaQuery.lastError().text();
        return false;
    }

    // Delete dataset_samples
    QSqlQuery samplesQuery(db);
    samplesQuery.prepare("DELETE FROM dataset_samples WHERE dataset_id = ?");
    samplesQuery.addBindValue(datasetId);
    if (!samplesQuery.exec()) {
        qWarning() << "DatasetService: Failed to delete samples:" << samplesQuery.lastError().text();
        return false;
    }

    // Delete the dataset itself
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("DELETE FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec()) {
        qWarning() << "DatasetService: Failed to delete dataset:" << datasetQuery.lastError().text();
        return false;
    }

    qDebug() << "DatasetService: Deleted dataset and associated records:" << datasetId;
    return true;
}

QVariantMap DatasetService::getSampleStats(const QString &datasetId)
{
    QVariantMap result;
    result["totalSamples"] = 0;
    result["validSamples"] = 0;
    result["invalidSamples"] = 0;
    result["labeledSamples"] = 0;
    result["unlabeledSamples"] = 0;

    if (datasetId.isEmpty()) {
        qWarning() << "DatasetService::getSampleStats: datasetId is empty";
        return result;
    }

    QSqlDatabase db = Database::instance().database();

    // Get sample counts
    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT "
                       "  COUNT(*) AS total, "
                       "  SUM(CASE WHEN validation_status = 'valid' THEN 1 ELSE 0 END) AS valid_count, "
                       "  SUM(CASE WHEN validation_status != 'valid' OR validation_status IS NULL THEN 1 ELSE 0 END) AS invalid_count, "
                       "  SUM(CASE WHEN label_path IS NOT NULL AND label_path != '' THEN 1 ELSE 0 END) AS labeled_count, "
                       "  SUM(CASE WHEN label_path IS NULL OR label_path = '' THEN 1 ELSE 0 END) AS unlabeled_count "
                       "FROM dataset_samples WHERE dataset_id = ?");
    countQuery.addBindValue(datasetId);

    if (!countQuery.exec()) {
        qWarning() << "DatasetService::getSampleStats: count query failed:" << countQuery.lastError().text();
        return result;
    }

    if (countQuery.next()) {
        result["totalSamples"] = countQuery.value(0).toInt();
        result["validSamples"] = countQuery.value(1).toInt();
        result["invalidSamples"] = countQuery.value(2).toInt();
        result["labeledSamples"] = countQuery.value(3).toInt();
        result["unlabeledSamples"] = countQuery.value(4).toInt();
    }

    // Read all label files and count class IDs and annotation counts per sample
    QVariantMap classDist;
    QVariantMap densityMap;
    densityMap["min"] = 0;
    densityMap["max"] = 0;
    densityMap["avg"] = 0.0;
    densityMap["median"] = 0;

    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id, label_path FROM dataset_samples WHERE dataset_id = ?");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        qWarning() << "DatasetService::getSampleStats: sample query failed:" << sampleQuery.lastError().text();
        result["classDistribution"] = classDist;
        result["annotationDensity"] = densityMap;
        return result;
    }

    QMap<int, int> classCounts;
    QList<int> annotationCounts;

    while (sampleQuery.next()) {
        QString labelPath = sampleQuery.value(1).toString();

        if (labelPath.isEmpty()) continue;

        QFile file(labelPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        int annotCount = 0;
        QTextStream in(&file);

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                bool ok = false;
                int classId = parts[0].toInt(&ok);
                if (ok && classId >= 0) {
                    classCounts[classId]++;
                    annotCount++;
                }
            }
        }

        file.close();
        annotationCounts.append(annotCount);
    }

    // Build class distribution variant map
    for (auto it = classCounts.constBegin(); it != classCounts.constEnd(); ++it) {
        classDist[QString::number(it.key())] = it.value();
    }
    result["classDistribution"] = classDist;

    // Compute annotation density stats
    if (!annotationCounts.isEmpty()) {
        std::sort(annotationCounts.begin(), annotationCounts.end());
        int minCount = annotationCounts.first();
        int maxCount = annotationCounts.last();
        double avgCount = 0.0;
        for (int c : annotationCounts) avgCount += c;
        avgCount /= annotationCounts.size();
        int medianCount = 0;
        int n = annotationCounts.size();
        if (n % 2 == 0) {
            medianCount = (annotationCounts[n / 2 - 1] + annotationCounts[n / 2]) / 2;
        } else {
            medianCount = annotationCounts[n / 2];
        }
        densityMap["min"] = minCount;
        densityMap["max"] = maxCount;
        densityMap["avg"] = qRound(avgCount * 100.0) / 100.0;
        densityMap["median"] = medianCount;
    }
    result["annotationDensity"] = densityMap;

    qDebug() << "DatasetService::getSampleStats: dataset" << datasetId
             << "total:" << result["totalSamples"] << "valid:" << result["validSamples"]
             << "classDist size:" << classDist.size();

    return result;
}

QVariantMap DatasetService::detectAnomalies(const QString &datasetId)
{
    QVariantMap result;
    QVariantList emptyLabels;
    QVariantList classErrors;
    QVariantList sizeAnomalies;
    QVariantList duplicateImages;

    if (datasetId.isEmpty()) {
        qWarning() << "DatasetService::detectAnomalies: datasetId is empty";
        result["emptyLabels"] = emptyLabels;
        result["classErrors"] = classErrors;
        result["sizeAnomalies"] = sizeAnomalies;
        result["duplicateImages"] = duplicateImages;
        result["totalAnomalies"] = 0;
        return result;
    }

    QSqlDatabase db = Database::instance().database();

    // Determine valid class range from imported_label_schemas
    int maxClassId = -1;
    QSqlQuery schemaQuery(db);
    schemaQuery.prepare("SELECT raw_class_names_json FROM imported_label_schemas WHERE dataset_id = ?");
    schemaQuery.addBindValue(datasetId);

    if (schemaQuery.exec() && schemaQuery.next()) {
        QString classNamesJson = schemaQuery.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(classNamesJson.toUtf8());
        if (doc.isArray()) {
            maxClassId = doc.array().size() - 1;
        }
    }

    // Check for duplicate hashes
    QSqlQuery hashQuery(db);
    hashQuery.prepare("SELECT id, hash FROM dataset_samples WHERE dataset_id = ? AND hash IS NOT NULL AND hash != ''");
    hashQuery.addBindValue(datasetId);

    QMap<QString, QStringList> hashToIds;
    if (hashQuery.exec()) {
        while (hashQuery.next()) {
            QString sampleId = hashQuery.value(0).toString();
            QString hash = hashQuery.value(1).toString();
            hashToIds[hash].append(sampleId);
        }
    }

    for (auto it = hashToIds.constBegin(); it != hashToIds.constEnd(); ++it) {
        if (it.value().size() > 1) {
            for (const auto &sid : it.value()) {
                duplicateImages.append(sid);
            }
        }
    }

    // Scan label files for empty labels and class errors
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id, label_path FROM dataset_samples WHERE dataset_id = ? AND label_path IS NOT NULL AND label_path != ''");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        qWarning() << "DatasetService::detectAnomalies: sample query failed:" << sampleQuery.lastError().text();
        result["emptyLabels"] = emptyLabels;
        result["classErrors"] = classErrors;
        result["sizeAnomalies"] = sizeAnomalies;
        result["duplicateImages"] = duplicateImages;
        result["totalAnomalies"] = emptyLabels.size() + classErrors.size() + sizeAnomalies.size() + duplicateImages.size();
        return result;
    }

    while (sampleQuery.next()) {
        QString sampleId = sampleQuery.value(0).toString();
        QString labelPath = sampleQuery.value(1).toString();

        QFile file(labelPath);
        if (!file.exists() || file.size() == 0) {
            emptyLabels.append(sampleId);
            continue;
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emptyLabels.append(sampleId);
            continue;
        }

        bool hasValidLines = false;
        bool hasClassError = false;
        QTextStream in(&file);

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            hasValidLines = true;
            QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                bool ok = false;
                int classId = parts[0].toInt(&ok);
                if (ok && maxClassId >= 0 && classId > maxClassId) {
                    hasClassError = true;
                }
            }
        }

        file.close();

        if (!hasValidLines) {
            emptyLabels.append(sampleId);
        } else if (hasClassError) {
            classErrors.append(sampleId);
        }
    }

    // Size anomalies: placeholder (width/height not typically populated)
    // Could be extended later when image dimensions are populated in DB

    result["emptyLabels"] = emptyLabels;
    result["classErrors"] = classErrors;
    result["sizeAnomalies"] = sizeAnomalies;
    result["duplicateImages"] = duplicateImages;
    result["totalAnomalies"] = emptyLabels.size() + classErrors.size() + sizeAnomalies.size() + duplicateImages.size();

    qDebug() << "DatasetService::detectAnomalies: dataset" << datasetId
             << "emptyLabels:" << emptyLabels.size()
             << "classErrors:" << classErrors.size()
             << "duplicateImages:" << duplicateImages.size()
             << "total:" << result["totalAnomalies"];

    return result;
}

QVariantList DatasetService::getClassDistribution(const QString &datasetId)
{
    QVariantList result;

    if (datasetId.isEmpty()) {
        qWarning() << "DatasetService::getClassDistribution: datasetId is empty";
        return result;
    }

    // Get class names from imported_label_schemas
    QSqlDatabase db = Database::instance().database();
    QMap<int, QString> classNames;
    int maxClassId = -1;

    QSqlQuery schemaQuery(db);
    schemaQuery.prepare("SELECT raw_class_names_json FROM imported_label_schemas WHERE dataset_id = ?");
    schemaQuery.addBindValue(datasetId);

    if (schemaQuery.exec() && schemaQuery.next()) {
        QString classNamesJson = schemaQuery.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(classNamesJson.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (int i = 0; i < arr.size(); ++i) {
                classNames[i] = arr[i].toString();
            }
            maxClassId = arr.size() - 1;
        }
    }

    // Count class IDs from label files
    QMap<int, int> classCounts;

    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT label_path FROM dataset_samples WHERE dataset_id = ? AND label_path IS NOT NULL AND label_path != ''");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        qWarning() << "DatasetService::getClassDistribution: sample query failed:" << sampleQuery.lastError().text();
        return result;
    }

    while (sampleQuery.next()) {
        QString labelPath = sampleQuery.value(0).toString();
        if (labelPath.isEmpty()) continue;

        QFile file(labelPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                bool ok = false;
                int classId = parts[0].toInt(&ok);
                if (ok && classId >= 0) {
                    classCounts[classId]++;
                }
            }
        }

        file.close();
    }

    // Build result list, ordered by count descending
    QList<QPair<int, int>> sortedCounts;
    for (auto it = classCounts.constBegin(); it != classCounts.constEnd(); ++it) {
        sortedCounts.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sortedCounts.begin(), sortedCounts.end(),
              [](const QPair<int, int> &a, const QPair<int, int> &b) {
                  return a.second > b.second;
              });

    for (const auto &pair : sortedCounts) {
        QVariantMap entry;
        entry["classId"] = pair.first;
        entry["className"] = classNames.value(pair.first, QStringLiteral("class_%1").arg(pair.first));
        entry["count"] = pair.second;
        result.append(entry);
    }

    qDebug() << "DatasetService::getClassDistribution: dataset" << datasetId
             << "classes:" << result.size();

    return result;
}

bool DatasetService::updateImportStatus(const QString &datasetId, const QString &status)
{
    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE datasets SET import_status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(datasetId);

    if (!query.exec()) {
        qWarning() << "DatasetService: Failed to update import status:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatasetService::insertSamples(const QString &datasetId, const QVariantList &samples)
{
    QSqlDatabase db = Database::instance().database();

    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString sampleId = Id::generate();

        QSqlQuery query(db);
        query.prepare("INSERT INTO dataset_samples "
                      "(id, dataset_id, image_path, label_path, validation_status, error_code) "
                      "VALUES (?, ?, ?, ?, ?, ?)");
        query.addBindValue(sampleId);
        query.addBindValue(datasetId);
        query.addBindValue(sample["imagePath"].toString());
        query.addBindValue(sample["labelPath"].toString());

        bool valid = sample["valid"].toBool();
        query.addBindValue(valid ? QStringLiteral("valid") : QStringLiteral("invalid_label"));

        // Store parse errors in error_code if any
        if (sample.contains("errors")) {
            QStringList errorList = sample["errors"].toStringList();
            if (!errorList.isEmpty()) {
                query.addBindValue(errorList.join("; "));
            } else {
                query.addBindValue(QVariant());
            }
        } else {
            query.addBindValue(QVariant());
        }

        if (!query.exec()) {
            qWarning() << "DatasetService: Failed to insert sample:" << query.lastError().text();
            return false;
        }
    }

    qDebug() << "DatasetService: Inserted" << samples.size() << "samples for dataset" << datasetId;
    return true;
}

bool DatasetService::extractAndStoreSchema(const QString &datasetId, const QVariantList &samples)
{
    // Collect all class IDs from all samples
    QSet<int> allClassIds;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QVariantList classIds = sample["classIds"].toList();
        for (const auto &cid : classIds) {
            allClassIds.insert(cid.toInt());
        }
    }

    if (allClassIds.isEmpty()) {
        qDebug() << "DatasetService: No class IDs found in any label file";
        // Still store an empty schema
    }

    // Determine max class_id and build class name list
    int maxClassId = 0;
    for (int cid : allClassIds) {
        if (cid > maxClassId) maxClassId = cid;
    }

    // Build class names list: ["class_0", "class_1", ..., "class_N"]
    QStringList classNames;
    for (int i = 0; i <= maxClassId; ++i) {
        classNames.append(QStringLiteral("class_%1").arg(i));
    }

    // Build class order list: indices that actually appeared
    QVariantList classOrder;
    QList<int> sortedIds = allClassIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classOrder.append(cid);
    }

    // Serialize to JSON
    QJsonArray classNamesArray;
    for (const auto &name : classNames) classNamesArray.append(name);
    QString classNamesJson = QJsonDocument(classNamesArray).toJson(QJsonDocument::Compact);

    QJsonArray classOrderArray;
    for (const auto &idx : classOrder) classOrderArray.append(idx.toInt());
    QString classOrderJson = QJsonDocument(classOrderArray).toJson(QJsonDocument::Compact);

    // Insert into imported_label_schemas
    QString schemaId = Id::generate();
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO imported_label_schemas "
                  "(id, dataset_id, raw_class_names_json, raw_class_order_json, source_format) "
                  "VALUES (?, ?, ?, ?, 'yolo_txt')");
    query.addBindValue(schemaId);
    query.addBindValue(datasetId);
    query.addBindValue(classNamesJson);
    query.addBindValue(classOrderJson);

    if (!query.exec()) {
        qWarning() << "DatasetService: Failed to insert label schema:" << query.lastError().text();
        return false;
    }

    qDebug() << "DatasetService: Extracted schema with" << classNames.size()
             << "class names, appearing class IDs:" << sortedIds;
    return true;
}
