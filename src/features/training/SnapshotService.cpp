#include "SnapshotService.h"
#include "Database.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QRandomGenerator>
#include <algorithm>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

SnapshotService::SnapshotService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_TRAINING()) << "parent=" << parent;
}

QString SnapshotService::createSnapshot(const QString &datasetId,
                                         double trainRatio,
                                         const QString &splitStrategy)
{
    ltTrace(LT_LOG_TRAINING()) << "datasetId=" << datasetId << "trainRatio=" << trainRatio << "splitStrategy=" << splitStrategy;

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    // 1. Collect all valid sample IDs from the dataset
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ? AND validation_status = 'valid' ORDER BY id");
    sampleQuery.addBindValue(datasetId);
    if (!sampleQuery.exec()) return {};

    QStringList allSampleIds;
    while (sampleQuery.next()) {
        allSampleIds.append(sampleQuery.value(0).toString());
    }

    if (allSampleIds.isEmpty()) return {};

    // 2. Get dataset's project_id for taxonomy lookup
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec() || !datasetQuery.next()) return {};
    QString projectId = datasetQuery.value(0).toString();

    // 3. Get current taxonomy version
    QString taxonomyVersion;
    QSqlQuery taxQuery(db);
    taxQuery.prepare("SELECT id, version FROM taxonomies WHERE project_id = ? ORDER BY created_at DESC LIMIT 1");
    taxQuery.addBindValue(projectId);
    if (taxQuery.exec() && taxQuery.next()) {
        taxonomyVersion = taxQuery.value(0).toString() + ":v" + taxQuery.value(1).toString();
    }

    // 4. Get current annotation revision boundary (max revision id)
    QString revisionBoundary;
    QSqlQuery revQuery(db);
    revQuery.prepare("SELECT MAX(id) FROM annotation_revisions WHERE dataset_id = ?");
    revQuery.addBindValue(datasetId);
    if (revQuery.exec() && revQuery.next()) {
        revisionBoundary = revQuery.value(0).toString();
    }
    if (revisionBoundary.isEmpty()) {
        revisionBoundary = "none";
    }

    // 5. Build sample manifest JSON
    QJsonArray manifestArray;
    for (const auto &id : allSampleIds) {
        manifestArray.append(id);
    }
    QJsonDocument manifestDoc(manifestArray);
    QString manifestJson = QString::fromUtf8(manifestDoc.toJson(QJsonDocument::Compact));

    // 6. Build train/val split
    QStringList trainIds;
    QStringList valIds;

    QStringList shuffledIds = allSampleIds;
    if (splitStrategy == "random") {
        // Fisher-Yates shuffle
        for (int i = shuffledIds.size() - 1; i > 0; --i) {
            int j = QRandomGenerator::global()->bounded(i + 1);
            std::swap(shuffledIds[i], shuffledIds[j]);
        }
    }

    int splitPoint = qMax(1, static_cast<int>(shuffledIds.size() * trainRatio));
    for (int i = 0; i < shuffledIds.size(); ++i) {
        if (i < splitPoint) {
            trainIds.append(shuffledIds[i]);
        } else {
            valIds.append(shuffledIds[i]);
        }
    }

    QJsonObject splitObj;
    QJsonArray trainArray, valArray;
    for (const auto &id : trainIds) trainArray.append(id);
    for (const auto &id : valIds) valArray.append(id);
    splitObj["train"] = trainArray;
    splitObj["val"] = valArray;
    QJsonDocument splitDoc(splitObj);
    QString splitJson = QString::fromUtf8(splitDoc.toJson(QJsonDocument::Compact));

    // 7. Insert snapshot record
    QString snapshotId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery insertQuery(db);
    insertQuery.prepare(
        "INSERT INTO dataset_snapshots (id, dataset_id, sample_manifest_json, split_manifest_json, "
        "taxonomy_version, annotation_revision_boundary, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    insertQuery.addBindValue(snapshotId);
    insertQuery.addBindValue(datasetId);
    insertQuery.addBindValue(manifestJson);
    insertQuery.addBindValue(splitJson);
    insertQuery.addBindValue(taxonomyVersion);
    insertQuery.addBindValue(revisionBoundary);
    insertQuery.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));

    if (!insertQuery.exec()) return {};

    ltInfo(LT_LOG_TRAINING()) << "Created snapshot:" << snapshotId
                              << "datasetId=" << datasetId
                              << "samples=" << allSampleIds.size()
                              << "train=" << trainIds.size()
                              << "val=" << valIds.size();
    return snapshotId;
}

QVariantList SnapshotService::listSnapshots(const QString &datasetId)
{
    ltTrace(LT_LOG_TRAINING()) << "datasetId=" << datasetId;

    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare("SELECT id, dataset_id, taxonomy_version, annotation_revision_boundary, created_at "
                  "FROM dataset_snapshots WHERE dataset_id = ? ORDER BY created_at DESC");
    query.addBindValue(datasetId);

    if (!query.exec()) return result;

    while (query.next()) {
        QVariantMap snapshot;
        snapshot["id"] = query.value(0).toString();
        snapshot["datasetId"] = query.value(1).toString();
        snapshot["taxonomyVersion"] = query.value(2).toString();
        snapshot["revisionBoundary"] = query.value(3).toString();
        snapshot["createdAt"] = query.value(4).toString();

        // Parse sample count from manifest
        QSqlQuery manifestQuery(db);
        manifestQuery.prepare("SELECT sample_manifest_json FROM dataset_snapshots WHERE id = ?");
        manifestQuery.addBindValue(query.value(0).toString());
        if (manifestQuery.exec() && manifestQuery.next()) {
            QJsonDocument doc = QJsonDocument::fromJson(manifestQuery.value(0).toString().toUtf8());
            snapshot["sampleCount"] = doc.array().size();
        } else {
            snapshot["sampleCount"] = 0;
        }

        // Parse split counts
        QSqlQuery splitQuery(db);
        splitQuery.prepare("SELECT split_manifest_json FROM dataset_snapshots WHERE id = ?");
        splitQuery.addBindValue(query.value(0).toString());
        if (splitQuery.exec() && splitQuery.next()) {
            QJsonDocument splitDoc = QJsonDocument::fromJson(splitQuery.value(0).toString().toUtf8());
            QJsonObject splitObj = splitDoc.object();
            snapshot["trainCount"] = splitObj["train"].toArray().size();
            snapshot["valCount"] = splitObj["val"].toArray().size();
        } else {
            snapshot["trainCount"] = 0;
            snapshot["valCount"] = 0;
        }

        result.append(snapshot);
    }

    ltDebug(LT_LOG_TRAINING()) << "Listed" << result.size() << "snapshots for dataset:" << datasetId;
    return result;
}

QVariantMap SnapshotService::getSnapshot(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare("SELECT id, dataset_id, sample_manifest_json, split_manifest_json, "
                  "taxonomy_version, annotation_revision_boundary, created_at "
                  "FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapshotId);

    if (!query.exec() || !query.next()) return result;

    result["id"] = query.value(0).toString();
    result["datasetId"] = query.value(1).toString();
    result["taxonomyVersion"] = query.value(4).toString();
    result["revisionBoundary"] = query.value(5).toString();
    result["createdAt"] = query.value(6).toString();

    // Parse counts from JSON manifests
    QJsonDocument manifestDoc = QJsonDocument::fromJson(query.value(2).toString().toUtf8());
    result["sampleCount"] = manifestDoc.array().size();

    QJsonDocument splitDoc = QJsonDocument::fromJson(query.value(3).toString().toUtf8());
    QJsonObject splitObj = splitDoc.object();
    result["trainCount"] = splitObj["train"].toArray().size();
    result["valCount"] = splitObj["val"].toArray().size();

    return result;
}

bool SnapshotService::deleteSnapshot(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();

    // Check if any training runs reference this snapshot
    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM training_runs WHERE snapshot_id = ?");
    checkQuery.addBindValue(snapshotId);
    if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
        ltWarning(LT_LOG_TRAINING()) << "Cannot delete snapshot, referenced by training runs:" << snapshotId;
        return false; // Cannot delete: in use by training runs
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare("DELETE FROM dataset_snapshots WHERE id = ?");
    deleteQuery.addBindValue(snapshotId);

    if (deleteQuery.exec()) {
        ltInfo(LT_LOG_TRAINING()) << "Deleted snapshot:" << snapshotId;
        return true;
    }
    return false;
}

QVariantList SnapshotService::getSampleManifest(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();
    QVariantList result;

    QSqlQuery query(db);
    query.prepare("SELECT sample_manifest_json FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapshotId);

    if (!query.exec() || !query.next()) return result;

    QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    for (const auto &item : doc.array()) {
        result.append(item.toString());
    }

    return result;
}

QVariantMap SnapshotService::getSplitManifest(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();
    QVariantMap result;

    QSqlQuery query(db);
    query.prepare("SELECT split_manifest_json FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapshotId);

    if (!query.exec() || !query.next()) return result;

    QJsonDocument doc = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    QJsonObject obj = doc.object();

    QVariantList trainList, valList;
    for (const auto &item : obj["train"].toArray()) {
        trainList.append(item.toString());
    }
    for (const auto &item : obj["val"].toArray()) {
        valList.append(item.toString());
    }

    result["train"] = trainList;
    result["val"] = valList;

    return result;
}

bool SnapshotService::isImmutable(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    auto db = Database::instance().database();

    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapshotId);

    return query.exec() && query.next() && query.value(0).toInt() > 0;
}

bool SnapshotService::isOBBDataset(const QString &datasetId)
{
    ltTrace(LT_LOG_TRAINING()) << "datasetId=" << datasetId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    if (datasetId.isEmpty()) {
        ltWarning(LT_LOG_TRAINING()) << "datasetId is empty";
        return false;
    }

    // Query label paths for this dataset
    QSqlQuery query(db);
    query.prepare("SELECT label_path FROM dataset_samples WHERE dataset_id = ? "
                  "AND label_path IS NOT NULL AND validation_status = 'valid' LIMIT 5");
    query.addBindValue(datasetId);

    if (!query.exec()) {
        ltError(LT_LOG_TRAINING()) << "query failed:" << query.lastError().text();
        return false;
    }

    int obbCount = 0;
    int hbbCount = 0;
    int checkedFiles = 0;

    while (query.next()) {
        QString labelPath = query.value(0).toString();
        if (labelPath.isEmpty()) continue;

        QFile file(labelPath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        // Check the first non-empty line
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() == 9) {
                obbCount++;
            } else if (parts.size() == 5) {
                hbbCount++;
            }
            break;
        }

        file.close();
        checkedFiles++;
    }

    if (checkedFiles == 0) {
        ltDebug(LT_LOG_TRAINING()) << "No valid label files found for dataset:" << datasetId;
        return false;
    }

    // If any files have OBB format lines, consider it an OBB dataset
    bool isOBB = obbCount > 0 && obbCount >= hbbCount;

    ltInfo(LT_LOG_TRAINING()) << "Dataset" << datasetId
                              << "obbCount:" << obbCount << "hbbCount:" << hbbCount
                              << "result:" << isOBB;

    return isOBB;
}
