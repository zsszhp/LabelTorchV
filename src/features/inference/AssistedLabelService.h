#ifndef ASSISTEDLABELSERVICE_H
#define ASSISTEDLABELSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QVector>

class SnapshotService;
class TrainingService;
struct AxisAlignedBox;

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

    /**
     * @brief Get false positive candidates (rejected by user review).
     *
     * Returns candidates from the batch whose state is "rejected".
     * Each map contains: candidateIndex, className, classIndex, confidence,
     * cx, cy, w, h, state, sampleId (if present).
     *
     * @param batchId The batch ID.
     * @return QVariantList of rejected candidate maps.
     */
    Q_INVOKABLE QVariantList getFalsePositives(const QString &batchId);

    /**
     * @brief Get samples with no detections (potential false negatives).
     *
     * Samples that exist in the dataset but had no inference candidates.
     * Compares all sample IDs in dataset_samples for the given dataset
     * against sample IDs referenced in the batch's candidate snapshot.
     * If candidates contain a "sampleId" field or the snapshot has a
     * "processedSamples" array, those sample IDs are subtracted.
     *
     * @param batchId The batch ID.
     * @param datasetId The dataset ID to check for missed samples.
     * @return QVariantList of sample ID strings.
     */
    Q_INVOKABLE QVariantList getFalseNegatives(const QString &batchId, const QString &datasetId);

    /**
     * @brief Get the hard-case queue sorted by priority.
     *
     * Combines low-confidence, false-positive, and false-negative samples.
     * Returns QVariantList of QVariantMap with: sampleId, reason, priority, confidence.
     * Priority: false_negative (3, highest) > low_confidence (2) > false_positive (1, lowest).
     *
     * @param batchId The batch ID.
     * @param datasetId The dataset ID for false-negative detection.
     * @param lowConfThreshold Confidence threshold for low-confidence samples (default 0.3).
     * @return QVariantList of hard-case maps sorted by priority (highest first).
     */
    Q_INVOKABLE QVariantList getHardCaseQueue(const QString &batchId, const QString &datasetId, float lowConfThreshold = 0.3f);

    /**
     * @brief 将批次中已确认（confirmed/edited）的候选框回写 YOLO txt 标签。
     *
     * 按 sampleId 分组，合并已有标注与本批确认框后原子写入标签文件，
     * 并为每个受影响样本写入 annotation_revisions（source_type=assisted_confirm）。
     *
     * @param batchId 辅助标注批次 ID。
     * @return 结构化结果：
     *         success(bool), batchId, samplesWritten(int), revisionsCreated(int),
     *         skipped(int), errors([{sampleId, message}]),
     *         sampleResults([{sampleId, labelPath, boxesWritten, revisionId}])
     */
    Q_INVOKABLE QVariantMap commitConfirmedLabels(const QString &batchId);

    /**
     * @brief 用本批确认数据启动增量训练（编排：回写标签 → 创建快照 → 创建并启动训练）。
     *
     * 快照创建与训练启动复用 SnapshotService / TrainingService，训练任务
     * 进入异步生命周期后由 TrainingService 继续跟踪。
     *
     * @param batchId 辅助标注批次 ID。
     * @param trainRatio 训练集划分比例（默认 0.8）。
     * @param splitStrategy 划分策略（默认 random）。
     * @param trainConfigJson 训练配置 JSON（可为空对象）。
     * @return 结构化结果：success, batchId, datasetId, projectId, snapshotId,
     *         runId, status(retrain_started|commit_failed|snapshot_failed|run_failed),
     *         commit(回写结果), error({code,message} 失败时)
     */
    Q_INVOKABLE QVariantMap retrainFromBatch(const QString &batchId,
                                             double trainRatio = 0.8,
                                             const QString &splitStrategy = QStringLiteral("random"),
                                             const QString &trainConfigJson = QStringLiteral("{}"));

    /// 注入快照服务依赖（用于增量训练编排）
    void setSnapshotService(SnapshotService *service);

    /// 注入训练服务依赖（用于增量训练编排）
    void setTrainingService(TrainingService *service);

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

    /**
     * @brief 查询批次关联的 dataset_id。
     * @param batchId 批次 ID。
     * @return 数据集 ID；失败返回空串。
     */
    QString batchDatasetId(const QString &batchId);

    /**
     * @brief 由候选框 JSON 生成 AxisAlignedBox。
     */
    static AxisAlignedBox boxFromCandidate(const QJsonObject &cand);

    /**
     * @brief 将候选框数组序列化为修订快照 JSON 数组（与 AnnotationService 字段一致）。
     */
    static QString serializeBoxesJson(const QVector<AxisAlignedBox> &boxes);

    /**
     * @brief 为单个样本写入标注修订记录。
     * @return 修订 ID；失败返回空串。
     */
    QString createAssistedRevision(const QString &datasetId,
                                   const QString &sampleId,
                                   const QString &beforeJson,
                                   const QString &afterJson);

    SnapshotService *m_snapshotService = nullptr;
    TrainingService *m_trainingService = nullptr;
};

#endif // ASSISTEDLABELSERVICE_H
