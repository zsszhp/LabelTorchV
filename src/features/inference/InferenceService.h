#ifndef INFERENCESERVICE_H
#define INFERENCESERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QSet>

class IpcClient;

/**
 * @brief Inference batch management service.
 *
 * Manages inference runs: creates assisted_label_batches records,
 * dispatches inference.run via IpcClient, tracks batch status,
 * and supports cancellation.
 */
class InferenceService : public QObject
{
    Q_OBJECT

public:
    explicit InferenceService(QObject *parent = nullptr);

    /**
     * @brief Inject IPC client dependency.
     */
    void setIpcClient(IpcClient *client);

    /**
     * @brief Start an inference batch.
     *
     * Creates an assisted_label_batches record with status "pending",
     * then sends inference.run via IpcClient.
     *
     * @param modelVersionId The model version to use for inference.
     * @param datasetId The dataset to run inference on.
     * @param sampleScope Target scope: "all", "unlabeled", or "failed".
     * @param confThreshold Confidence threshold (0-1).
     * @param iouThreshold IoU threshold for NMS (0-1).
     * @return Batch ID on success, empty string on failure.
     */
    Q_INVOKABLE QString runInference(const QString &modelVersionId,
                                      const QString &datasetId,
                                      const QString &sampleScope,
                                      double confThreshold,
                                      double iouThreshold);

    /**
     * @brief Get the status/details of an inference batch.
     * @param batchId The batch ID.
     * @return QVariantMap with batch fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getBatchStatus(const QString &batchId);

    /**
     * @brief List inference batches for a dataset.
     * @param datasetId The dataset ID.
     * @return QVariantList of QVariantMap with batch fields.
     */
    Q_INVOKABLE QVariantList listBatches(const QString &datasetId);

    /**
     * @brief Cancel a running inference batch.
     *
     * Sends inference.cancel via IpcClient and updates the batch
     * candidate_snapshot_json status field to "cancelled".
     *
     * @param batchId The batch ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool cancelBatch(const QString &batchId);

    /**
     * @brief 启动视频流推理（P2-6，supervision 集成）。
     *
     * 异步调用 Python 后端 inference.run_video 命令，使用 sv.VideoInfo +
     * sv.VideoSink + sv.get_video_frames_generator 逐帧推理并渲染标注框，
     * 输出标注后视频到指定路径。完成后发射 videoInferenceFinished 信号。
     *
     * @param modelVersionId 模型版本 ID（用于解析权重文件路径）。
     * @param videoPath 输入视频文件路径。
     * @param outputPath 输出视频文件路径（空则自动派生到项目 cache 目录）。
     * @param confThreshold 置信度阈值（0-1）。
     * @param iouThreshold IoU 阈值（0-1）。
     * @return 请求 ID（非空表示已成功派发），空串表示失败。
     */
    Q_INVOKABLE QString runVideoInference(const QString &modelVersionId,
                                           const QString &videoPath,
                                           const QString &outputPath,
                                           double confThreshold,
                                           double iouThreshold);

signals:
    /**
     * @brief Emitted when an inference batch status changes.
     * @param batchId The batch ID.
     * @param status The new status.
     */
    void batchStatusChanged(const QString &batchId, const QString &status);

    /**
     * @brief Emitted when an inference batch completes (success or failure).
     * @param batchId The batch ID.
     * @param success Whether inference succeeded.
     * @param candidateCount Number of candidate boxes detected.
     */
    void batchCompleted(const QString &batchId, bool success, int candidateCount);

    /**
     * @brief P2-6 视频推理完成信号。
     * @param outputPath 输出视频路径（成功时为绝对路径，失败时为空串）。
     * @param success 是否成功。
     * @param totalFrames 总帧数。
     * @param totalDetections 总检测框数。
     * @param error 错误信息（失败时）。
     */
    void videoInferenceFinished(const QString &outputPath, bool success,
                                int totalFrames, int totalDetections,
                                const QString &error);

private slots:
    /// 处理 IPC 响应（A1：推理结果回传）
    void onResponseReceived(const QJsonObject &response);

private:
    IpcClient *m_ipcClient = nullptr;
    /// 待响应推理请求映射：requestId → batchId（用于关联 IPC 响应与批次）
    QMap<QString, QString> m_pendingBatches;
    /// 待响应视频推理请求集合（requestId 集合）
    QSet<QString> m_pendingVideoRequests;
};

#endif // INFERENCESERVICE_H
