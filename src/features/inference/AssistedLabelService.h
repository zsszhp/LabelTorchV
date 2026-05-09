#ifndef ASSISTEDLABELSERVICE_H
#define ASSISTEDLABELSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/**
 * @brief Assisted labeling service for reviewing inference candidates.
 *
 * Manages candidate annotations from inference batches: confirm, reject,
 * batch operations by threshold, and statistics.
 *
 * Candidate states: pending, confirmed, rejected, edited.
 * Candidates are stored in the candidate_snapshot_json field of
 * assisted_label_batches as a JSON array of objects, each with a
 * "state" field.
 */
class AssistedLabelService : public QObject
{
    Q_OBJECT

public:
    explicit AssistedLabelService(QObject *parent = nullptr);

    /**
     * @brief Get candidate annotations from a batch.
     *
     * Parses candidate_snapshot_json and returns the candidates array.
     * Each candidate has: className, classIndex, cx, cy, w, h, confidence, state.
     *
     * @param batchId The batch ID.
     * @return QVariantList of candidate maps.
     */
    Q_INVOKABLE QVariantList getCandidates(const QString &batchId);

    /**
     * @brief Confirm a candidate by index. Marks state as "confirmed".
     * @param batchId The batch ID.
     * @param candidateIndex Index in the candidates array.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool confirmCandidate(const QString &batchId, int candidateIndex);

    /**
     * @brief Reject a candidate by index. Marks state as "rejected".
     * @param batchId The batch ID.
     * @param candidateIndex Index in the candidates array.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool rejectCandidate(const QString &batchId, int candidateIndex);

    /**
     * @brief Batch confirm all candidates above a confidence threshold.
     * @param batchId The batch ID.
     * @param threshold Confidence threshold (0-1).
     * @return Number of candidates confirmed, or -1 on failure.
     */
    Q_INVOKABLE int confirmAllAboveThreshold(const QString &batchId, double threshold);

    /**
     * @brief Batch reject all candidates below a confidence threshold.
     * @param batchId The batch ID.
     * @param threshold Confidence threshold (0-1).
     * @return Number of candidates rejected, or -1 on failure.
     */
    Q_INVOKABLE int rejectAllBelowThreshold(const QString &batchId, double threshold);

    /**
     * @brief Get batch statistics.
     * @param batchId The batch ID.
     * @return QVariantMap with keys: total, confirmed, rejected, pending, edited.
     */
    Q_INVOKABLE QVariantMap getBatchStats(const QString &batchId);

    /**
     * @brief Collect low-confidence samples from an inference batch.
     *
     * Returns a list of candidate maps where confidence < threshold.
     * Each map contains: candidateIndex, className, classIndex, confidence,
     * cx, cy, w, h, state.
     *
     * @param batchId The inference batch ID.
     * @param threshold Confidence threshold (default 0.3).
     * @return QVariantList of candidate maps with low confidence.
     */
    Q_INVOKABLE QVariantList getLowConfidenceSamples(const QString &batchId, float threshold = 0.3f);

    /**
     * @brief Get statistics about confidence distribution in a batch.
     *
     * @param batchId The batch ID.
     * @param threshold Confidence threshold (default 0.3).
     * @return QVariantMap with: totalCandidates, lowConfCount, highConfCount,
     *         averageConfidence, threshold.
     */
    Q_INVOKABLE QVariantMap getConfidenceStats(const QString &batchId, float threshold = 0.3f);

private:
    /**
     * @brief Read and parse the candidate_snapshot_json for a batch.
     * @param batchId The batch ID.
     * @param[out] snapshotObj The parsed JSON object.
     * @return true on success, false if batch not found or invalid JSON.
     */
    bool readSnapshot(const QString &batchId, QJsonObject &snapshotObj);

    /**
     * @brief Write the updated candidate_snapshot_json for a batch.
     * @param batchId The batch ID.
     * @param snapshotObj The JSON object to write.
     * @return true on success, false on failure.
     */
    bool writeSnapshot(const QString &batchId, const QJsonObject &snapshotObj);
};

#endif // ASSISTEDLABELSERVICE_H
