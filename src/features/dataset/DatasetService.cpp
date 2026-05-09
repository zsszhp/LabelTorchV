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
#include <algorithm>

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
