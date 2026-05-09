#include "AssistedLabelService.h"
#include "Database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSet>
#include <algorithm>

AssistedLabelService::AssistedLabelService(QObject *parent) : QObject(parent) {}

QVariantList AssistedLabelService::getCandidates(const QString &batchId)
{
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

    return result;
}

bool AssistedLabelService::confirmCandidate(const QString &batchId, int candidateIndex)
{
    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return false;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        qWarning() << "Candidate index out of range:" << candidateIndex;
        return false;
    }

    QJsonObject candidate = candidates[candidateIndex].toObject();
    candidate["state"] = "confirmed";
    candidates[candidateIndex] = candidate;

    snapshotObj["candidates"] = candidates;
    return writeSnapshot(batchId, snapshotObj);
}

bool AssistedLabelService::rejectCandidate(const QString &batchId, int candidateIndex)
{
    QJsonObject snapshotObj;
    if (!readSnapshot(batchId, snapshotObj)) return false;

    QJsonArray candidates = snapshotObj.value("candidates").toArray();
    if (candidateIndex < 0 || candidateIndex >= candidates.size()) {
        qWarning() << "Candidate index out of range:" << candidateIndex;
        return false;
    }

    QJsonObject candidate = candidates[candidateIndex].toObject();
    candidate["state"] = "rejected";
    candidates[candidateIndex] = candidate;

    snapshotObj["candidates"] = candidates;
    return writeSnapshot(batchId, snapshotObj);
}

int AssistedLabelService::confirmAllAboveThreshold(const QString &batchId, double threshold)
{
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

    return count;
}

int AssistedLabelService::rejectAllBelowThreshold(const QString &batchId, double threshold)
{
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

    return count;
}

QVariantMap AssistedLabelService::getBatchStats(const QString &batchId)
{
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
    QVariantList result;
    auto db = Database::instance().database();
    if (!db.isOpen()) return result;

    // Collect all sample IDs from dataset_samples for the given dataset
    QSet<QString> allSampleIds;
    QSqlQuery sampleQuery(db);
    sampleQuery.prepare("SELECT id FROM dataset_samples WHERE dataset_id = ?");
    sampleQuery.addBindValue(datasetId);
    if (!sampleQuery.exec()) {
        qWarning() << "Failed to query dataset_samples:" << sampleQuery.lastError().text();
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

    return result;
}

QVariantList AssistedLabelService::getHardCaseQueue(const QString &batchId, const QString &datasetId, float lowConfThreshold)
{
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

    return result;
}

bool AssistedLabelService::readSnapshot(const QString &batchId, QJsonObject &snapshotObj)
{
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare("SELECT candidate_snapshot_json FROM assisted_label_batches WHERE id = ?");
    query.addBindValue(batchId);

    if (!query.exec() || !query.next()) {
        qWarning() << "Batch not found:" << batchId;
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
    auto db = Database::instance().database();
    if (!db.isOpen()) return false;

    QString updatedJson = QString::fromUtf8(
        QJsonDocument(snapshotObj).toJson(QJsonDocument::Compact));

    QSqlQuery query(db);
    query.prepare("UPDATE assisted_label_batches SET candidate_snapshot_json = ? WHERE id = ?");
    query.addBindValue(updatedJson);
    query.addBindValue(batchId);

    if (!query.exec()) {
        qWarning() << "Failed to update snapshot:" << query.lastError().text();
        return false;
    }

    return true;
}
