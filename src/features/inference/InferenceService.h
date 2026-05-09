#ifndef INFERENCESERVICE_H
#define INFERENCESERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

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

signals:
    /**
     * @brief Emitted when an inference batch status changes.
     * @param batchId The batch ID.
     * @param status The new status.
     */
    void batchStatusChanged(const QString &batchId, const QString &status);

private:
    IpcClient *m_ipcClient = nullptr;
};

#endif // INFERENCESERVICE_H
