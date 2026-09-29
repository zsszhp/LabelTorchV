#include "SnapshotService.h"
#include "Database.h"
#include "ipc/IpcClient.h"
#include "ipc/IpcProtocol.h"
#include "utils/Log.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <algorithm>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <random>

struct ThreadDbGuard {
    QString connectionName;
    ~ThreadDbGuard() {
        QSqlDatabase::removeDatabase(connectionName);
    }
};

/// 原子性写入文件（先写 .tmp 再 rename，防止崩溃半写损坏）
static bool writeFileAtomically(const QString &targetPath, const QString &content)
{
    QString tmpPath = targetPath + QStringLiteral(".tmp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ltError(LT_LOG_TRAINING()) << "Failed to create temp file:" << tmpPath;
        return false;
    }
    QTextStream out(&tmpFile);
    out << content;
    out.flush();
    tmpFile.close();

    // 移除已存在的目标文件（rename 不会覆盖已存在文件）
    QFile::remove(targetPath);
    if (!tmpFile.rename(targetPath)) {
        ltError(LT_LOG_TRAINING()) << "Failed to rename temp file to:" << targetPath;
        QFile::remove(tmpPath);
        return false;
    }
    return true;
}

// ============================================================================
// P0-1: 快照哈希冻结 —— 内部工具
// ============================================================================

QString SnapshotService::computeFileSha256(const QString &filePath)
{
    // 文件不存在时返回空串，由调用方决定降级策略（分类数据集 label_path 存的是类别名而非路径）
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ltWarning(LT_LOG_TRAINING()) << "Cannot open file for hashing:" << filePath;
        return {};
    }

    // 分块哈希（1MB），避免大图片一次性读入内存
    QCryptographicHash hash(QCryptographicHash::Sha256);
    constexpr int kChunkSize = 1024 * 1024;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty() && !file.atEnd()) {
            ltWarning(LT_LOG_TRAINING()) << "Read error while hashing:" << filePath;
            return {};
        }
        hash.addData(chunk);
    }
    return QString::fromLatin1(hash.result().toHex());
}

void SnapshotService::setLastError(const QString &code, const QString &message)
{
    m_lastErrorCode = code;
    m_lastError = message;
    ltError(LT_LOG_TRAINING()) << "Snapshot error [" << code << "]:" << message;
}

void SnapshotService::clearLastError()
{
    m_lastErrorCode = QStringLiteral("OK");
    m_lastError.clear();
}

QString SnapshotService::lastError() const
{
    return m_lastError;
}

QString SnapshotService::lastErrorCode() const
{
    return m_lastErrorCode;
}

QMap<QString, QJsonObject> SnapshotService::parseManifestHashes(const QString &manifestJson)
{
    QMap<QString, QJsonObject> result;
    QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8());
    if (!doc.isArray()) return result;

    for (const auto &val : doc.array()) {
        if (val.isObject()) {
            // 新格式：对象含 id / imageHash / labelHash
            QJsonObject obj = val.toObject();
            QString id = obj.value(QStringLiteral("id")).toString();
            if (!id.isEmpty()) {
                result.insert(id, obj);
            }
        }
        // 旧格式为纯字符串 ID，无哈希信息——不入 map，调用方按无哈希降级
    }
    return result;
}

bool SnapshotService::writeFreezeMarker(const QString &snapshotDir, const QString &manifestJson)
{
    // 冻结标记记录创建时间与 manifest 原文摘要，供离线审计与二次校验
    QJsonObject marker;
    marker[QStringLiteral("frozenAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    {
        // manifest 是字符串不是文件，直接对字符串内容做 SHA256
        QCryptographicHash h(QCryptographicHash::Sha256);
        h.addData(manifestJson.toUtf8());
        marker[QStringLiteral("manifestSha256")] = QString::fromLatin1(h.result().toHex());
    }
    marker[QStringLiteral("manifestJson")] = manifestJson;

    QString markerPath = snapshotDir + QStringLiteral("/.frozen.json");
    QString markerJson = QString::fromUtf8(QJsonDocument(marker).toJson(QJsonDocument::Compact));
    return writeFileAtomically(markerPath, markerJson);
}

bool SnapshotService::isFrozenCopyPresent(const QString &snapshotDir)
{
    return QFile::exists(snapshotDir + QStringLiteral("/.frozen.json"));
}

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

    clearLastError();

    // 1. 收集所有有效样本 ID 及其文件路径（路径用于计算内容哈希）
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id, image_path, label_path FROM dataset_samples "
                        "WHERE dataset_id = ? AND validation_status IN ('valid', 'good', 'defective') ORDER BY id");
    sampleQuery.addBindValue(datasetId);
    if (!sampleQuery.exec()) return {};

    QStringList allSampleIds;
    // 样本 ID → [imagePath, labelPath]，后续计算哈希用
    QMap<QString, QStringList> samplePaths;
    while (sampleQuery.next()) {
        QString sid = sampleQuery.value(0).toString();
        allSampleIds.append(sid);
        samplePaths.insert(sid, {sampleQuery.value(1).toString(), sampleQuery.value(2).toString()});
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

    // 5. 构建样本 manifest JSON（P0-1：含每个样本的图片/标签内容哈希）
    // 新格式：[{"id":"...","imageHash":"sha256...","labelHash":"sha256..."}, ...]
    // 旧格式（纯 ID 字符串数组）仍可被 getSampleManifest 兼容读取
    QJsonArray manifestArray;
    int hashedCount = 0;
    for (const auto &id : allSampleIds) {
        QJsonObject entry;
        entry[QStringLiteral("id")] = id;

        QStringList paths = samplePaths.value(id);
        QString imagePath = paths.value(0);
        QString labelPath = paths.value(1);

        // 图片哈希：文件存在则计算；不存在（异常样本）置空串，后续校验会报缺失
        QString imageHash = computeFileSha256(imagePath);
        entry[QStringLiteral("imageHash")] = imageHash;
        if (!imageHash.isEmpty()) hashedCount++;

        // 标签哈希：分类任务的 label_path 存的是类别名而非文件路径，此时置空串跳过校验
        QString labelHash = computeFileSha256(labelPath);
        entry[QStringLiteral("labelHash")] = labelHash;

        manifestArray.append(entry);
    }
    QJsonDocument manifestDoc(manifestArray);
    QString manifestJson = QString::fromUtf8(manifestDoc.toJson(QJsonDocument::Compact));

    ltInfo(LT_LOG_TRAINING()) << "Snapshot manifest hashed:" << hashedCount << "/" << allSampleIds.size()
                              << "samples with content hash";

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
        if (deleteQuery.numRowsAffected() == 0) {
            ltWarning(LT_LOG_TRAINING()) << "Snapshot not found for deletion:" << snapshotId;
            return false;
        }
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
        // 兼容旧格式（纯字符串 ID）与新格式（含哈希的对象）
        if (item.isObject()) {
            result.append(item.toObject().value(QStringLiteral("id")).toString());
        } else {
            result.append(item.toString());
        }
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

    // 1. 记录必须存在
    QSqlQuery query(db);
    query.prepare("SELECT dataset_id, sample_manifest_json FROM dataset_snapshots WHERE id = ?");
    query.addBindValue(snapshotId);

    if (!query.exec() || !query.next()) return false;

    QString datasetId = query.value(0).toString();
    QString manifestJson = query.value(1).toString();
    if (manifestJson.isEmpty()) return false;

    // 2. 冻结标记已存在 → 物理副本已冻结，视为不可变
    // 解析项目根目录以定位冻结目录
    QSqlQuery dsQuery(db);
    dsQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    dsQuery.addBindValue(datasetId);
    if (dsQuery.exec() && dsQuery.next()) {
        QString projectId = dsQuery.value(0).toString();
        QSqlQuery projQuery(db);
        projQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
        projQuery.addBindValue(projectId);
        if (projQuery.exec() && projQuery.next()) {
            QString snapshotDir = projQuery.value(0).toString()
                                  + QStringLiteral("/cache/snapshots/") + snapshotId;
            if (isFrozenCopyPresent(snapshotDir)) {
                return true;
            }
        }
    }

    // 3. 无冻结目录时，校验 manifest 哈希清单完整性：
    //    每个样本条目都必须含非空 imageHash（labelHash 允许为空——分类任务无标签文件）
    QJsonDocument doc = QJsonDocument::fromJson(manifestJson.toUtf8());
    if (!doc.isArray() || doc.array().isEmpty()) return false;

    for (const auto &val : doc.array()) {
        if (!val.isObject()) {
            // 旧格式纯字符串 ID → 无哈希，不满足完整性要求
            return false;
        }
        QJsonObject obj = val.toObject();
        if (obj.value(QStringLiteral("imageHash")).toString().isEmpty()) {
            return false;
        }
    }

    return true;
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

QString SnapshotService::prepareSnapshotPhysicalDir(const QString &snapshotId, bool materializeFrozenCopy)
{
    ltInfo(LT_LOG_TRAINING()) << "Preparing snapshot physical directory for" << snapshotId
                              << "materializeFrozenCopy=" << materializeFrozenCopy;
    clearLastError();

    QString dbPath = Database::instance().dbPath();
    QString connectionName = QStringLiteral("thread_snap_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ThreadDbGuard guard{connectionName};

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        setLastError(QStringLiteral("E_DB_ERROR"),
                     QStringLiteral("无法打开数据库连接"));
        return {};
    }

    // 1. Get snapshot details（含 sample_manifest_json 用于哈希校验）
    QSqlQuery snapQuery(db);
    snapQuery.prepare("SELECT dataset_id, sample_manifest_json, split_manifest_json, taxonomy_version "
                      "FROM dataset_snapshots WHERE id = ?");
    snapQuery.addBindValue(snapshotId);
    if (!snapQuery.exec() || !snapQuery.next()) {
        setLastError(QStringLiteral("E_SNAPSHOT_NOT_FOUND"),
                     QStringLiteral("快照不存在：%1").arg(snapshotId));
        return {};
    }

    QString datasetId = snapQuery.value(0).toString();
    QString sampleManifestJson = snapQuery.value(1).toString();
    QString splitManifestJson = snapQuery.value(2).toString();
    QString taxonomyVersion = snapQuery.value(3).toString();

    // 2. Get project root path
    QSqlQuery datasetQuery(db);
    datasetQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    datasetQuery.addBindValue(datasetId);
    if (!datasetQuery.exec() || !datasetQuery.next()) {
        setLastError(QStringLiteral("E_DB_ERROR"),
                     QStringLiteral("找不到数据集：%1").arg(datasetId));
        return {};
    }
    QString projectId = datasetQuery.value(0).toString();

    QSqlQuery projectQuery(db);
    projectQuery.prepare("SELECT root_path, task_type FROM projects WHERE id = ?");
    projectQuery.addBindValue(projectId);
    if (!projectQuery.exec() || !projectQuery.next()) {
        setLastError(QStringLiteral("E_DB_ERROR"),
                     QStringLiteral("找不到项目：%1").arg(projectId));
        return {};
    }
    QString projectRoot = projectQuery.value(0).toString();

    // 2b. 获取项目任务类型
    QString taskType = projectQuery.value(1).toString();
    bool isAnomaly = (taskType == QStringLiteral("anomaly"));
    bool isClassify = (taskType == QStringLiteral("classify"));

    // 3. Define target snapshot directory
    QString cacheDir = projectRoot + QStringLiteral("/cache/snapshots");
    QString snapshotDir = cacheDir + QStringLiteral("/") + snapshotId;

    // 3a. P0-1：若冻结副本已存在，直接使用冻结副本，不再回源校验/拷贝
    if (isFrozenCopyPresent(snapshotDir)) {
        ltInfo(LT_LOG_TRAINING()) << "Frozen copy already present, reusing:" << snapshotDir;
        QString existingYaml = snapshotDir + QStringLiteral("/data.yaml");
        if (QFile::exists(existingYaml)) {
            return existingYaml;
        }
        // data.yaml 缺失（历史中断）——继续走生成流程，但跳过哈希校验（数据已冻结）
        // 通过 parseManifestHashes 返回空并设置特殊标记来跳过，下面拷贝逻辑不会执行
        // 直接跳到生成 data.yaml 阶段：走拷贝会覆盖冻结文件，因此只补生成配置
    } else {
        // 3b. 冻结副本不存在 → 校验源文件哈希与 manifest 一致（P0-1 核心防线）
        QMap<QString, QJsonObject> manifestHashes = parseManifestHashes(sampleManifestJson);
        if (manifestHashes.isEmpty() && !sampleManifestJson.isEmpty()) {
            // 旧格式 manifest（无哈希）：无法校验完整性，拒绝以防源数据漂移
            setLastError(QStringLiteral("E_MANIFEST_INVALID"),
                         QStringLiteral("快照 manifest 缺少内容哈希，无法验证数据完整性。"
                                        "请删除此快照后重新创建。"));
            return {};
        }

        // 解析 split manifest 获取所有样本 ID
        QJsonParseError parseError;
        QJsonDocument splitDoc = QJsonDocument::fromJson(splitManifestJson.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            setLastError(QStringLiteral("E_MANIFEST_INVALID"),
                         QStringLiteral("split manifest 解析失败：%1").arg(parseError.errorString()));
            return {};
        }
        QJsonObject splitObj = splitDoc.object();
        QJsonArray allSamples = splitObj[QStringLiteral("train")].toArray();
        // 注意：QJsonArray::append(const QJsonArray&) 会把数组作为单个嵌套元素追加，
        // 必须逐元素展开 val 数组，否则会产生空字符串样本 ID 导致误判漂移
        QJsonArray valArray = splitObj[QStringLiteral("val")].toArray();
        for (const auto &v : valArray) {
            allSamples.append(v);
        }

        // 逐样本校验当前文件哈希与 manifest 冻结哈希
        QStringList driftedSamples;
        QStringList missingFiles;
        for (const auto &val : allSamples) {
            QString sampleId = val.toString();
            QJsonObject expected = manifestHashes.value(sampleId);
            if (expected.isEmpty()) {
                // manifest 中找不到该样本——数据不一致
                driftedSamples.append(sampleId);
                continue;
            }

            QSqlQuery sampleQuery(db);
            sampleQuery.prepare("SELECT image_path, label_path FROM dataset_samples WHERE id = ?");
            sampleQuery.addBindValue(sampleId);
            if (!sampleQuery.exec() || !sampleQuery.next()) {
                missingFiles.append(sampleId);
                continue;
            }
            QString srcImg = sampleQuery.value(0).toString();
            QString srcLbl = sampleQuery.value(1).toString();

            // 校验图片哈希
            QString expectedImgHash = expected.value(QStringLiteral("imageHash")).toString();
            QString actualImgHash = computeFileSha256(srcImg);
            if (actualImgHash.isEmpty()) {
                missingFiles.append(sampleId);
                continue;
            }
            if (actualImgHash != expectedImgHash) {
                driftedSamples.append(sampleId);
                continue;
            }

            // 校验标签哈希（labelHash 为空表示该样本无标签文件，跳过）
            QString expectedLblHash = expected.value(QStringLiteral("labelHash")).toString();
            if (!expectedLblHash.isEmpty()) {
                QString actualLblHash = computeFileSha256(srcLbl);
                if (actualLblHash != expectedLblHash) {
                    driftedSamples.append(sampleId);
                    continue;
                }
            }
        }

        if (!missingFiles.isEmpty()) {
            setLastError(QStringLiteral("E_FILE_MISSING"),
                         QStringLiteral("快照源文件缺失（%1 个样本），无法准备训练数据")
                             .arg(missingFiles.size()));
            return {};
        }

        if (!driftedSamples.isEmpty()) {
            if (!materializeFrozenCopy) {
                // 默认拒绝：源数据已变更，不能悄悄用漂移后的数据训练
                setLastError(QStringLiteral("E_SOURCE_DRIFT"),
                             QStringLiteral("快照源数据已变更（%1 个样本哈希不一致）。"
                                            "如需继续，请选择「物化冻结副本」以当前数据重新冻结。")
                                 .arg(driftedSamples.size()));
                return {};
            }
            // 用户显式选择物化冻结副本：记录漂移警告，继续用当前数据物化
            ltWarning(LT_LOG_TRAINING()) << "Source drift detected for" << driftedSamples.size()
                                         << "samples; materializing frozen copy from current state as requested";
        }
    }

    // 4. 创建物理目录结构
    QDir dir;
    if (isAnomaly) {
        // Anomalib 目录结构: train/good + test/good + test/defective
        if (!dir.mkpath(snapshotDir + QStringLiteral("/train/good")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/test/good")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/test/defective"))) {
            setLastError(QStringLiteral("E_COPY_FAILED"),
                         QStringLiteral("无法创建 anomaly 物理目录：%1").arg(snapshotDir));
            return {};
        }
    } else if (isClassify) {
        // 分类目录结构: train/ + val/（子目录在拷贝样本时按类别创建）
        if (!dir.mkpath(snapshotDir + QStringLiteral("/train")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/val"))) {
            setLastError(QStringLiteral("E_COPY_FAILED"),
                         QStringLiteral("无法创建分类物理目录：%1").arg(snapshotDir));
            return {};
        }
    } else {
        // YOLO 目录结构: images/train + images/val + labels/train + labels/val
        if (!dir.mkpath(snapshotDir + QStringLiteral("/images/train")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/images/val")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/labels/train")) ||
            !dir.mkpath(snapshotDir + QStringLiteral("/labels/val"))) {
            setLastError(QStringLiteral("E_COPY_FAILED"),
                         QStringLiteral("无法创建 YOLO 物理目录：%1").arg(snapshotDir));
            return {};
        }
    }

    // 5. Parse split manifest（用于拷贝阶段）
    QJsonParseError parseError;
    QJsonDocument splitDoc = QJsonDocument::fromJson(splitManifestJson.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setLastError(QStringLiteral("E_MANIFEST_INVALID"),
                     QStringLiteral("split manifest 解析失败：%1").arg(parseError.errorString()));
        return {};
    }

    QJsonObject splitObj = splitDoc.object();
    QJsonArray trainSamples = splitObj[QStringLiteral("train")].toArray();
    QJsonArray valSamples = splitObj[QStringLiteral("val")].toArray();

    auto copySamples = [&](const QJsonArray &samples, const QString &splitName) -> bool {
        for (const auto &val : samples) {
            QString sampleId = val.toString();
            QSqlQuery sampleQuery(db);
            sampleQuery.prepare("SELECT image_path, label_path, validation_status FROM dataset_samples WHERE id = ?");
            sampleQuery.addBindValue(sampleId);
            if (!sampleQuery.exec() || !sampleQuery.next()) {
                ltWarning(LT_LOG_TRAINING()) << "Sample not found during snapshot preparation:" << sampleId;
                continue;
            }

            QString srcImg = sampleQuery.value(0).toString();
            QString srcLbl = sampleQuery.value(1).toString();
            QString validationStatus = sampleQuery.value(2).toString();

            QFileInfo imgInfo(srcImg);
            QString dstImg;

            if (isAnomaly) {
                // Anomaly 格式：train/good 放训练图片，test/good 或 test/defective 放验证图片
                if (splitName == QStringLiteral("train")) {
                    dstImg = snapshotDir + QStringLiteral("/train/good/") + imgInfo.fileName();
                } else {
                    // 验证集：依据导入阶段记录的样本状态决定 good/defective 目录
                    if (validationStatus == QStringLiteral("defective")) {
                        dstImg = snapshotDir + QStringLiteral("/test/defective/") + imgInfo.fileName();
                    } else {
                        dstImg = snapshotDir + QStringLiteral("/test/good/") + imgInfo.fileName();
                    }
                }
            } else if (isClassify) {
                // 分类格式：train/类名/ 或 val/类名/（label_path 存储类别名）
                QString className = srcLbl; // 分类数据集的 label_path 存储类别名
                if (className.isEmpty()) {
                    className = QStringLiteral("unknown");
                }
                // 创建类别子目录
                QString classDir = snapshotDir + QStringLiteral("/") + splitName + QStringLiteral("/") + className;
                dir.mkpath(classDir);
                dstImg = classDir + QStringLiteral("/") + imgInfo.fileName();
            } else {
                dstImg = snapshotDir + QStringLiteral("/images/") + splitName + QStringLiteral("/") + imgInfo.fileName();
            }

            if (QFile::exists(dstImg)) {
                QFile::remove(dstImg);
            }
            if (!QFile::copy(srcImg, dstImg)) {
                ltError(LT_LOG_TRAINING()) << "Failed to copy image from" << srcImg << "to" << dstImg;
                return false;
            }

            if (!isAnomaly && !isClassify) {
                // YOLO 格式需要标签文件
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
                } else {
                    QFileInfo imgInfo2(srcImg);
                    QString dstLbl = snapshotDir + QStringLiteral("/labels/") + splitName + QStringLiteral("/")
                                     + imgInfo2.completeBaseName() + QStringLiteral(".txt");
                    if (!QFile::exists(dstLbl)) {
                        QFile file(dstLbl);
                        file.open(QIODevice::WriteOnly);
                        file.close();
                    }
                }
            }
        }
        return true;
    };

    // 若冻结副本已存在但 data.yaml 缺失，只补配置不覆盖冻结数据文件
    bool frozenAlready = isFrozenCopyPresent(snapshotDir);
    if (!frozenAlready) {
        if (!copySamples(trainSamples, QStringLiteral("train")) ||
            !copySamples(valSamples, QStringLiteral("val"))) {
            setLastError(QStringLiteral("E_COPY_FAILED"),
                         QStringLiteral("拷贝样本文件到快照目录失败"));
            return {};
        }
    }

    // 6. Query taxonomy classes
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

    // 7. 生成配置文件 + 写入冻结标记
    // 冻结标记在数据拷贝完成后写入，之后再调用本接口将直接复用冻结副本
    auto finalizeWithYaml = [&](const QString &yamlPath, const QString &content) -> QString {
        if (!writeFileAtomically(yamlPath, content)) {
            setLastError(QStringLiteral("E_COPY_FAILED"),
                         QStringLiteral("写入 data.yaml 失败：%1").arg(yamlPath));
            return {};
        }
        if (!frozenAlready) {
            if (!writeFreezeMarker(snapshotDir, sampleManifestJson)) {
                ltWarning(LT_LOG_TRAINING()) << "Failed to write freeze marker for" << snapshotDir;
                // 冻结标记写入失败不阻断训练，但记录警告——下次会重新校验哈希
            }
        }
        return yamlPath;
    };

    if (isAnomaly) {
        // Anomaly 类型：生成 data.yaml 指向目录结构，Anomalib 适配器会自动识别
        QString yamlPath = snapshotDir + QStringLiteral("/data.yaml");
        QString content;
        QTextStream stream(&content);
        stream << "path: " << QDir(snapshotDir).absolutePath() << "\n";
        stream << "task: anomaly\n";
        stream << "normal_dir: train/good\n";
        stream << "abnormal_dir: test/defective\n";
        stream << "normal_test_dir: test/good\n";

        QString result = finalizeWithYaml(yamlPath, content);
        if (!result.isEmpty()) {
            ltInfo(LT_LOG_TRAINING()) << "Anomaly snapshot prepared at:" << snapshotDir;
        }
        return result;
    } else if (isClassify) {
        // 分类类型：生成简洁的 data.yaml（Ultralytics 从目录结构自动推断类别）
        QString yamlPath = snapshotDir + QStringLiteral("/data.yaml");
        QString content;
        QTextStream stream(&content);
        stream << "path: " << QDir(snapshotDir).absolutePath() << "\n";
        stream << "train: train\n";
        stream << "val: val\n";

        QString result = finalizeWithYaml(yamlPath, content);
        if (!result.isEmpty()) {
            ltInfo(LT_LOG_TRAINING()) << "Classify snapshot prepared at:" << snapshotDir;
        }
        return result;
    } else {
        // YOLO 类型：生成标准 data.yaml
        QString yamlPath = snapshotDir + QStringLiteral("/data.yaml");
        QString content;
        QTextStream stream(&content);
        stream << "path: " << QDir(snapshotDir).absolutePath() << "\n";
        stream << "train: images/train\n";
        stream << "val: images/val\n";
        stream << "nc: " << classes.size() << "\n";
        stream << "names:\n";
        for (int i = 0; i < classes.size(); ++i) {
            stream << "  " << i << ": " << classes[i] << "\n";
        }

        QString result = finalizeWithYaml(yamlPath, content);
        if (!result.isEmpty()) {
            ltInfo(LT_LOG_TRAINING()) << "YOLO snapshot prepared at:" << snapshotDir;
        }
        return result;
    }
}

// ============================================================================
// P0-2: 数据快照预览图（supervision 集成）
// ============================================================================

void SnapshotService::setIpcClient(IpcClient *client)
{
    ltTrace(LT_LOG_TRAINING()) << "client=" << client;
    if (m_ipcClient) {
        disconnect(m_ipcClient, &IpcClient::responseReceived,
                   this, &SnapshotService::onPreviewResponseReceived);
    }
    m_ipcClient = client;
    if (m_ipcClient) {
        // 仅过滤处理 snapshot.preview 响应，其他命令忽略
        connect(m_ipcClient, &IpcClient::responseReceived,
                this, &SnapshotService::onPreviewResponseReceived);
    }
}

QString SnapshotService::generatePreview(const QString &snapshotId)
{
    ltTrace(LT_LOG_TRAINING()) << "snapshotId=" << snapshotId;

    if (!m_ipcClient) {
        ltError(LT_LOG_TRAINING()) << "IpcClient not injected for snapshot preview";
        emit previewGenerated(snapshotId, QString(), false, QStringLiteral("IPC 客户端未注入"));
        return {};
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) {
        emit previewGenerated(snapshotId, QString(), false, QStringLiteral("数据库未打开"));
        return {};
    }

    // 1. 校验快照存在并解析所属项目根目录 + dataset_id
    QSqlQuery snapQuery(db);
    snapQuery.prepare("SELECT dataset_id FROM dataset_snapshots WHERE id = ?");
    snapQuery.addBindValue(snapshotId);
    if (!snapQuery.exec() || !snapQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Snapshot not found:" << snapshotId;
        emit previewGenerated(snapshotId, QString(), false, QStringLiteral("快照不存在"));
        return {};
    }
    QString datasetId = snapQuery.value(0).toString();

    QSqlQuery projQuery(db);
    projQuery.prepare(
        "SELECT p.root_path FROM projects p "
        "JOIN datasets d ON d.project_id = p.id WHERE d.id = ?");
    projQuery.addBindValue(datasetId);
    if (!projQuery.exec() || !projQuery.next()) {
        ltError(LT_LOG_TRAINING()) << "Project root not resolvable for dataset:" << datasetId;
        emit previewGenerated(snapshotId, QString(), false, QStringLiteral("无法解析项目根目录"));
        return {};
    }
    QString projectRoot = projQuery.value(0).toString();
    QString snapshotDir = projectRoot + QStringLiteral("/cache/snapshots/") + snapshotId;
    QString dataYamlPath = snapshotDir + QStringLiteral("/data.yaml");

    // 2. 若物理目录尚未准备，先调用 prepareSnapshotPhysicalDir
    if (!QFileInfo::exists(dataYamlPath)) {
        ltInfo(LT_LOG_TRAINING()) << "Physical dir not ready, preparing:" << snapshotId;
        QString prepared = prepareSnapshotPhysicalDir(snapshotId);
        if (prepared.isEmpty()) {
            emit previewGenerated(snapshotId, QString(), false,
                                  QStringLiteral("快照物理目录准备失败"));
            return {};
        }
        dataYamlPath = prepared;
    }

    // 3. 发起 IPC 请求 snapshot.preview
    QJsonObject payload;
    payload["snapshot_dir"] = snapshotDir;
    payload["data_yaml_path"] = dataYamlPath;
    payload["max_samples"] = 9;
    payload["grid_cols"] = 3;
    payload["thumb_size"] = 320;

    QString requestId = m_ipcClient->sendRequest(IpcProtocol::CMD_SNAPSHOT_PREVIEW, payload);
    if (requestId.isEmpty()) {
        ltError(LT_LOG_TRAINING()) << "Failed to send snapshot.preview request";
        emit previewGenerated(snapshotId, QString(), false, QStringLiteral("IPC 请求发送失败"));
        return {};
    }

    m_pendingPreviews[requestId] = snapshotId;
    ltInfo(LT_LOG_TRAINING()) << "Snapshot preview requested:" << snapshotId
                              << "requestId=" << requestId;
    return requestId;
}

QString SnapshotService::getPreviewPath(const QString &snapshotId) const
{
    if (snapshotId.isEmpty()) return {};

    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    QSqlQuery snapQuery(db);
    snapQuery.prepare(
        "SELECT d.project_id FROM dataset_snapshots s "
        "JOIN datasets d ON d.id = s.dataset_id WHERE s.id = ?");
    snapQuery.addBindValue(snapshotId);
    if (!snapQuery.exec() || !snapQuery.next()) return {};

    QString projectId = snapQuery.value(0).toString();
    QSqlQuery projQuery(db);
    projQuery.prepare("SELECT root_path FROM projects WHERE id = ?");
    projQuery.addBindValue(projectId);
    if (!projQuery.exec() || !projQuery.next()) return {};

    QString previewPath = projQuery.value(0).toString()
                          + QStringLiteral("/cache/snapshots/")
                          + snapshotId
                          + QStringLiteral("/preview.jpg");
    return QFileInfo::exists(previewPath) ? previewPath : QString();
}

void SnapshotService::onPreviewResponseReceived(const QJsonObject &response)
{
    // 仅处理 snapshot.preview 响应，其他命令直接返回
    QString command = response.value("command").toString();
    if (command != IpcProtocol::CMD_SNAPSHOT_PREVIEW) {
        return;
    }

    QString requestId = response.value("request_id").toString();
    if (!m_pendingPreviews.contains(requestId)) {
        return; // 不是本服务发起的请求
    }

    QString snapshotId = m_pendingPreviews.take(requestId);
    bool success = response.value("success").toBool(false);
    QJsonObject result = response.value("result").toObject();
    QString error = response.value("error").toObject().value("message").toString();

    if (success) {
        QString previewPath = result.value("preview_path").toString();
        if (previewPath.isEmpty()) {
            // 后端未返回路径，回退到本地推断
            previewPath = getPreviewPath(snapshotId);
        }
        ltInfo(LT_LOG_TRAINING()) << "Snapshot preview generated:" << snapshotId
                                  << "path=" << previewPath;
        emit previewGenerated(snapshotId, previewPath, true, QString());
    } else {
        ltError(LT_LOG_TRAINING()) << "Snapshot preview failed:" << snapshotId
                                   << "error=" << error;
        emit previewGenerated(snapshotId, QString(), false,
                              error.isEmpty() ? QStringLiteral("后端生成预览失败") : error);
    }
}
