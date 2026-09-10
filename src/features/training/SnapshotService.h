#ifndef SNAPSHOTSERVICE_H
#define SNAPSHOTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>

class IpcClient;

class SnapshotService : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotService(QObject *parent = nullptr);

    /**
     * @brief 注入 IPC 客户端依赖（用于调用 snapshot.preview 等命令）。
     * @param client IpcClient 实例。
     */
    void setIpcClient(IpcClient *client);

    /**
     * @brief Create an immutable snapshot of a dataset.
     *
     * Freezes the current sample list, taxonomy version, annotation revision
     * boundary, and generates train/val split.
     *
     * @param datasetId The dataset to snapshot.
     * @param trainRatio Train split ratio (0.0-1.0, default 0.8).
     * @param splitStrategy "sequential" or "random" (default "random").
     * @return Snapshot ID on success, empty string on failure.
     */
    Q_INVOKABLE QString createSnapshot(const QString &datasetId,
                                        double trainRatio = 0.8,
                                        const QString &splitStrategy = "random");

    /**
     * @brief List all snapshots for a dataset.
     * @param datasetId The dataset ID.
     * @return QVariantList of QVariantMap with snapshot fields.
     */
    Q_INVOKABLE QVariantList listSnapshots(const QString &datasetId);

    /**
     * @brief Get details of a specific snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantMap with snapshot fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getSnapshot(const QString &snapshotId);

    /**
     * @brief Delete a snapshot. Only allowed if no training runs reference it.
     * @param snapshotId The snapshot ID.
     * @return true on success, false on failure (e.g. in use).
     */
    Q_INVOKABLE bool deleteSnapshot(const QString &snapshotId);

    /**
     * @brief Get the sample manifest for a snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantList of sample IDs in the manifest.
     */
    Q_INVOKABLE QVariantList getSampleManifest(const QString &snapshotId);

    /**
     * @brief Get the split manifest for a snapshot.
     * @param snapshotId The snapshot ID.
     * @return QVariantMap with "train" and "val" keys, each containing QVariantList of sample IDs.
     */
    Q_INVOKABLE QVariantMap getSplitManifest(const QString &snapshotId);

    /**
     * @brief Check if a snapshot is immutable (always true after creation).
     * @param snapshotId The snapshot ID.
     * @return true if snapshot exists and is immutable.
     */
    Q_INVOKABLE bool isImmutable(const QString &snapshotId);

    /**
     * @brief Detect if a dataset uses OBB format labels.
     *
     * Checks the first few label files for 9-column format (OBB)
     * vs 5-column format (HBB). Reads the first non-empty line of each
     * file and counts whitespace-separated tokens.
     *
     * @param datasetId The dataset ID.
     * @return true if the dataset uses OBB format, false if HBB or undetermined.
     */
    Q_INVOKABLE bool isOBBDataset(const QString &datasetId);

    /**
     * @brief Prepare a physical directory for a snapshot, copy files and generate data.yaml.
     * @param snapshotId The snapshot ID.
     * @return The path to data.yaml on success, or empty string on failure.
     */
    Q_INVOKABLE QString prepareSnapshotPhysicalDir(const QString &snapshotId);

    /**
     * @brief 生成数据快照的预览网格图（P0-2）。
     *
     * 异步调用 Python 后端 snapshot.preview 命令，使用 supervision 库
     * 加载快照数据集，对采样图片渲染 GT 框并合成网格图，输出到
     * {snapshotDir}/preview.jpg。完成后发射 previewGenerated 信号。
     *
     * @param snapshotId 快照 ID。
     * @return 请求 ID（非空表示已成功派发），空串表示失败（IPC 未连接或快照不存在）。
     */
    Q_INVOKABLE QString generatePreview(const QString &snapshotId);

    /**
     * @brief 查询快照预览图路径（同步、纯本地）。
     *
     * 不发起 IPC 请求，仅返回快照目录下已存在的 preview.jpg 路径；
     * 若文件不存在则返回空串。QML 可在 previewGenerated 信号触发后调用此方法刷新显示。
     *
     * @param snapshotId 快照 ID。
     * @return preview.jpg 绝对路径，或空串。
     */
    Q_INVOKABLE QString getPreviewPath(const QString &snapshotId) const;

signals:
    /**
     * @brief 预览图生成完成信号。
     * @param snapshotId 快照 ID。
     * @param previewPath 预览图路径（成功时为绝对路径，失败时为空串）。
     * @param success 是否成功。
     * @param error 错误信息（失败时）。
     */
    void previewGenerated(const QString &snapshotId, const QString &previewPath,
                          bool success, const QString &error);

private slots:
    /// 处理 IPC 响应（snapshot.preview 结果回传）
    void onPreviewResponseReceived(const QJsonObject &response);

private:
    IpcClient *m_ipcClient = nullptr;
    /// 待响应预览请求映射：requestId → snapshotId
    QMap<QString, QString> m_pendingPreviews;
};

#endif // SNAPSHOTSERVICE_H
