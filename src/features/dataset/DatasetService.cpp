#include "DatasetService.h"
#include "ImportScanner.h"
#include "database/Database.h"
#include "utils/Id.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

DatasetService::DatasetService(QObject *parent)
    : QObject(parent)
    , m_scanner(new ImportScanner(this))
{
    ltTrace(LT_LOG_DATASET()) << "DatasetService parent=" << parent;
}

QString DatasetService::importDataset(const QString &projectId, const QString &name,
                                      const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "importDataset projectId=" << projectId << "name=" << name
                              << "imageDir=" << imageDir << "labelDir=" << labelDir;

    if (projectId.isEmpty() || name.isEmpty() || imageDir.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "importDataset: 缺少必要参数";
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "导入开始: name=" << name << "imageDir=" << imageDir << "labelDir=" << labelDir;

    // 判断是否有标签目录
    bool hasLabelDir = !labelDir.isEmpty() && QDir(labelDir).exists();
    QString format = hasLabelDir ? QStringLiteral("yolo_txt") : QStringLiteral("image_only");

    // 步骤1: 创建数据集记录，状态为 scanning
    QString datasetId = Id::generate();

    // 使用事务保证原子性：多表关联操作必须在同一事务内完成
    auto db = Database::instance().database();
    if (!db.transaction()) {
        ltError(LT_LOG_DATASET()) << "Failed to start transaction";
        return {};
    }

    QSqlQuery query(db);
    query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                  "VALUES (?, ?, ?, ?, ?, ?, 0, 'scanning')");
    query.addBindValue(datasetId);
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(imageDir);
    query.addBindValue(hasLabelDir ? labelDir : QString());
    query.addBindValue(format);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "创建数据集记录失败:" << query.lastError().text();
        db.rollback();
        return {};
    }

    ltDebug(LT_LOG_DATASET()) << "数据集记录已创建:" << datasetId << name << "- 扫描中...";

    // 步骤2: 执行扫描
    QVariantMap scanResult = m_scanner->scan(imageDir, hasLabelDir ? labelDir : QString());

    if (scanResult.contains("error")) {
        ltError(LT_LOG_DATASET()) << "扫描失败:" << scanResult["error"].toString();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        db.rollback();
        return {};
    }

    int matched = scanResult["matched"].toInt();
    QVariantList samples = scanResult["samples"].toList();

    // 过滤有效样本：匹配的 + 无标签图片 + 标签无效但有图片的
    QVariantList matchedSamples;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString status = sample["status"].toString();
        if (status == QStringLiteral("matched")
            || status == QStringLiteral("invalid_label")
            || status == QStringLiteral("unmatched_image")) {
            matchedSamples.append(sample);
        }
    }

    // 步骤3: 更新状态为导入中
    if (!updateImportStatus(datasetId, QStringLiteral("importing"))) {
        ltError(LT_LOG_DATASET()) << "更新状态为导入中失败";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        db.rollback();
        return {};
    }

    // 步骤4: 插入匹配的样本
    if (!insertSamples(datasetId, matchedSamples)) {
        ltError(LT_LOG_DATASET()) << "插入样本失败";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        db.rollback();
        return {};
    }

    // 步骤5: 提取并存储类别体系（仅有标签样本时提取）
    QVariantList labeledSamples;
    for (const auto &s : matchedSamples) {
        QVariantMap sample = s.toMap();
        if (sample.contains("classIds") && !sample["classIds"].toList().isEmpty()) {
            labeledSamples.append(sample);
        }
    }
    if (!labeledSamples.isEmpty()) {
        if (!extractAndStoreSchema(datasetId, labeledSamples)) {
            ltError(LT_LOG_DATASET()) << "提取类别体系失败";
            updateImportStatus(datasetId, QStringLiteral("failed"));
            db.rollback();
            return {};
        }
    }

    // 步骤6: 更新样本数并完成状态
    QSqlQuery updateQuery(Database::instance().database());
    updateQuery.prepare("UPDATE datasets SET sample_count = ?, import_status = 'completed' WHERE id = ?");
    updateQuery.addBindValue(matchedSamples.size());
    updateQuery.addBindValue(datasetId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "完成数据集更新失败:" << updateQuery.lastError().text();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        db.rollback();
        return {};
    }

    // 步骤7: 将导入的类别同步到项目 taxonomy
    syncClassesToTaxonomy(datasetId);

    // 提交事务
    db.commit();

    ltInfo(LT_LOG_DATASET()) << "导入完成:" << datasetId
                             << "共" << matchedSamples.size() << "个样本";
    return datasetId;
}

QVariantList DatasetService::listDatasets(const QString &projectId)
{
    ltTrace(LT_LOG_DATASET()) << "listDatasets projectId=" << projectId;

    QVariantList result;
    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, project_id, name, image_root, label_root, format, "
                  "sample_count, import_status, created_at "
                  "FROM datasets WHERE project_id = ? ORDER BY created_at DESC");
    query.addBindValue(projectId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "listDatasets failed:" << query.lastError().text();
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

    ltDebug(LT_LOG_DATASET()) << "listDatasets: found" << result.size() << "datasets for project" << projectId;
    return result;
}

void DatasetService::scanFolderAsync(const QString &folderPath)
{
    ltInfo(LT_LOG_DATASET()) << "scanFolderAsync folderPath=" << folderPath;
    auto *self = this;
    QtConcurrent::run([self, folderPath]() {
        QVariantMap result = self->m_scanner->scanFolder(folderPath);
        QMetaObject::invokeMethod(self, [self, result]() {
            emit self->scanFolderFinished(result);
        }, Qt::QueuedConnection);
    });
}

void DatasetService::scanSeparateAsync(const QString &imageDir, const QString &labelDir)
{
    ltInfo(LT_LOG_DATASET()) << "scanSeparateAsync imageDir=" << imageDir << "labelDir=" << labelDir;
    auto *self = this;
    QtConcurrent::run([self, imageDir, labelDir]() {
        QVariantMap result = self->scanSeparate(imageDir, labelDir);
        QMetaObject::invokeMethod(self, [self, result]() {
            emit self->scanSeparateFinished(result);
        }, Qt::QueuedConnection);
    });
}

QVariantMap DatasetService::getDataset(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "getDataset datasetId=" << datasetId;

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
    ltTrace(LT_LOG_DATASET()) << "deleteDataset datasetId=" << datasetId;

    QSqlDatabase db = Database::instance().database();

    // Delete assisted_label_batches (references dataset)
    QSqlQuery assistedQuery(db);
    assistedQuery.prepare("DELETE FROM assisted_label_batches WHERE dataset_id = ?");
    assistedQuery.addBindValue(datasetId);
    if (!assistedQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete assisted_label_batches:" << assistedQuery.lastError().text();
        return false;
    }

    // Delete annotation_revisions (references dataset and dataset_samples)
    QSqlQuery annotQuery(db);
    annotQuery.prepare("DELETE FROM annotation_revisions WHERE dataset_id = ?");
    annotQuery.addBindValue(datasetId);
    if (!annotQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete annotation_revisions:" << annotQuery.lastError().text();
        return false;
    }

    // Delete class_mapping_revisions (references dataset)
    QSqlQuery mappingQuery(db);
    mappingQuery.prepare("DELETE FROM class_mapping_revisions WHERE dataset_id = ?");
    mappingQuery.addBindValue(datasetId);
    if (!mappingQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete class_mapping_revisions:" << mappingQuery.lastError().text();
        return false;
    }

    // Delete dataset_snapshots (references dataset)
    QSqlQuery snapshotQuery(db);
    snapshotQuery.prepare("DELETE FROM dataset_snapshots WHERE dataset_id = ?");
    snapshotQuery.addBindValue(datasetId);
    if (!snapshotQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete dataset_snapshots:" << snapshotQuery.lastError().text();
        return false;
    }

    // Delete imported_label_schemas (references dataset)
    QSqlQuery schemaQuery(db);
    schemaQuery.prepare("DELETE FROM imported_label_schemas WHERE dataset_id = ?");
    schemaQuery.addBindValue(datasetId);
    if (!schemaQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete label schemas:" << schemaQuery.lastError().text();
        return false;
    }

    // Delete dataset_samples (references dataset)
    QSqlQuery samplesQuery(db);
    samplesQuery.prepare("DELETE FROM dataset_samples WHERE dataset_id = ?");
    samplesQuery.addBindValue(datasetId);
    if (!samplesQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete samples:" << samplesQuery.lastError().text();
        return false;
    }

    // Finally delete the dataset itself
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("DELETE FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to delete dataset:" << datasetQuery.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "Deleted dataset and all associated records:" << datasetId;
    return true;
}


QVariantMap DatasetService::getSampleStats(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "getSampleStats datasetId=" << datasetId;

    QVariantMap result;
    result["totalSamples"] = 0;
    result["validSamples"] = 0;
    result["invalidSamples"] = 0;
    result["labeledSamples"] = 0;
    result["unlabeledSamples"] = 0;

    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "getSampleStats: datasetId is empty";
        return result;
    }

    QSqlDatabase db = Database::instance().database();

    // Get sample counts
    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT "
                       "  COUNT(*) AS total, "
                       "  SUM(CASE WHEN validation_status IN ('valid', 'good', 'defective') THEN 1 ELSE 0 END) AS valid_count, "
                       "  SUM(CASE WHEN validation_status NOT IN ('valid', 'good', 'defective') OR validation_status IS NULL THEN 1 ELSE 0 END) AS invalid_count, "
                       "  SUM(CASE WHEN label_path IS NOT NULL AND label_path != '' THEN 1 ELSE 0 END) AS labeled_count, "
                       "  SUM(CASE WHEN label_path IS NULL OR label_path = '' THEN 1 ELSE 0 END) AS unlabeled_count "
                       "FROM dataset_samples WHERE dataset_id = ?");
    countQuery.addBindValue(datasetId);

    if (!countQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "getSampleStats: count query failed:" << countQuery.lastError().text();
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
        ltError(LT_LOG_DATASET()) << "getSampleStats: sample query failed:" << sampleQuery.lastError().text();
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

    ltDebug(LT_LOG_DATASET()) << "getSampleStats: dataset" << datasetId
                              << "total:" << result["totalSamples"] << "valid:" << result["validSamples"]
                              << "classDist size:" << classDist.size();

    return result;
}

QVariantMap DatasetService::detectAnomalies(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "detectAnomalies datasetId=" << datasetId;

    QVariantMap result;
    QVariantList emptyLabels;
    QVariantList classErrors;
    QVariantList sizeAnomalies;
    QVariantList duplicateImages;

    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "detectAnomalies: datasetId is empty";
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
        ltError(LT_LOG_DATASET()) << "detectAnomalies: sample query failed:" << sampleQuery.lastError().text();
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

    ltDebug(LT_LOG_DATASET()) << "detectAnomalies: dataset" << datasetId
                              << "emptyLabels:" << emptyLabels.size()
                              << "classErrors:" << classErrors.size()
                              << "duplicateImages:" << duplicateImages.size()
                              << "total:" << result["totalAnomalies"];

    return result;
}

QVariantList DatasetService::getClassDistribution(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "getClassDistribution datasetId=" << datasetId;

    QVariantList result;

    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "getClassDistribution: datasetId is empty";
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
        ltError(LT_LOG_DATASET()) << "getClassDistribution: sample query failed:" << sampleQuery.lastError().text();
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

    ltDebug(LT_LOG_DATASET()) << "getClassDistribution: dataset" << datasetId
                              << "classes:" << result.size();

    return result;
}

bool DatasetService::updateImportStatus(const QString &datasetId, const QString &status)
{
    ltTrace(LT_LOG_DATASET()) << "updateImportStatus datasetId=" << datasetId << "status=" << status;

    QSqlQuery query(Database::instance().database());
    query.prepare("UPDATE datasets SET import_status = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(datasetId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to update import status:" << query.lastError().text();
        return false;
    }
    return true;
}

QVariantList DatasetService::listSamples(const QString &datasetId, int offset, int limit)
{
    ltTrace(LT_LOG_DATASET()) << "listSamples datasetId=" << datasetId << "offset=" << offset << "limit=" << limit;

    QVariantList result;
    if (datasetId.isEmpty()) return result;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, image_path, label_path, validation_status, split, width, height "
                  "FROM dataset_samples WHERE dataset_id = ? "
                  "ORDER BY image_path LIMIT ? OFFSET ?");
    query.addBindValue(datasetId);
    query.addBindValue(limit);
    query.addBindValue(offset);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "listSamples failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap s;
        s["id"] = query.value(0);
        s["imagePath"] = query.value(1);
        s["labelPath"] = query.value(2);
        s["validationStatus"] = query.value(3);
        s["split"] = query.value(4);
        s["width"] = query.value(5);
        s["height"] = query.value(6);
        result.append(s);
    }

    ltDebug(LT_LOG_DATASET()) << "listSamples: returned" << result.size() << "samples";
    return result;
}

int DatasetService::getSampleCount(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "getSampleCount datasetId=" << datasetId;

    if (datasetId.isEmpty()) return 0;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT COUNT(*) FROM dataset_samples WHERE dataset_id = ?");
    query.addBindValue(datasetId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool DatasetService::insertSamples(const QString &datasetId, const QVariantList &samples)
{
    ltTrace(LT_LOG_DATASET()) << "insertSamples datasetId=" << datasetId << "count=" << samples.size();

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
            ltError(LT_LOG_DATASET()) << "Failed to insert sample:" << query.lastError().text();
            return false;
        }
    }

    ltDebug(LT_LOG_DATASET()) << "Inserted" << samples.size() << "samples for dataset" << datasetId;
    return true;
}

bool DatasetService::extractAndStoreSchema(const QString &datasetId, const QVariantList &samples)
{
    ltTrace(LT_LOG_DATASET()) << "extractAndStoreSchema datasetId=" << datasetId << "sampleCount=" << samples.size();

    QSet<int> allClassIds;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QVariantList classIds = sample["classIds"].toList();
        for (const auto &cid : classIds) {
            allClassIds.insert(cid.toInt());
        }
    }

    if (allClassIds.isEmpty()) {
        ltDebug(LT_LOG_DATASET()) << "No class IDs found in any label file";
    }

    int maxClassId = 0;
    for (int cid : allClassIds) {
        if (cid > maxClassId) maxClassId = cid;
    }

    QStringList classNames;
    for (int i = 0; i <= maxClassId; ++i) {
        classNames.append(QStringLiteral("class_%1").arg(i));
    }

    QVariantList classOrder;
    QList<int> sortedIds = allClassIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classOrder.append(cid);
    }

    QJsonArray classNamesArray;
    for (const auto &name : classNames) classNamesArray.append(name);
    QString classNamesJson = QJsonDocument(classNamesArray).toJson(QJsonDocument::Compact);

    QJsonArray classOrderArray;
    for (const auto &idx : classOrder) classOrderArray.append(idx.toInt());
    QString classOrderJson = QJsonDocument(classOrderArray).toJson(QJsonDocument::Compact);

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
        ltError(LT_LOG_DATASET()) << "Failed to insert label schema:" << query.lastError().text();
        return false;
    }

    ltDebug(LT_LOG_DATASET()) << "Extracted schema with" << classNames.size()
                              << "class names, appearing class IDs:" << sortedIds;
    return true;
}

bool DatasetService::syncClassesToTaxonomy(const QString &datasetId)
{
    ltTrace(LT_LOG_DATASET()) << "syncClassesToTaxonomy datasetId=" << datasetId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    // 1. 从 imported_label_schemas 读取类别名
    QSqlQuery schemaQuery(db);
    schemaQuery.prepare("SELECT raw_class_names_json FROM imported_label_schemas "
                        "WHERE dataset_id = ? ORDER BY created_at DESC LIMIT 1");
    schemaQuery.addBindValue(datasetId);
    if (!schemaQuery.exec() || !schemaQuery.next()) {
        ltDebug(LT_LOG_DATASET()) << "No imported schema found for dataset:" << datasetId;
        return true; // 无 schema 不是错误，可能只是无标签数据集
    }

    QString classNamesJson = schemaQuery.value(0).toString();
    QJsonDocument doc = QJsonDocument::fromJson(classNamesJson.toUtf8());
    if (!doc.isArray()) {
        ltWarning(LT_LOG_DATASET()) << "Invalid class names JSON for dataset:" << datasetId;
        return false;
    }

    QStringList importedClasses;
    QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        importedClasses.append(v.toString());
    }

    if (importedClasses.isEmpty()) {
        ltDebug(LT_LOG_DATASET()) << "Empty class list in schema for dataset:" << datasetId;
        return true;
    }

    // 2. 获取数据集所属项目 ID
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec() || !datasetQuery.next()) {
        ltError(LT_LOG_DATASET()) << "Dataset not found:" << datasetId;
        return false;
    }
    QString projectId = datasetQuery.value(0).toString();

    // 3. 查找项目的 taxonomy（取第一个）
    QSqlQuery taxonomyQuery(db);
    taxonomyQuery.prepare("SELECT id, class_definitions_json FROM taxonomies "
                          "WHERE project_id = ? ORDER BY version DESC LIMIT 1");
    taxonomyQuery.addBindValue(projectId);
    if (!taxonomyQuery.exec() || !taxonomyQuery.next()) {
        ltWarning(LT_LOG_DATASET()) << "No taxonomy found for project:" << projectId
                                    << ", creating one with imported classes";
        // 项目没有 taxonomy，创建一个
        QSqlQuery createQuery(db);
        QString taxonomyId = Id::generate();
        createQuery.prepare("INSERT INTO taxonomies (id, project_id, name, version, class_definitions_json) "
                            "VALUES (?, ?, '默认类别体系', 1, ?)");
        createQuery.addBindValue(taxonomyId);
        createQuery.addBindValue(projectId);
        createQuery.addBindValue(classNamesJson);
        if (!createQuery.exec()) {
            ltError(LT_LOG_DATASET()) << "Failed to create taxonomy:" << createQuery.lastError().text();
            return false;
        }
        ltInfo(LT_LOG_DATASET()) << "Created taxonomy" << taxonomyId << "with" << importedClasses.size() << "classes";
        return true;
    }

    // 4. 合并新类别到已有 taxonomy
    QString taxonomyId = taxonomyQuery.value(0).toString();
    QString existingClassesJson = taxonomyQuery.value(1).toString();

    QStringList existingClasses;
    if (!existingClassesJson.isEmpty()) {
        QJsonDocument existingDoc = QJsonDocument::fromJson(existingClassesJson.toUtf8());
        if (existingDoc.isArray()) {
            for (const auto &v : existingDoc.array()) {
                existingClasses.append(v.toString());
            }
        }
    }

    // 追加不重复的类别
    QSet<QString> existingSet(existingClasses.begin(), existingClasses.end());
    int addedCount = 0;
    for (const QString &cls : importedClasses) {
        if (!existingSet.contains(cls)) {
            existingClasses.append(cls);
            existingSet.insert(cls);
            addedCount++;
        }
    }

    if (addedCount == 0) {
        ltDebug(LT_LOG_DATASET()) << "No new classes to merge into taxonomy" << taxonomyId;
        return true;
    }

    // 5. 更新 taxonomy
    QJsonArray mergedArray;
    for (const auto &name : existingClasses) mergedArray.append(name);
    QString mergedJson = QJsonDocument(mergedArray).toJson(QJsonDocument::Compact);

    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE taxonomies SET class_definitions_json = ?, version = version + 1 WHERE id = ?");
    updateQuery.addBindValue(mergedJson);
    updateQuery.addBindValue(taxonomyId);
    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to update taxonomy:" << updateQuery.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "Synced" << addedCount << "new classes to taxonomy" << taxonomyId
                             << ", total:" << existingClasses.size() << "classes";
    return true;
}

QString DatasetService::appendImport(const QString &datasetId, const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "appendImport datasetId=" << datasetId
                              << "imageDir=" << imageDir << "labelDir=" << labelDir;

    if (datasetId.isEmpty() || imageDir.isEmpty() || labelDir.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "appendImport: missing required parameters";
        return {};
    }

    QVariantMap existing = getDataset(datasetId);
    if (existing.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "appendImport: dataset not found:" << datasetId;
        return {};
    }

    QVariantMap scanResult = m_scanner->scan(imageDir, labelDir);
    if (scanResult.contains("error")) {
        ltError(LT_LOG_DATASET()) << "appendImport scan failed:" << scanResult["error"].toString();
        return {};
    }

    QVariantList samples = scanResult["samples"].toList();
    QVariantList matchedSamples;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString status = sample["status"].toString();
        if (status == QStringLiteral("matched") || status == QStringLiteral("invalid_label")) {
            matchedSamples.append(sample);
        }
    }

    if (!insertSamples(datasetId, matchedSamples)) {
        ltError(LT_LOG_DATASET()) << "appendImport: failed to insert samples";
        return {};
    }

    int newTotal = existing["sampleCount"].toInt() + matchedSamples.size();
    QSqlQuery updateQuery(Database::instance().database());
    updateQuery.prepare("UPDATE datasets SET sample_count = ? WHERE id = ?");
    updateQuery.addBindValue(newTotal);
    updateQuery.addBindValue(datasetId);
    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "appendImport: failed to update sample count:" << updateQuery.lastError().text();
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "appendImport completed: added" << matchedSamples.size()
                             << "samples to dataset" << datasetId;
    return datasetId;
}

bool DatasetService::resplitDataset(const QString &datasetId, double valRatio, int seed)
{
    ltTrace(LT_LOG_DATASET()) << "resplitDataset datasetId=" << datasetId
                              << "valRatio=" << valRatio << "seed=" << seed;

    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "resplitDataset: datasetId is empty";
        return false;
    }

    QSqlDatabase db = Database::instance().database();

    QSqlQuery countQuery(db);
    countQuery.prepare("SELECT COUNT(*) FROM dataset_samples WHERE dataset_id = ?");
    countQuery.addBindValue(datasetId);
    if (!countQuery.exec() || !countQuery.next()) {
        ltError(LT_LOG_DATASET()) << "resplitDataset: count query failed:" << countQuery.lastError().text();
        return false;
    }

    int total = countQuery.value(0).toInt();
    if (total == 0) {
        ltWarning(LT_LOG_DATASET()) << "resplitDataset: no samples in dataset";
        return false;
    }

    QSqlQuery idQuery(db);
    idQuery.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ? ORDER BY id");
    idQuery.addBindValue(datasetId);
    if (!idQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "resplitDataset: id query failed:" << idQuery.lastError().text();
        return false;
    }

    QStringList allIds;
    while (idQuery.next()) {
        allIds.append(idQuery.value(0).toString());
    }

    std::srand(seed);
    QStringList shuffled = allIds;
    for (int i = shuffled.size() - 1; i > 0; --i) {
        int j = std::rand() % (i + 1);
        qSwap(shuffled[i], shuffled[j]);
    }

    int valCount = qMax(1, static_cast<int>(total * valRatio));
    int trainCount = total - valCount;

    QSqlQuery clearQuery(db);
    clearQuery.prepare("UPDATE dataset_samples SET split = NULL WHERE dataset_id = ?");
    clearQuery.addBindValue(datasetId);
    if (!clearQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "resplitDataset: clear split failed:" << clearQuery.lastError().text();
        return false;
    }

    QSqlQuery trainQuery(db);
    trainQuery.prepare("UPDATE dataset_samples SET split = 'train' WHERE id = ? AND dataset_id = ?");
    for (int i = 0; i < trainCount; ++i) {
        trainQuery.addBindValue(shuffled[i]);
        trainQuery.addBindValue(datasetId);
        if (!trainQuery.exec()) {
            ltError(LT_LOG_DATASET()) << "resplitDataset: train update failed:" << trainQuery.lastError().text();
            return false;
        }
        trainQuery.finish();
    }

    QSqlQuery valQuery(db);
    valQuery.prepare("UPDATE dataset_samples SET split = 'val' WHERE id = ? AND dataset_id = ?");
    for (int i = trainCount; i < total; ++i) {
        valQuery.addBindValue(shuffled[i]);
        valQuery.addBindValue(datasetId);
        if (!valQuery.exec()) {
            ltError(LT_LOG_DATASET()) << "resplitDataset: val update failed:" << valQuery.lastError().text();
            return false;
        }
        valQuery.finish();
    }

    ltInfo(LT_LOG_DATASET()) << "resplitDataset completed: train=" << trainCount
                             << "val=" << valCount << "for dataset" << datasetId;
    return true;
}

bool DatasetService::updateClassName(const QString &taxonomyId, int classId, const QString &name)
{
    ltTrace(LT_LOG_DATASET()) << "updateClassName taxonomyId=" << taxonomyId
                              << "classId=" << classId << "name=" << name;

    if (taxonomyId.isEmpty() || name.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "updateClassName: invalid parameters";
        return false;
    }

    QSqlDatabase db = Database::instance().database();
    QSqlQuery query(db);
    query.prepare("UPDATE taxonomy_classes SET name = ? WHERE taxonomy_id = ? AND class_id = ?");
    query.addBindValue(name);
    query.addBindValue(taxonomyId);
    query.addBindValue(classId);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "updateClassName failed:" << query.lastError().text();
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "updateClassName completed for taxonomy" << taxonomyId
                             << "class" << classId << "->" << name;
    return true;
}

QVariantMap DatasetService::scanFolder(const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "scanFolder folderPath=" << folderPath;
    return m_scanner->scanFolder(folderPath);
}

QVariantMap DatasetService::scanSeparate(const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "scanSeparate imageDir=" << imageDir << "labelDir=" << labelDir;

    if (imageDir.isEmpty()) {
        QVariantMap result;
        result["isValid"] = false;
        result["error"] = QStringLiteral("图片目录路径为空");
        return result;
    }

    // 使用 ImportScanner::scan 进行分别路径扫描
    QVariantMap scanResult = m_scanner->scan(imageDir, labelDir);

    // 将 scan 结果转换为 scanFolder 兼容格式
    QVariantMap result;
    bool hasLabelDir = !labelDir.isEmpty() && QDir(labelDir).exists();

    int totalImages = 0;
    int matched = scanResult["matched"].toInt();
    int unmatchedImages = scanResult["unmatchedImages"].toInt();
    int unmatchedLabels = scanResult["unmatchedLabels"].toInt();

    // 统计图片数量
    QFileInfoList imageFiles = m_scanner->collectImageFiles(QDir(imageDir), true);
    totalImages = imageFiles.size();

    // 提取 classIds
    QVariantList samples = scanResult["samples"].toList();
    QSet<int> allClassIds;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QVariantList classIds = sample["classIds"].toList();
        for (const auto &cid : classIds) {
            allClassIds.insert(cid.toInt());
        }
    }

    QVariantList classIdList;
    QList<int> sortedIds = allClassIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classIdList.append(cid);
    }

    // 判断格式
    QString detectedFormat;
    if (hasLabelDir) {
        // 递归检测标签格式
        QDir lblDir(labelDir);
        QFileInfoList labelFiles = m_scanner->collectLabelFiles(lblDir, true);
        bool hasLabelMeJson = false;
        bool hasCocoJson = false;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                if (m_scanner->isLabelMeJsonFile(fi.absoluteFilePath())) {
                    hasLabelMeJson = true;
                } else {
                    hasCocoJson = true;
                }
            }
        }
        if (hasLabelMeJson) {
            detectedFormat = QStringLiteral("labelme_json");
        } else if (hasCocoJson) {
            detectedFormat = QStringLiteral("coco_json");
        } else {
            detectedFormat = QStringLiteral("yolo_txt");
        }
    } else {
        detectedFormat = QStringLiteral("image_only");
    }

    result["isValid"] = totalImages > 0;
    result["detectedFormat"] = detectedFormat;
    result["imageDir"] = imageDir;
    result["labelDirOrPath"] = hasLabelDir ? labelDir : QString();
    result["imageCount"] = totalImages;
    result["labelCount"] = matched;
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = classIdList;
    result["classes"] = QVariantMap();

    if (scanResult.contains("error")) {
        result["error"] = scanResult["error"];
    }

    ltInfo(LT_LOG_DATASET()) << "scanSeparate 完成: imageDir=" << imageDir
                             << "labelDir=" << labelDir
                             << "images=" << totalImages
                             << "matched=" << matched
                             << "format=" << detectedFormat;

    return result;
}

QString DatasetService::importDatasetJson(const QString &projectId, const QString &name,
                                           const QString &imageDir, const QString &jsonLabelPath)
{
    ltTrace(LT_LOG_DATASET()) << "importDatasetJson projectId=" << projectId << "name=" << name
                              << "imageDir=" << imageDir << "jsonLabelPath=" << jsonLabelPath;

    if (projectId.isEmpty() || name.isEmpty() || imageDir.isEmpty() || jsonLabelPath.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "importDatasetJson: missing required parameters";
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "JSON import start: name=" << name
                             << "imageDir=" << imageDir << "jsonLabelPath=" << jsonLabelPath;

    // 步骤1: 创建数据集记录，格式为 coco_json
    QString datasetId = Id::generate();

    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                  "VALUES (?, ?, ?, ?, ?, 'coco_json', 0, 'scanning')");
    query.addBindValue(datasetId);
    query.addBindValue(projectId);
    query.addBindValue(name);
    query.addBindValue(imageDir);
    query.addBindValue(jsonLabelPath);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to create dataset record:" << query.lastError().text();
        return {};
    }

    // 步骤2: 使用 ImportScanner 的 JSON 扫描流程
    QFileInfo jsonFi(jsonLabelPath);
    QString labelDir = jsonFi.absolutePath();

    QVariantMap scanResult = m_scanner->scan(imageDir, labelDir);

    if (scanResult.contains("error")) {
        ltError(LT_LOG_DATASET()) << "JSON scan failed:" << scanResult["error"].toString();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    int matched = scanResult["matched"].toInt();
    QVariantList samples = scanResult["samples"].toList();

    // 过滤出有效样本
    QVariantList matchedSamples;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString status = sample["status"].toString();
        if (status == QStringLiteral("matched") || status == QStringLiteral("invalid_label")) {
            matchedSamples.append(sample);
        }
    }

    // 步骤3: 更新状态为导入中
    if (!updateImportStatus(datasetId, QStringLiteral("importing"))) {
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    // 步骤4: 为 JSON 导入的样本生成 YOLO txt 标签文件
    QSqlQuery projectQuery(Database::instance().database());
    projectQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
    projectQuery.addBindValue(projectId);
    QString projectRoot;
    if (projectQuery.exec() && projectQuery.next()) {
        projectRoot = projectQuery.value(0).toString();
    }

    for (auto &s : matchedSamples) {
        QVariantMap sample = s.toMap();
        QVariantList annotations = sample["annotations"].toList();

        if (annotations.isEmpty()) continue;

        // 为每个样本生成 YOLO txt 标签文件
        QString stem = sample["stem"].toString();
        QString labelOutputDir = projectRoot + "/cache/labels/" + datasetId;
        QDir().mkpath(labelOutputDir);
        QString labelOutputPath = labelOutputDir + "/" + stem + ".txt";

        QFile labelFile(labelOutputPath);
        if (labelFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&labelFile);
            for (const auto &ann : annotations) {
                QVariantMap annMap = ann.toMap();
                int catId = annMap["category_id"].toInt();
                double cx = annMap["cx"].toDouble();
                double cy = annMap["cy"].toDouble();
                double w = annMap["w"].toDouble();
                double h = annMap["h"].toDouble();
                out << catId << " " << cx << " " << cy << " " << w << " " << h << "\n";
            }
            labelFile.close();

            // 更新样本的 labelPath 为生成的 YOLO txt 文件
            QVariantMap updatedSample = sample;
            updatedSample["labelPath"] = labelOutputPath;
            s = updatedSample;
        }
    }

    // 步骤5: 插入样本到数据库
    if (!insertSamples(datasetId, matchedSamples)) {
        ltError(LT_LOG_DATASET()) << "Failed to insert samples from JSON import";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    // 步骤6: 提取并存储类别体系（使用 JSON 中的 categories）
    QVariantMap categories = scanResult["categories"].toMap();
    if (!categories.isEmpty()) {
        if (!extractAndStoreSchemaFromCategories(datasetId, categories)) {
            ltWarning(LT_LOG_DATASET()) << "Failed to extract schema from categories, falling back";
            extractAndStoreSchema(datasetId, matchedSamples);
        }
    } else {
        extractAndStoreSchema(datasetId, matchedSamples);
    }

    // 步骤7: 更新样本数和最终状态
    QSqlQuery updateQuery(Database::instance().database());
    updateQuery.prepare("UPDATE datasets SET sample_count = ?, import_status = 'completed' WHERE id = ?");
    updateQuery.addBindValue(matchedSamples.size());
    updateQuery.addBindValue(datasetId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to finalize dataset:" << updateQuery.lastError().text();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "JSON import completed:" << datasetId
                             << "with" << matchedSamples.size() << "samples";

    syncClassesToTaxonomy(datasetId);
    return datasetId;
}

QString DatasetService::importDatasetSeparate(const QString &projectId,
                                               const QString &datasetName,
                                               const QString &imageDir,
                                               const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "importDatasetSeparate projectId=" << projectId
                              << "datasetName=" << datasetName
                              << "imageDir=" << imageDir << "labelDir=" << labelDir;

    if (projectId.isEmpty() || datasetName.isEmpty() || imageDir.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "importDatasetSeparate: 缺少必要参数";
        return {};
    }

    if (!QDir(imageDir).exists()) {
        ltError(LT_LOG_DATASET()) << "图片目录不存在:" << imageDir;
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "分别路径导入开始: name=" << datasetName
                             << "imageDir=" << imageDir << "labelDir=" << labelDir;

    // 判断标签目录是否存在
    bool hasLabelDir = !labelDir.isEmpty() && QDir(labelDir).exists();

    // 检测标签格式：优先检测 LabelMe JSON，其次 COCO JSON，否则 YOLO TXT
    if (hasLabelDir) {
        QDir lblDir(labelDir);
        QFileInfoList allLabelFiles = m_scanner->collectLabelFiles(lblDir, true);
        bool hasLabelMeJson = false;
        bool hasCocoJson = false;
        for (const auto &fi : allLabelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                if (m_scanner->isLabelMeJsonFile(fi.absoluteFilePath())) {
                    hasLabelMeJson = true;
                } else {
                    hasCocoJson = true;
                }
            }
        }

        if (hasLabelMeJson) {
            ltInfo(LT_LOG_DATASET()) << "检测到 LabelMe JSON 标签，使用 LabelMe JSON 导入流程";
            // 创建数据集记录
            QString datasetId = Id::generate();
            QSqlQuery query(Database::instance().database());
            query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                          "VALUES (?, ?, ?, ?, ?, 'labelme_json', 0, 'scanning')");
            query.addBindValue(datasetId);
            query.addBindValue(projectId);
            query.addBindValue(datasetName);
            query.addBindValue(imageDir);
            query.addBindValue(labelDir);

            if (!query.exec()) {
                ltError(LT_LOG_DATASET()) << "importDatasetSeparate: 创建 LabelMe 数据集记录失败:" << query.lastError().text();
                return {};
            }

            if (!importLabelMeDataset(datasetId, imageDir, labelDir)) {
                ltError(LT_LOG_DATASET()) << "importDatasetSeparate: LabelMe 导入失败";
                updateImportStatus(datasetId, QStringLiteral("failed"));
                return {};
            }

            syncClassesToTaxonomy(datasetId);
            return datasetId;
        }

        if (hasCocoJson) {
            // COCO JSON 格式
            ltInfo(LT_LOG_DATASET()) << "检测到 COCO JSON 标签，使用 COCO JSON 导入流程";
            for (const auto &fi : allLabelFiles) {
                if (fi.suffix().toLower() == QStringLiteral("json")
                    && !m_scanner->isLabelMeJsonFile(fi.absoluteFilePath())) {
                    return importDatasetJson(projectId, datasetName, imageDir,
                                             fi.absoluteFilePath());
                }
            }
        }
    }

    // YOLO TXT 或无标签格式
    return importDataset(projectId, datasetName, imageDir, hasLabelDir ? labelDir : QString());
}

QString DatasetService::importDatasetV2(const QString &projectId,
                                        const QString &datasetName,
                                        const QString &folderPath,
                                        const QString &detectedFormat,
                                        const QString &labelDirOrPath,
                                        bool autoMergeClasses)
{
    ltTrace(LT_LOG_DATASET()) << "importDatasetV2 projectId=" << projectId
                              << "datasetName=" << datasetName
                              << "folderPath=" << folderPath
                              << "detectedFormat=" << detectedFormat
                              << "labelDirOrPath=" << labelDirOrPath
                              << "autoMergeClasses=" << autoMergeClasses;

    if (projectId.isEmpty() || datasetName.isEmpty() || folderPath.isEmpty() || detectedFormat.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "importDatasetV2: 缺少必要参数";
        return {};
    }

    ltInfo(LT_LOG_DATASET()) << "V2 导入开始: name=" << datasetName
                             << "folderPath=" << folderPath
                             << "format=" << detectedFormat
                             << "labelDirOrPath=" << labelDirOrPath;

    // 根据探测到的格式分发到对应的导入流程
    if (detectedFormat == QStringLiteral("yolo_txt")) {
        QString imageDir = folderPath;
        QString labelDir = labelDirOrPath;

        // 检查 images 子目录
        QDir baseDir(folderPath);
        QString imagesSubDir = baseDir.filePath(QStringLiteral("images"));
        if (QDir(imagesSubDir).exists()) imageDir = imagesSubDir;

        // 如果没有提供标签路径，尝试探测 labels 子目录
        if (labelDir.isEmpty()) {
            QString labelsSubDir = baseDir.filePath(QStringLiteral("labels"));
            if (QDir(labelsSubDir).exists()) labelDir = labelsSubDir;
        }

        ltDebug(LT_LOG_DATASET()) << "importDatasetV2: YOLO txt 分发 imageDir=" << imageDir
                                  << "labelDir=" << labelDir;
        return importDataset(projectId, datasetName, imageDir, labelDir);
    }

    if (detectedFormat == QStringLiteral("coco_json")) {
        QString jsonLabelPath = labelDirOrPath;

        // 如果扫描阶段提供了 JSON 路径，直接使用
        if (!jsonLabelPath.isEmpty() && QFile::exists(jsonLabelPath)) {
            ltDebug(LT_LOG_DATASET()) << "importDatasetV2: COCO JSON 使用扫描阶段路径:" << jsonLabelPath;
            QDir baseDir(folderPath);
            QString imageDir = folderPath;
            QString imagesSubDir = baseDir.filePath(QStringLiteral("images"));
            if (QDir(imagesSubDir).exists()) imageDir = imagesSubDir;

            return importDatasetJson(projectId, datasetName, imageDir, jsonLabelPath);
        }

        // 否则搜索 JSON 文件
        QDir baseDir(folderPath);
        QString imageDir = folderPath;

        // 检查 images 子目录
        QString imagesSubDir = baseDir.filePath(QStringLiteral("images"));
        if (QDir(imagesSubDir).exists()) imageDir = imagesSubDir;

        // 搜索常见的 COCO JSON 文件名（在根目录和子目录中递归搜索）
        QStringList jsonCandidates = {
            QStringLiteral("annotations.json"),
            QStringLiteral("instances.json"),
            QStringLiteral("coco.json")
        };

        // 先在根目录搜索
        for (const auto &candidate : jsonCandidates) {
            QString path = baseDir.filePath(candidate);
            if (QFile::exists(path)) {
                jsonLabelPath = path;
                break;
            }
        }

        // 在 labels 子目录搜索
        if (jsonLabelPath.isEmpty()) {
            QString labelsSubDir = baseDir.filePath(QStringLiteral("labels"));
            if (QDir(labelsSubDir).exists()) {
                for (const auto &candidate : jsonCandidates) {
                    QString path = QDir(labelsSubDir).filePath(candidate);
                    if (QFile::exists(path)) {
                        jsonLabelPath = path;
                        break;
                    }
                }
            }
        }

        // 如果没找到，递归搜索目录下所有 JSON 文件
        if (jsonLabelPath.isEmpty()) {
            QFileInfoList allJsonFiles;
            QDir searchDir(folderPath);
            QFileInfoList entries = searchDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
            for (const auto &fi : entries) {
                if (fi.suffix().toLower() == QStringLiteral("json")) {
                    allJsonFiles.append(fi);
                }
            }
            QFileInfoList subDirs = searchDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const auto &subDir : subDirs) {
                QDir sd(subDir.absoluteFilePath());
                QFileInfoList subEntries = sd.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
                for (const auto &fi : subEntries) {
                    if (fi.suffix().toLower() == QStringLiteral("json")) {
                        allJsonFiles.append(fi);
                    }
                }
            }
            if (!allJsonFiles.isEmpty()) {
                jsonLabelPath = allJsonFiles.first().absoluteFilePath();
            }
        }

        if (jsonLabelPath.isEmpty()) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: 在" << folderPath << "中未找到 JSON 标签文件";
            return {};
        }

        ltDebug(LT_LOG_DATASET()) << "importDatasetV2: COCO JSON 分发 jsonLabelPath=" << jsonLabelPath;
        return importDatasetJson(projectId, datasetName, imageDir, jsonLabelPath);
    }

    if (detectedFormat == QStringLiteral("labelme_json")) {
        // LabelMe JSON 格式（每图一个 JSON 文件）
        QString imageDir = folderPath;
        QString labelDir = labelDirOrPath;

        // 检查 images 子目录
        QDir baseDir(folderPath);
        QString imagesSubDir = baseDir.filePath(QStringLiteral("images"));
        if (QDir(imagesSubDir).exists()) imageDir = imagesSubDir;

        // 如果没有提供标签路径，尝试探测 labels 子目录
        if (labelDir.isEmpty()) {
            QString labelsSubDir = baseDir.filePath(QStringLiteral("labels"));
            if (QDir(labelsSubDir).exists()) labelDir = labelsSubDir;
        }

        if (labelDir.isEmpty()) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: LabelMe JSON 格式需要标签目录路径";
            return {};
        }

        ltDebug(LT_LOG_DATASET()) << "importDatasetV2: LabelMe JSON 分发 imageDir=" << imageDir
                                  << "labelDir=" << labelDir;

        // 创建数据集记录
        QString datasetId = Id::generate();
        QSqlQuery query(Database::instance().database());
        query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                      "VALUES (?, ?, ?, ?, ?, 'labelme_json', 0, 'scanning')");
        query.addBindValue(datasetId);
        query.addBindValue(projectId);
        query.addBindValue(datasetName);
        query.addBindValue(imageDir);
        query.addBindValue(labelDir);

        if (!query.exec()) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: 创建 LabelMe 数据集记录失败:" << query.lastError().text();
            return {};
        }

        if (!importLabelMeDataset(datasetId, imageDir, labelDir)) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: LabelMe 导入失败";
            updateImportStatus(datasetId, QStringLiteral("failed"));
            return {};
        }

        syncClassesToTaxonomy(datasetId);
        return datasetId;
    }

    if (detectedFormat == QStringLiteral("anomaly_unsupervised") || detectedFormat == QStringLiteral("image_only")) {
        // 获取项目的 task_type
        QString taskType = QStringLiteral("detect");
        {
            QSqlQuery projectQuery(Database::instance().database());
            projectQuery.prepare("SELECT task_type FROM projects WHERE id = ?");
            projectQuery.addBindValue(projectId);
            if (projectQuery.exec() && projectQuery.next()) {
                taskType = projectQuery.value(0).toString();
            }
        }

        // 检查目录结构是否具有 anomaly detection 的 train/good
        QDir baseDir(folderPath);
        bool hasTrainGood = QDir(baseDir.filePath(QStringLiteral("train/good"))).exists();

        if (taskType != QStringLiteral("anomaly") || !hasTrainGood) {
            ltInfo(LT_LOG_DATASET()) << "importDatasetV2: 项目类型不是异常检测，或者缺少 train/good 目录。路由到纯图片导入。";
            return importDataset(projectId, datasetName, folderPath, QString());
        }

        // 异常检测无监督格式：创建数据集记录后调用专用导入
        QString datasetId = Id::generate();

        QSqlQuery query(Database::instance().database());
        query.prepare("INSERT INTO datasets (id, project_id, name, image_root, label_root, format, sample_count, import_status) "
                      "VALUES (?, ?, ?, ?, ?, 'anomaly_unsupervised', 0, 'scanning')");
        query.addBindValue(datasetId);
        query.addBindValue(projectId);
        query.addBindValue(datasetName);
        query.addBindValue(folderPath);
        query.addBindValue(folderPath);

        if (!query.exec()) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: 创建数据集记录失败:" << query.lastError().text();
            return {};
        }

        ltDebug(LT_LOG_DATASET()) << "异常检测数据集记录已创建:" << datasetId << datasetName;

        if (!importAnomalyDataset(datasetId, folderPath)) {
            ltError(LT_LOG_DATASET()) << "importDatasetV2: 异常检测导入失败";
            updateImportStatus(datasetId, QStringLiteral("failed"));
            return {};
        }

        return datasetId;
    }

    ltError(LT_LOG_DATASET()) << "importDatasetV2: 不支持的格式:" << detectedFormat;
    return {};
}

bool DatasetService::importAnomalyDataset(const QString &datasetId, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "importAnomalyDataset datasetId=" << datasetId
                              << "folderPath=" << folderPath;

    QDir baseDir(folderPath);
    QStringList imageFilters = {
        QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
        QStringLiteral("*.bmp"), QStringLiteral("*.tif"), QStringLiteral("*.tiff"),
        QStringLiteral("*.webp")
    };

    int totalSamples = 0;
    QSqlDatabase db = Database::instance().database();

    // 扫描 train/good 目录
    QDir trainGoodDir(baseDir.filePath(QStringLiteral("train/good")));
    if (trainGoodDir.exists()) {
        QStringList images = trainGoodDir.entryList(imageFilters, QDir::Files);
        for (const auto &imgName : images) {
            QString imagePath = trainGoodDir.absoluteFilePath(imgName);
            QString sampleId = Id::generate();

            QSqlQuery query(db);
            query.prepare("INSERT INTO dataset_samples "
                          "(id, dataset_id, image_path, label_path, validation_status, split) "
                          "VALUES (?, ?, ?, NULL, 'good', 'train')");
            query.addBindValue(sampleId);
            query.addBindValue(datasetId);
            query.addBindValue(imagePath);

            if (!query.exec()) {
                ltError(LT_LOG_DATASET()) << "importAnomalyDataset: 插入 train/good 样本失败:"
                                          << query.lastError().text();
                return false;
            }
            totalSamples++;
        }
        ltDebug(LT_LOG_DATASET()) << "importAnomalyDataset: train/good -" << images.size() << "张图片";
    } else {
        ltWarning(LT_LOG_DATASET()) << "importAnomalyDataset: train/good 目录不存在";
    }

    // 扫描 test 子目录
    QDir testDir(baseDir.filePath(QStringLiteral("test")));
    if (testDir.exists()) {
        QStringList categoryDirs = testDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &category : categoryDirs) {
            QDir catDir(testDir.filePath(category));
            QStringList images = catDir.entryList(imageFilters, QDir::Files);

            // good 目录下的样本标记为 good，其他目录标记为 defective
            QString validationStatus = (category == QStringLiteral("good"))
                                           ? QStringLiteral("good")
                                           : QStringLiteral("defective");

            for (const auto &imgName : images) {
                QString imagePath = catDir.absoluteFilePath(imgName);
                QString sampleId = Id::generate();

                QSqlQuery query(db);
                query.prepare("INSERT INTO dataset_samples "
                              "(id, dataset_id, image_path, label_path, validation_status, split) "
                              "VALUES (?, ?, ?, NULL, ?, 'test')");
                query.addBindValue(sampleId);
                query.addBindValue(datasetId);
                query.addBindValue(imagePath);
                query.addBindValue(validationStatus);

                if (!query.exec()) {
                    ltError(LT_LOG_DATASET()) << "importAnomalyDataset: 插入 test/" << category
                                              << "样本失败:" << query.lastError().text();
                    return false;
                }
                totalSamples++;
            }
            ltDebug(LT_LOG_DATASET()) << "importAnomalyDataset: test/" << category
                                      << "-" << images.size() << "张图片"
                                      << "status=" << validationStatus;
        }
    } else {
        ltWarning(LT_LOG_DATASET()) << "importAnomalyDataset: test 目录不存在";
    }

    if (totalSamples == 0) {
        ltError(LT_LOG_DATASET()) << "importAnomalyDataset: 在" << folderPath << "中未找到任何样本";
        return false;
    }

    // 更新状态为导入中
    if (!updateImportStatus(datasetId, QStringLiteral("importing"))) {
        ltError(LT_LOG_DATASET()) << "importAnomalyDataset: 更新状态为 importing 失败";
        return false;
    }

    // 更新样本数和最终状态
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE datasets SET sample_count = ?, import_status = 'completed' WHERE id = ?");
    updateQuery.addBindValue(totalSamples);
    updateQuery.addBindValue(datasetId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "importAnomalyDataset: 完成数据集更新失败:"
                                  << updateQuery.lastError().text();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "异常检测导入完成:" << datasetId
                             << "共" << totalSamples << "个样本";
    return true;
}

bool DatasetService::extractAndStoreSchemaFromCategories(const QString &datasetId, const QVariantMap &categories)
{
    ltTrace(LT_LOG_DATASET()) << "extractAndStoreSchemaFromCategories datasetId=" << datasetId
                              << "categories count=" << categories.size();

    if (categories.isEmpty()) return false;

    // 从 categories 映射中提取类别信息
    QMap<int, QString> categoryMap;
    QList<int> sortedIds;

    for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
        bool ok = false;
        int catId = it.key().toInt(&ok);
        if (ok && catId >= 0) {
            categoryMap[catId] = it.value().toString();
            sortedIds.append(catId);
        }
    }

    if (sortedIds.isEmpty()) return false;

    std::sort(sortedIds.begin(), sortedIds.end());

    // 构建类别名称数组（按 ID 顺序填充，空缺位置用 class_N 填充）
    int maxId = sortedIds.last();
    QStringList classNames;
    for (int i = 0; i <= maxId; ++i) {
        if (categoryMap.contains(i)) {
            classNames.append(categoryMap[i]);
        } else {
            classNames.append(QStringLiteral("class_%1").arg(i));
        }
    }

    // 构建类别顺序数组
    QVariantList classOrder;
    for (int cid : sortedIds) {
        classOrder.append(cid);
    }

    QJsonArray classNamesArray;
    for (const auto &name : classNames) classNamesArray.append(name);
    QString classNamesJson = QJsonDocument(classNamesArray).toJson(QJsonDocument::Compact);

    QJsonArray classOrderArray;
    for (const auto &idx : classOrder) classOrderArray.append(idx.toInt());
    QString classOrderJson = QJsonDocument(classOrderArray).toJson(QJsonDocument::Compact);

    QString schemaId = Id::generate();
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO imported_label_schemas "
                  "(id, dataset_id, raw_class_names_json, raw_class_order_json, source_format) "
                  "VALUES (?, ?, ?, ?, 'coco_json')");
    query.addBindValue(schemaId);
    query.addBindValue(datasetId);
    query.addBindValue(classNamesJson);
    query.addBindValue(classOrderJson);

    if (!query.exec()) {
        ltError(LT_LOG_DATASET()) << "Failed to insert label schema from categories:" << query.lastError().text();
        return false;
    }

    ltDebug(LT_LOG_DATASET()) << "Extracted schema from categories with" << classNames.size()
                              << "class names, category IDs:" << sortedIds;
    return true;
}

bool DatasetService::importLabelMeDataset(const QString &datasetId, const QString &imageDir, const QString &labelDir)
{
    ltInfo(LT_LOG_DATASET()) << "importLabelMeDataset datasetId=" << datasetId
                             << "imageDir=" << imageDir << "labelDir=" << labelDir;

    // 使用 ImportScanner 的 LabelMe JSON 扫描流程
    QVariantMap scanResult = m_scanner->scanWithLabelMeJsonLabels(imageDir, labelDir);

    if (scanResult.contains("error")) {
        ltError(LT_LOG_DATASET()) << "importLabelMeDataset: 扫描失败:" << scanResult["error"].toString();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    QVariantList samples = scanResult["samples"].toList();

    // 过滤出有效样本
    QVariantList matchedSamples;
    for (const auto &s : samples) {
        QVariantMap sample = s.toMap();
        QString status = sample["status"].toString();
        if (status == QStringLiteral("matched") || status == QStringLiteral("invalid_label")) {
            matchedSamples.append(sample);
        }
    }

    // 更新状态为导入中
    if (!updateImportStatus(datasetId, QStringLiteral("importing"))) {
        ltError(LT_LOG_DATASET()) << "importLabelMeDataset: 更新状态为导入中失败";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    // 通过 dataset_id 反查 project_id，再查 project_root 获取项目根目录
    QString projectRoot;
    {
        QSqlQuery dsQuery(Database::instance().database());
        dsQuery.prepare("SELECT root_path FROM projects WHERE id = "
                        "(SELECT project_id FROM datasets WHERE id = ?)");
        dsQuery.addBindValue(datasetId);
        if (dsQuery.exec() && dsQuery.next()) {
            projectRoot = dsQuery.value(0).toString();
        }
    }
    if (projectRoot.isEmpty()) {
        ltError(LT_LOG_DATASET()) << "importLabelMeDataset: 无法获取项目根目录";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    for (auto &s : matchedSamples) {
        QVariantMap sample = s.toMap();
        QVariantList annotations = sample["annotations"].toList();

        if (annotations.isEmpty()) continue;

        // 为每个样本生成 YOLO txt 标签文件
        QString stem = sample["stem"].toString();
        QString labelOutputDir = projectRoot + "/cache/labels/" + datasetId;
        QDir().mkpath(labelOutputDir);
        QString labelOutputPath = labelOutputDir + "/" + stem + ".txt";

        QFile labelFile(labelOutputPath);
        if (labelFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&labelFile);
            for (const auto &ann : annotations) {
                QVariantMap annMap = ann.toMap();
                int classId = annMap["classId"].toInt();
                QString shapeType = annMap["shapeType"].toString();

                if (shapeType == QStringLiteral("rectangle") || shapeType == QStringLiteral("polygon")) {
                    // 矩形/多边形标注：输出 YOLO HBB 格式
                    if (annMap.contains("cx") && annMap.contains("cy")
                        && annMap.contains("w") && annMap.contains("h")) {
                        double cx = annMap["cx"].toDouble();
                        double cy = annMap["cy"].toDouble();
                        double w = annMap["w"].toDouble();
                        double h = annMap["h"].toDouble();
                        out << classId << " " << cx << " " << cy << " " << w << " " << h << "\n";
                    }
                }
                // 其他 shape_type（polygon/circle 等）可后续扩展
            }
            labelFile.close();

            // 更新样本的 labelPath 为生成的 YOLO txt 文件
            QVariantMap updatedSample = sample;
            updatedSample["labelPath"] = labelOutputPath;
            s = updatedSample;
        }
    }

    // 插入样本到数据库
    if (!insertSamples(datasetId, matchedSamples)) {
        ltError(LT_LOG_DATASET()) << "importLabelMeDataset: 插入样本失败";
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    // 提取并存储类别体系（使用 LabelMe JSON 中的 categories）
    QVariantMap categories = scanResult["categories"].toMap();
    if (!categories.isEmpty()) {
        // LabelMe 使用 label -> classId 映射，需要用 labelme_json 作为 source_format
        if (!extractAndStoreSchemaFromCategories(datasetId, categories)) {
            ltWarning(LT_LOG_DATASET()) << "importLabelMeDataset: 从 categories 提取类别体系失败，回退到样本提取";
            extractAndStoreSchema(datasetId, matchedSamples);
        } else {
            // 更新 source_format 为 labelme_json
            QSqlQuery fmtQuery(Database::instance().database());
            fmtQuery.prepare("UPDATE imported_label_schemas SET source_format = 'labelme_json' WHERE dataset_id = ?");
            fmtQuery.addBindValue(datasetId);
            fmtQuery.exec();
        }
    } else {
        extractAndStoreSchema(datasetId, matchedSamples);
    }

    // 更新样本数和最终状态
    QSqlQuery updateQuery(Database::instance().database());
    updateQuery.prepare("UPDATE datasets SET sample_count = ?, import_status = 'completed' WHERE id = ?");
    updateQuery.addBindValue(matchedSamples.size());
    updateQuery.addBindValue(datasetId);

    if (!updateQuery.exec()) {
        ltError(LT_LOG_DATASET()) << "importLabelMeDataset: 完成数据集更新失败:" << updateQuery.lastError().text();
        updateImportStatus(datasetId, QStringLiteral("failed"));
        return false;
    }

    ltInfo(LT_LOG_DATASET()) << "LabelMe 导入完成:" << datasetId
                             << "共" << matchedSamples.size() << "个样本";
    return true;
}
