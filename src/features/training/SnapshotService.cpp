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
#include <QDir>
#include <QFileInfo>
#include <random>

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
    sampleQuery.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ? AND validation_status IN ('valid', 'good', 'defective') ORDER BY id");
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
                  "AND label_path IS NOT NULL AND validation_status IN ('valid', 'good', 'defective') LIMIT 5");
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

QString SnapshotService::prepareSnapshotPhysicalDir(const QString &snapshotId)
{
    ltInfo(LT_LOG_TRAINING()) << "Preparing snapshot physical directory for" << snapshotId;
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_TRAINING()) << "Database not open";
        return {};
    }

    // 1. Get snapshot details
    QSqlQuery snapQuery(db);
    snapQuery.prepare("SELECT dataset_id, split_manifest_json, taxonomy_version FROM dataset_snapshots WHERE id = ?");
    snapQuery.addBindValue(snapshotId);
    if (!snapQuery.exec() || !snapQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Snapshot not found in database:" << snapshotId;
        return {};
    }

    QString datasetId = snapQuery.value(0).toString();
    QString splitManifestJson = snapQuery.value(1).toString();
    QString taxonomyVersion = snapQuery.value(2).toString();

    // 2. Get project root path
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec() || !datasetQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Dataset not found for snapshot:" << datasetId;
        return {};
    }
    QString projectId = datasetQuery.value(0).toString();

    QSqlQuery projectQuery(db);
    projectQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
    projectQuery.addBindValue(projectId);
    if (!projectQuery.exec() || !projectQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Project not found for dataset:" << projectId;
        return {};
    }
    QString projectRoot = projectQuery.value(0).toString();

    // 3. Define target snapshot directory
    QString cacheDir = projectRoot + QStringLiteral("/cache/snapshots");
    QString snapshotDir = cacheDir + QStringLiteral("/") + snapshotId;

    QDir dir;
    if (!dir.mkpath(snapshotDir + QStringLiteral("/images/train")) ||
        !dir.mkpath(snapshotDir + QStringLiteral("/images/val")) ||
        !dir.mkpath(snapshotDir + QStringLiteral("/labels/train")) ||
        !dir.mkpath(snapshotDir + QStringLiteral("/labels/val"))) {
        ltError(LT_LOG_TRAINING()) << "Failed to create physical folders for snapshot:" << snapshotDir;
        return {};
    }

    // 4. Parse split manifest
    QJsonParseError parseError;
    QJsonDocument splitDoc = QJsonDocument::fromJson(splitManifestJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        ltError(LT_LOG_TRAINING()) << "Failed to parse split manifest JSON:" << parseError.errorString();
        return {};
    }

    QJsonObject splitObj = splitDoc.object();
    QJsonArray trainSamples = splitObj[QStringLiteral("train")].toArray();
    QJsonArray valSamples = splitObj[QStringLiteral("val")].toArray();

    auto copySamples = [&](const QJsonArray &samples, const QString &splitName) -> bool {
        for (const auto &val : samples) {
            QString sampleId = val.toString();
            QSqlQuery sampleQuery(db);
            sampleQuery.prepare("SELECT image_path, label_path FROM dataset_samples WHERE id = ?");
            sampleQuery.addBindValue(sampleId);
            if (!sampleQuery.exec() || !sampleQuery.next()) {
                ltWarning(LT_LOG_TRAINING()) << "Sample not found during snapshot preparation:" << sampleId;
                continue;
            }

            QString srcImg = sampleQuery.value(0).toString();
            QString srcLbl = sampleQuery.value(1).toString();

            QFileInfo imgInfo(srcImg);
            QString dstImg = snapshotDir + QStringLiteral("/images/") + splitName + QStringLiteral("/") + imgInfo.fileName();
            
            if (QFile::exists(dstImg)) {
                QFile::remove(dstImg);
            }
            if (!QFile::copy(srcImg, dstImg)) {
                ltError(LT_LOG_TRAINING()) << "Failed to copy image from" << srcImg << "to" << dstImg;
                return false;
            }

            if (!srcLbl.isEmpty()) {
                QFileInfo lblInfo(srcLbl);
                QString dstLbl = snapshotDir + QStringLiteral("/labels/") + splitName + QStringLiteral("/") + lblInfo.fileName();
                if (QFile::exists(dstLbl)) {
                    QFile::remove(dstLbl);
                }
                if (QFile::exists(srcLbl)) {
                    QFile::copy(srcLbl, dstLbl);
                } else {
                    QFile file(dstLbl);
                    file.open(QIODevice::WriteOnly);
                    file.close();
                }
            }
        }
        return true;
    };

    if (!copySamples(trainSamples, QStringLiteral("train")) ||
        !copySamples(valSamples, QStringLiteral("val"))) {
        return {};
    }

    // 5. Query taxonomy classes
    QStringList classes;
    QStringList parts = taxonomyVersion.split(QChar(':'));
    if (parts.size() >= 2) {
        QString taxonomyId = parts[0];
        QSqlQuery taxQuery(db);
        taxQuery.prepare("SELECT class_definitions_json FROM taxonomies WHERE id = ?");
        taxQuery.addBindValue(taxonomyId);
        if (taxQuery.exec() && taxQuery.next()) {
            QJsonDocument classDoc = QJsonDocument::fromJson(taxQuery.value(0).toString().toUtf8());
            for (const auto &val : classDoc.array()) {
                classes.append(val.toString());
            }
        }
    }

    if (classes.isEmpty()) {
        classes.append(QStringLiteral("defect"));
    }

    // 6. Write data.yaml
    QString yamlPath = snapshotDir + QStringLiteral("/data.yaml");
    QFile yamlFile(yamlPath);
    if (!yamlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ltError(LT_LOG_TRAINING()) << "Failed to create data.yaml file at" << yamlPath;
        return {};
    }

    QTextStream out(&yamlFile);
    out << "path: " << QDir(snapshotDir).absolutePath() << "\n";
    out << "train: images/train\n";
    out << "val: images/val\n";
    out << "nc: " << classes.size() << "\n";
    out << "names:\n";
    for (int i = 0; i < classes.size(); ++i) {
        out << "  " << i << ": " << classes[i] << "\n";
    }
    yamlFile.close();

    ltInfo(LT_LOG_TRAINING()) << "Snapshot prepared physically at:" << snapshotDir;
    return yamlPath;
}

QString SnapshotService::prepareAnomalySnapshotDir(const QString &snapshotId)
{
    ltInfo(LT_LOG_TRAINING()) << "Preparing anomaly detection snapshot directory for" << snapshotId;
    auto db = Database::instance().database();
    if (!db.isOpen()) {
        ltError(LT_LOG_TRAINING()) << "Database not open";
        return {};
    }

    // 1. 获取快照信息
    QSqlQuery snapQuery(db);
    snapQuery.prepare("SELECT dataset_id, sample_manifest_json FROM dataset_snapshots WHERE id = ?");
    snapQuery.addBindValue(snapshotId);
    if (!snapQuery.exec() || !snapQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Snapshot not found:" << snapshotId;
        return {};
    }

    QString datasetId = snapQuery.value(0).toString();
    QString manifestJson = snapQuery.value(1).toString();

    // 2. 获取项目根路径
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec() || !datasetQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Dataset not found:" << datasetId;
        return {};
    }
    QString projectId = datasetQuery.value(0).toString();

    QSqlQuery projectQuery(db);
    projectQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
    projectQuery.addBindValue(projectId);
    if (!projectQuery.exec() || !projectQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Project not found:" << projectId;
        return {};
    }
    QString projectRoot = projectQuery.value(0).toString();

    // 3. 创建快照目录结构（Anomalib 规范）
    QString cacheDir = projectRoot + QStringLiteral("/cache/snapshots");
    QString snapshotDir = cacheDir + QStringLiteral("/") + snapshotId;

    QDir dir;
    if (!dir.mkpath(snapshotDir + QStringLiteral("/train/good")) ||
        !dir.mkpath(snapshotDir + QStringLiteral("/test/good")) ||
        !dir.mkpath(snapshotDir + QStringLiteral("/test/defective"))) {
        ltError(LT_LOG_TRAINING()) << "Failed to create anomaly snapshot directories:" << snapshotDir;
        return {};
    }

    // 4. 查询样本并按 validation_status 和 split 分类复制
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT image_path, validation_status, split FROM dataset_samples "
                        "WHERE dataset_id = ? AND validation_status IN ('good', 'defective')");
    sampleQuery.addBindValue(datasetId);

    if (!sampleQuery.exec()) {
        ltError(LT_LOG_TRAINING()) << "Failed to query samples for anomaly snapshot:" << sampleQuery.lastError().text();
        return {};
    }

    int copiedCount = 0;
    while (sampleQuery.next()) {
        QString srcImg = sampleQuery.value(0).toString();
        QString validationStatus = sampleQuery.value(1).toString();
        QString split = sampleQuery.value(2).toString();

        if (srcImg.isEmpty()) continue;

        QFileInfo imgInfo(srcImg);
        QString destSubDir;

        if (split == QStringLiteral("train") && validationStatus == QStringLiteral("good")) {
            destSubDir = QStringLiteral("/train/good/");
        } else if (split == QStringLiteral("test") && validationStatus == QStringLiteral("good")) {
            destSubDir = QStringLiteral("/test/good/");
        } else if (split == QStringLiteral("test") && validationStatus == QStringLiteral("defective")) {
            destSubDir = QStringLiteral("/test/defective/");
        } else {
            // 默认：正常样本放 train/good
            destSubDir = QStringLiteral("/train/good/");
        }

        QString dstImg = snapshotDir + destSubDir + imgInfo.fileName();
        if (QFile::exists(dstImg)) {
            QFile::remove(dstImg);
        }
        if (QFile::copy(srcImg, dstImg)) {
            copiedCount++;
        } else {
            ltWarning(LT_LOG_TRAINING()) << "Failed to copy image:" << srcImg << "to" << dstImg;
        }
    }

    ltInfo(LT_LOG_TRAINING()) << "Anomaly snapshot prepared at:" << snapshotDir
                              << "copied" << copiedCount << "images";
    return snapshotDir;
}
