#include "AssistedLabelService.h"
#include "Database.h"
#include "utils/Log.h"
#include "utils/Id.h"
#include "geometry/AxisAlignedBox.h"
#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"
#include "SnapshotService.h"
#include "TrainingService.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <algorithm>

AssistedLabelService::AssistedLabelService(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_INFERENCE()) << "parent=" << parent;
}

QVariantList AssistedLabelService::getCandidates(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    QVariantList result;
    QJsonObject snapshotObj;

    if (!readSnapshot(batchId, snapshotObj)) return result;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    for (const auto &cand : candidates) {
        QJsonObject obj = cand.toObject();
        QVariantMap candidate;
        candidate["className"] = obj.value("className").toString();
        candidate["classIndex"] = obj.value("classIndex").toInt();
        candidate["cx"] = obj.value("cx").toDouble();
        candidate["cy"] = obj.value("cy").toDouble();
        candidate["w"] = obj.value("w").toDouble();
        candidate["h"] = obj.value("h").toDouble();
        candidate["confidence"] = obj.value("confidence").toDouble();
        candidate["state"] = obj.value("state").toString("pending");
        result.append(candidate);
    }

    ltDebug(LT_LOG_INFERENCE()) << "Retrieved" << result.size() << "candidates for batch:" << batchId;
    return result;
}

bool AssistedLabelService::confirmCandidate(const QString &batchId, int candidateIndex)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "candidateIndex=" << candidateIndex;

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return false;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        ltWarning(LT_LOG_INFERENCE()) << "Candidate index out of range:" << candidateIndex;
        return false;
    }

    QJsonObject candidate = candidates[candidateIndex].toObject();
    candidate["state"] = "confirmed";
    candidates[candidateIndex] = candidate;

    snapshotObj["candidates"] = candidates;
    if (writeSnapshot(batchId, snapshotObj)) {
        ltInfo(LT_LOG_INFERENCE()) << "Confirmed candidate" << candidateIndex << "in batch:" << batchId;
        return true;
    }
    return false;
}

bool AssistedLabelService::rejectCandidate(const QString &batchId, int candidateIndex)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "candidateIndex=" << candidateIndex;

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return false;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        ltWarning(LT_LOG_INFERENCE()) << "Candidate index out of range:" << candidateIndex;
        return false;
    }

    QJsonObject candidate = candidates[candidateIndex].toObject();
    candidate["state"] = "rejected";
    candidates[candidateIndex] = candidate;

    snapshotObj["candidates"] = candidates;
    if (writeSnapshot(batchId, snapshotObj)) {
        ltInfo(LT_LOG_INFERENCE()) << "Rejected candidate" << candidateIndex << "in batch:" << batchId;
        return true;
    }
    return false;
}

int AssistedLabelService::confirmAllAboveThreshold(const QString &batchId, double threshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "threshold=" << threshold;

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return -1;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    int count = 0;

    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject candidate = candidates[i].toObject();
        double conf = candidate.value("confidence").toDouble();
        QString state = candidate.value("state").toString("pending");
        if (conf >= threshold && state == "pending") {
            candidate["state"] = "confirmed";
            candidates[i] = candidate;
            ++count;
        }
    }

    snapshotObj["candidates"] = candidates;
    if (!writeSnapshot(batchId, snapshotObj)) return -1;

    ltInfo(LT_LOG_INFERENCE()) << "Confirmed" << count << "candidates above threshold" << threshold << "in batch:" << batchId;
    return count;
}

int AssistedLabelService::rejectAllBelowThreshold(const QString &batchId, double threshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "threshold=" << threshold;

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return -1;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    int count = 0;

    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject candidate = candidates[i].toObject();
        double conf = candidate.value("confidence").toDouble();
        QString state = candidate.value("state").toString("pending");
        if (conf < threshold && state == "pending") {
            candidate["state"] = "rejected";
            candidates[i] = candidate;
            ++count;
        }
    }

    snapshotObj["candidates"] = candidates;
    if (!writeSnapshot(batchId, snapshotObj)) return -1;

    ltInfo(LT_LOG_INFERENCE()) << "Rejected" << count << "candidates below threshold" << threshold << "in batch:" << batchId;
    return count;
}

QVariantMap AssistedLabelService::getBatchStats(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    QVariantMap result;
    result["total"] = 0;
    result["confirmed"] = 0;
    result["rejected"] = 0;
    result["pending"] = 0;
    result["edited"] = 0;

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return result;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    int total = candidates.size();
    int confirmed = 0, rejected = 0, pending = 0, edited = 0;

    for (const auto &cand : candidates) {
        QString state = cand.toObject().value("state").toString("pending");
        if (state == "confirmed") ++confirmed;
        else if (state == "rejected") ++rejected;
        else if (state == "edited") ++edited;
        else ++pending;
    }

    result["total"] = total;
    result["confirmed"] = confirmed;
    result["rejected"] = rejected;
    result["pending"] = pending;
    result["edited"] = edited;

    return result;
}

QVariantList AssistedLabelService::getLowConfidenceSamples(const QString &batchId, float threshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "threshold=" << threshold;

    QVariantList result;
    QJsonObject snapshotObj;

    if (!readSnapshot(batchId, snapshotObj)) return result;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject obj = candidates[i].toObject();
        double conf = obj.value("confidence").toDouble();
        if (conf < threshold) {
            QVariantMap candidate;
            candidate["candidateIndex"] = i;
            candidate["className"] = obj.value("className").toString();
            candidate["classIndex"] = obj.value("classIndex").toInt();
            candidate["cx"] = obj.value("cx").toDouble();
            candidate["cy"] = obj.value("cy").toDouble();
            candidate["w"] = obj.value("w").toDouble();
            candidate["h"] = obj.value("h").toDouble();
            candidate["confidence"] = conf;
            candidate["state"] = obj.value("state").toString("pending");
            result.append(candidate);
        }
    }

    return result;
}

QVariantMap AssistedLabelService::getConfidenceStats(const QString &batchId, float threshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "threshold=" << threshold;

    QVariantMap result;
    result["totalCandidates"] = 0;
    result["lowConfCount"] = 0;
    result["highConfCount"] = 0;
    result["averageConfidence"] = 0.0;
    result["threshold"] = static_cast<double>(threshold);

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return result;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    int total = candidates.size();
    int lowConf = 0;
    int highConf = 0;
    double confSum = 0.0;

    for (const auto &cand : candidates) {
        double conf = cand.toObject().value("confidence").toDouble();
        confSum += conf;
        if (conf < threshold) {
            ++lowConf;
        } else {
            ++highConf;
        }
    }

    result["totalCandidates"] = total;
    result["lowConfCount"] = lowConf;
    result["highConfCount"] = highConf;
    result["averageConfidence"] = total > 0 ? confSum / total : 0.0;
    result["threshold"] = static_cast<double>(threshold);

    return result;
}

QVariantList AssistedLabelService::getFalsePositives(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    QVariantList result;
    QJsonObject snapshotObj;

    if (!readSnapshot(batchId, snapshotObj)) return result;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    for (int i = 0; i < candidates.size(); ++i) {
        QJsonObject obj = candidates[i].toObject();
        QString state = obj.value("state").toString("pending");
        if (state == "rejected") {
            QVariantMap candidate;
            candidate["candidateIndex"] = i;
            candidate["className"] = obj.value("className").toString();
            candidate["classIndex"] = obj.value("classIndex").toInt();
            candidate["cx"] = obj.value("cx").toDouble();
            candidate["cy"] = obj.value("cy").toDouble();
            candidate["w"] = obj.value("w").toDouble();
            candidate["h"] = obj.value("h").toDouble();
            candidate["confidence"] = obj.value("confidence").toDouble();
            candidate["state"] = state;
            if (obj.contains("sampleId")) {
                candidate["sampleId"] = obj.value("sampleId").toString();
            }
            result.append(candidate);
        }
    }

    return result;
}

QVariantList AssistedLabelService::getFalseNegatives(const QString &batchId, const QString &datasetId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "datasetId=" << datasetId;

    QVariantList result;
    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // Collect all sample IDs from dataset_samples for the given dataset
    QSet<QString> allSampleIds;
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ?");
    sampleQuery.addBindValue(datasetId);
    if (!sampleQuery.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to query dataset_samples:" << sampleQuery.lastError().text();
        return result;
    }
    while (sampleQuery.next()) {
        allSampleIds.insert(sampleQuery.value(0).toString());
    }

    // Collect sample IDs that had detections in this batch
    QSet<QString> processedSampleIds;

    QJsonObject snapshotObj;
    if (readSnapshot(batchId, snapshotObj)) {
        // Check for "processedSamples" array at root level
        QJsonArray processedArr = snapshotObj.value("processedSamples").toArray();
        for (const auto &val : processedArr) {
            processedSampleIds.insert(val.toString());
        }

        // Also check for "sampleId" in each candidate
        QJsonArray candidates = snapshotObj.value("candidates").toArray();
        for (const auto &cand : candidates) {
            QJsonObject obj = cand.toObject();
            if (obj.contains("sampleId")) {
                processedSampleIds.insert(obj.value("sampleId").toString());
            }
        }
    }

    // False negatives = samples in dataset but not processed by this batch
    QSet<QString> fnSampleIds = allSampleIds - processedSampleIds;
    for (const auto &sampleId : fnSampleIds) {
        result.append(sampleId);
    }

    ltDebug(LT_LOG_INFERENCE()) << "Found" << result.size() << "false negatives for batch:" << batchId;
    return result;
}

QVariantList AssistedLabelService::getHardCaseQueue(const QString &batchId, const QString &datasetId, float lowConfThreshold)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId << "datasetId=" << datasetId << "lowConfThreshold=" << lowConfThreshold;

    QVariantList result;

    // Collect false negatives (priority 3 - highest)
    QVariantList falseNegatives = getFalseNegatives(batchId, datasetId);
    for (const auto &item : falseNegatives) {
        QVariantMap entry;
        entry["sampleId"] = item.toString();
        entry["reason"] = "false_negative";
        entry["priority"] = 3;
        entry["confidence"] = 0.0;
        result.append(entry);
    }

    // Collect low-confidence samples (priority 2)
    QVariantList lowConfSamples = getLowConfidenceSamples(batchId, lowConfThreshold);
    for (const auto &item : lowConfSamples) {
        QVariantMap lcMap = item.toMap();
        QVariantMap entry;
        entry["sampleId"] = lcMap.value("sampleId", "").toString();
        entry["reason"] = "low_confidence";
        entry["priority"] = 2;
        entry["confidence"] = lcMap.value("confidence", 0.0).toDouble();
        entry["candidateIndex"] = lcMap.value("candidateIndex", -1);
        entry["className"] = lcMap.value("className", "");
        entry["classIndex"] = lcMap.value("classIndex", -1);
        result.append(entry);
    }

    // Collect false positives (priority 1 - lowest)
    QVariantList falsePositives = getFalsePositives(batchId);
    for (const auto &item : falsePositives) {
        QVariantMap fpMap = item.toMap();
        QVariantMap entry;
        entry["sampleId"] = fpMap.value("sampleId", "").toString();
        entry["reason"] = "false_positive";
        entry["priority"] = 1;
        entry["confidence"] = fpMap.value("confidence", 0.0).toDouble();
        entry["candidateIndex"] = fpMap.value("candidateIndex", -1);
        entry["className"] = fpMap.value("className", "");
        entry["classIndex"] = fpMap.value("classIndex", -1);
        result.append(entry);
    }

    // Sort by priority descending (highest priority first)
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("priority").toInt() > b.toMap().value("priority").toInt();
    });

    ltDebug(LT_LOG_INFERENCE()) << "Built hard case queue with" << result.size() << "entries for batch:" << batchId;
    return result;
}

void AssistedLabelService::setSnapshotService(SnapshotService *service)
{
    ltTrace(LT_LOG_INFERENCE()) << "service=" << service;
    m_snapshotService = service;
}

void AssistedLabelService::setTrainingService(TrainingService *service)
{
    ltTrace(LT_LOG_INFERENCE()) << "service=" << service;
    m_trainingService = service;
}

QString AssistedLabelService::batchDatasetId(const QString &batchId)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    QSqlQuery query(db);
    query.prepare("SELECT dataset_id FROM assisted_label_batches WHERE id = ?");
    query.addBindValue(batchId);
    if (!query.exec() || !query.next()) {
        ltWarning(LT_LOG_INFERENCE()) << "Batch not found for dataset lookup:" << batchId;
        return {};
    }
    return query.value(0).toString();
}

AxisAlignedBox AssistedLabelService::boxFromCandidate(const QJsonObject &cand)
{
    AxisAlignedBox box;
    box.classIndex = cand.value("classIndex").toInt(-1);
    box.className = cand.value("className").toString();
    box.cx = static_cast<float>(cand.value("cx").toDouble());
    box.cy = static_cast<float>(cand.value("cy").toDouble());
    box.w = static_cast<float>(cand.value("w").toDouble());
    box.h = static_cast<float>(cand.value("h").toDouble());
    box.confidence = static_cast<float>(cand.value("confidence").toDouble());
    box.sourceType = QStringLiteral("assisted");
    box.isConfirmed = true;
    return box;
}

QString AssistedLabelService::serializeBoxesJson(const QVector<AxisAlignedBox> &boxes)
{
    // 字段与 AnnotationService::createRevision 保持一致，便于统一审计/回放
    QJsonArray arr;
    for (const AxisAlignedBox &box : boxes) {
        QJsonObject obj;
        obj[QStringLiteral("id")]          = box.id;
        obj[QStringLiteral("classIndex")]  = box.classIndex;
        obj[QStringLiteral("className")]   = box.className;
        obj[QStringLiteral("cx")]          = box.cx;
        obj[QStringLiteral("cy")]          = box.cy;
        obj[QStringLiteral("w")]           = box.w;
        obj[QStringLiteral("h")]           = box.h;
        obj[QStringLiteral("angle")]       = 0.0;
        obj[QStringLiteral("confidence")]  = box.confidence;
        obj[QStringLiteral("sourceType")]  = box.sourceType;
        obj[QStringLiteral("isConfirmed")] = box.isConfirmed;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString AssistedLabelService::createAssistedRevision(const QString &datasetId,
                                                     const QString &sampleId,
                                                     const QString &beforeJson,
                                                     const QString &afterJson)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return {};

    QString revisionId = Id::generate();
    QSqlQuery query(db);
    query.prepare("INSERT INTO annotation_revisions "
                  "(id, dataset_id, sample_id, source_type, before_snapshot_json, after_snapshot_json) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(revisionId);
    query.addBindValue(datasetId);
    query.addBindValue(sampleId);
    query.addBindValue(QStringLiteral("assisted_confirm"));
    query.addBindValue(beforeJson.isEmpty() ? QVariant() : beforeJson);
    query.addBindValue(afterJson);

    if (!query.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to create assisted revision:" << query.lastError().text();
        return {};
    }
    return revisionId;
}

QVariantMap AssistedLabelService::commitConfirmedLabels(const QString &batchId)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    QVariantMap result;
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("batchId")] = batchId;
    result[QStringLiteral("samplesWritten")] = 0;
    result[QStringLiteral("revisionsCreated")] = 0;
    result[QStringLiteral("skipped")] = 0;
    result[QStringLiteral("errors")] = QVariantList();
    result[QStringLiteral("sampleResults")] = QVariantList();

    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) {
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_BATCH_NOT_FOUND")},
            {QStringLiteral("message"), QStringLiteral("辅助标注批次不存在或快照损坏")}
        };
        return result;
    }

    QString datasetId = batchDatasetId(batchId);
    if (datasetId.isEmpty()) {
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_DATASET_NOT_FOUND")},
            {QStringLiteral("message"), QStringLiteral("批次未关联数据集")}
        };
        return result;
    }

    auto db = Database::instance().database();
    if (!db.isOpen()) {
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_DB_ERROR")},
            {QStringLiteral("message"), QStringLiteral("数据库未打开")}
        };
        return result;
    }

    // 按 sampleId 分组收集确认框（confirmed / edited 均视为用户已确认）
    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    QMap<QString, QVector<AxisAlignedBox>> boxesBySample;
    QVariantList errors = result[QStringLiteral("errors")].toList();
    int skipped = 0;

    for (const auto &candVal : candidates) {
        QJsonObject cand = candVal.toObject();
        QString state = cand.value("state").toString("pending");
        if (state != "confirmed" && state != "edited") continue;

        QString sampleId = cand.value("sampleId").toString();
        if (sampleId.isEmpty()) {
            ++skipped;
            errors.append(QVariantMap{
                {QStringLiteral("sampleId"), QString()},
                {QStringLiteral("message"), QStringLiteral("候选框缺少 sampleId，无法定位标签文件")}
            });
            continue;
        }

        AxisAlignedBox box = boxFromCandidate(cand);
        if (!box.isValid() || box.classIndex < 0) {
            ++skipped;
            errors.append(QVariantMap{
                {QStringLiteral("sampleId"), sampleId},
                {QStringLiteral("message"), QStringLiteral("候选框几何非法或缺少类别索引")}
            });
            continue;
        }
        boxesBySample[sampleId].append(box);
    }

    QVariantList sampleResults;
    int samplesWritten = 0;
    int revisionsCreated = 0;

    for (auto it = boxesBySample.constBegin(); it != boxesBySample.constEnd(); ++it) {
        const QString &sampleId = it.key();
        const QVector<AxisAlignedBox> &newBoxes = it.value();

        // 解析样本标签路径
        QSqlQuery sampleQuery(db);
        sampleQuery.prepare("SELECT label_path FROM dataset_samples WHERE id = ? AND dataset_id = ?");
        sampleQuery.addBindValue(sampleId);
        sampleQuery.addBindValue(datasetId);
        if (!sampleQuery.exec() || !sampleQuery.next()) {
            errors.append(QVariantMap{
                {QStringLiteral("sampleId"), sampleId},
                {QStringLiteral("message"), QStringLiteral("样本不存在或不属于该数据集")}
            });
            continue;
        }
        QString labelPath = sampleQuery.value(0).toString();
        if (labelPath.isEmpty()) {
            errors.append(QVariantMap{
                {QStringLiteral("sampleId"), sampleId},
                {QStringLiteral("message"), QStringLiteral("样本缺少标签文件路径")}
            });
            continue;
        }

        // 读取既有标注作为 before 快照
        QVector<AxisAlignedBox> beforeBoxes = YoloTxtReader::read(labelPath);
        QString beforeJson = serializeBoxesJson(beforeBoxes);

        // 合并策略：保留既有标注，追加确认框；与既有框高 IoU 同类的确认框视为重复，跳过
        QVector<AxisAlignedBox> afterBoxes = beforeBoxes;
        int appended = 0;
        for (const AxisAlignedBox &candBox : newBoxes) {
            bool duplicate = false;
            for (const AxisAlignedBox &exist : beforeBoxes) {
                if (exist.classIndex == candBox.classIndex && exist.iou(candBox) >= 0.5f) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                AxisAlignedBox writeBox = candBox;
                writeBox.id = Id::generate();
                afterBoxes.append(writeBox);
                ++appended;
            }
        }

        if (!YoloTxtWriter::write(labelPath, afterBoxes)) {
            errors.append(QVariantMap{
                {QStringLiteral("sampleId"), sampleId},
                {QStringLiteral("message"), QStringLiteral("标签文件写入失败：%1").arg(labelPath)}
            });
            continue;
        }

        QString afterJson = serializeBoxesJson(afterBoxes);
        QString revisionId = createAssistedRevision(datasetId, sampleId, beforeJson, afterJson);
        if (!revisionId.isEmpty()) ++revisionsCreated;

        QVariantMap sampleResult;
        sampleResult[QStringLiteral("sampleId")] = sampleId;
        sampleResult[QStringLiteral("labelPath")] = labelPath;
        sampleResult[QStringLiteral("boxesWritten")] = appended;
        sampleResult[QStringLiteral("boxesTotal")] = afterBoxes.size();
        sampleResult[QStringLiteral("revisionId")] = revisionId;
        sampleResults.append(sampleResult);
        ++samplesWritten;

        ltInfo(LT_LOG_INFERENCE()) << "Committed assisted labels for sample:" << sampleId
                                   << "appended:" << appended << "total:" << afterBoxes.size();
    }

    result[QStringLiteral("samplesWritten")] = samplesWritten;
    result[QStringLiteral("revisionsCreated")] = revisionsCreated;
    result[QStringLiteral("skipped")] = skipped;
    result[QStringLiteral("errors")] = errors;
    result[QStringLiteral("sampleResults")] = sampleResults;
    // 至少成功写回一个样本即视为成功；空批次（无确认框）也视为成功
    result[QStringLiteral("success")] = (samplesWritten > 0) || boxesBySample.isEmpty();

    // 回写完成后在快照中记录提交信息，便于审计
    snapshotObj[QStringLiteral("committedAt")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    snapshotObj[QStringLiteral("committedSamples")] = samplesWritten;
    writeSnapshot(batchId, snapshotObj);

    ltInfo(LT_LOG_INFERENCE()) << "commitConfirmedLabels finished for batch:" << batchId
                               << "samplesWritten:" << samplesWritten
                               << "revisions:" << revisionsCreated
                               << "skipped:" << skipped;
    return result;
}

QVariantMap AssistedLabelService::retrainFromBatch(const QString &batchId,
                                                   double trainRatio,
                                                   const QString &splitStrategy,
                                                   const QString &trainConfigJson)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId
                                << "trainRatio=" << trainRatio
                                << "splitStrategy=" << splitStrategy;

    QVariantMap result;
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("batchId")] = batchId;

    // 第一步：回写已确认标签
    QVariantMap commitResult = commitConfirmedLabels(batchId);
    result[QStringLiteral("commit")] = commitResult;
    if (!commitResult.value("success").toBool()) {
        result[QStringLiteral("status")] = QStringLiteral("commit_failed");
        result[QStringLiteral("error")] = commitResult.contains("error")
            ? commitResult.value("error")
            : QVariantMap{{QStringLiteral("code"), QStringLiteral("E_COMMIT_FAILED")},
                          {QStringLiteral("message"), QStringLiteral("标签回写失败")}};
        return result;
    }

    QString datasetId = batchDatasetId(batchId);
    if (datasetId.isEmpty()) {
        result[QStringLiteral("status")] = QStringLiteral("commit_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_DATASET_NOT_FOUND")},
            {QStringLiteral("message"), QStringLiteral("批次未关联数据集")}
        };
        return result;
    }
    result[QStringLiteral("datasetId")] = datasetId;

    // 查询项目 ID（训练任务需要）
    auto db = Database::instance().database();
    QSqlQuery dsQuery(db);
    dsQuery.prepare("SELECT project_id FROM datasets WHERE id = ?");
    dsQuery.addBindValue(datasetId);
    if (!dsQuery.exec() || !dsQuery.next()) {
        result[QStringLiteral("status")] = QStringLiteral("commit_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_DATASET_NOT_FOUND")},
            {QStringLiteral("message"), QStringLiteral("数据集记录不存在")}
        };
        return result;
    }
    QString projectId = dsQuery.value(0).toString();
    result[QStringLiteral("projectId")] = projectId;

    if (!m_snapshotService || !m_trainingService) {
        result[QStringLiteral("status")] = QStringLiteral("snapshot_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_SERVICE_NOT_READY")},
            {QStringLiteral("message"), QStringLiteral("快照/训练服务未注入，无法启动增量训练")}
        };
        return result;
    }

    // 第二步：创建不可变数据快照
    QString snapshotId = m_snapshotService->createSnapshot(datasetId, trainRatio, splitStrategy);
    if (snapshotId.isEmpty()) {
        result[QStringLiteral("status")] = QStringLiteral("snapshot_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_SNAPSHOT_FAILED")},
            {QStringLiteral("message"), m_snapshotService->lastError().isEmpty()
                ? QStringLiteral("创建数据快照失败")
                : m_snapshotService->lastError()}
        };
        return result;
    }
    result[QStringLiteral("snapshotId")] = snapshotId;

    // 第三步：创建训练运行
    QString runId = m_trainingService->createRun(projectId, snapshotId, trainConfigJson);
    if (runId.isEmpty()) {
        result[QStringLiteral("status")] = QStringLiteral("run_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_RUN_CREATE_FAILED")},
            {QStringLiteral("message"), QStringLiteral("创建训练任务失败（请检查训练配置 JSON）")}
        };
        return result;
    }
    result[QStringLiteral("runId")] = runId;

    // 第四步：启动训练（内部异步准备快照物理目录并派发 train.start）
    if (!m_trainingService->startTraining(runId)) {
        result[QStringLiteral("status")] = QStringLiteral("run_failed");
        result[QStringLiteral("error")] = QVariantMap{
            {QStringLiteral("code"), QStringLiteral("E_RUN_START_FAILED")},
            {QStringLiteral("message"), QStringLiteral("启动训练失败（任务须处于 draft 状态且后端已连接）")}
        };
        return result;
    }

    result[QStringLiteral("success")] = true;
    result[QStringLiteral("status")] = QStringLiteral("retrain_started");
    ltInfo(LT_LOG_INFERENCE()) << "Retrain from batch started:" << batchId
                               << "snapshot:" << snapshotId << "run:" << runId;
    return result;
}

bool AssistedLabelService::readSnapshot(const QString &batchId, QJsonObject &snapshotObj)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("SELECT candidate_snapshot_json FROM assisted_label_batches WHERE id = ?");
    query.addBindValue(batchId);

    if (!query.exec() || !query.next()) {
        ltWarning(LT_LOG_INFERENCE()) << "Batch not found:" << batchId;
        return false;
    }

    QString snapshotJsonStr = query.value(0).toString();
    if (!snapshotJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(snapshotJsonStr.toUtf8());
        if (doc.isObject()) {
            snapshotObj = doc.object();
            return true;
        }
    }

    // Return minimal valid structure if empty
    snapshotObj = QJsonObject();
    snapshotObj["status"] = "pending";
    snapshotObj["candidates"] = QJsonArray();
    return true;
}

bool AssistedLabelService::writeSnapshot(const QString &batchId, const QJsonObject &snapshotObj)
{
    ltTrace(LT_LOG_INFERENCE()) << "batchId=" << batchId;

    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QString updatedJson = QString::fromUtf8(
        QJsonDocument(snapshotObj).toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    query.addBindValue(updatedJson);
    query.addBindValue(batchId);

    if (!query.exec()) {
        ltError(LT_LOG_INFERENCE()) << "Failed to update snapshot:" << query.lastError().text();
        return false;
    }

    return true;
}
