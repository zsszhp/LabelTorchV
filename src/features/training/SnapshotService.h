#ifndef SNAPSHOTSERVICE_H
#define SNAPSHOTSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QJsonObject>

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
     * @brief Check if a snapshot is immutable.
     *
     * 快照不可变性判定：sample_manifest 存在且（冻结目录完整 或 每个样本条目均含 imageHash/labelHash 字段）。
     * 仅凭记录存在不足以证明不可变——源数据可能在创建后漂移。
     *
     * @param snapshotId The snapshot ID.
     * @return true if snapshot exists and its manifest is integrity-complete.
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
     *
     * 拷贝前逐样本校验当前文件 SHA256 与 manifest 中冻结哈希一致：
     * - 一致 → 正常拷贝，并写入冻结标记供后续复用；
     * - 不一致且 materializeFrozenCopy=false（默认）→ 拒绝并置 lastError=「快照源数据已变更」；
     * - 不一致且 materializeFrozenCopy=true → 以当前文件物化冻结副本（记录漂移警告）；
     * - 若冻结目录已存在 → 直接使用冻结副本，不再回源校验。
     *
     * @param snapshotId The snapshot ID.
     * @param materializeFrozenCopy 检测到漂移时是否物化冻结副本（默认 false=拒绝）。
     * @return The path to data.yaml on success, or empty string on failure (see lastError()).
     */
    Q_INVOKABLE QString prepareSnapshotPhysicalDir(const QString &snapshotId,
                                                   bool materializeFrozenCopy = false);

    /**
     * @brief 获取最近一次操作的错误消息（中文，可直接展示给用户）。
     * @return 错误消息；成功时返回空串。
     */
    Q_INVOKABLE QString lastError() const;

    /**
     * @brief 获取最近一次操作的错误码（供 UI 分支处理）。
     *
     * 已定义错误码：
     * - OK                    无错误
     * - E_SNAPSHOT_NOT_FOUND  快照不存在
     * - E_MANIFEST_INVALID    manifest 缺失或格式非法
     * - E_SOURCE_DRIFT        快照源数据已变更（哈希不一致）
     * - E_FILE_MISSING        源文件缺失
     * - E_COPY_FAILED         拷贝失败
     * - E_DB_ERROR            数据库错误
     *
     * @return 错误码字符串；成功时返回 "OK"。
     */
    Q_INVOKABLE QString lastErrorCode() const;

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
    /**
     * @brief 计算文件内容的 SHA256（分块读取，避免大文件占满内存）。
     * @param filePath 文件路径。
     * @return 十六进制哈希串；文件无法打开时返回空串。
     */
    static QString computeFileSha256(const QString &filePath);

    /**
     * @brief 设置错误码与错误消息（内部使用，统一出口）。
     */
    void setLastError(const QString &code, const QString &message);

    /**
     * @brief 清空错误状态（成功路径调用）。
     */
    void clearLastError();

    /**
     * @brief 从 manifest JSON 数组解析样本哈希清单。
     *
     * 兼容两种历史格式：
     * - 旧格式（纯 ID 字符串数组）：无哈希，返回空 map（调用方按"无哈希"降级处理）；
     * - 新格式（对象数组，含 id/imageHash/labelHash）：返回 id → {imageHash, labelHash}。
     *
     * @param manifestJson sample_manifest_json 原文。
     * @return 样本 ID → QJsonObject{imageHash, labelHash}；旧格式返回空 map。
     */
    static QMap<QString, QJsonObject> parseManifestHashes(const QString &manifestJson);

    /**
     * @brief 写入冻结标记文件（.frozen.json），记录冻结时间与哈希清单摘要。
     * @param snapshotDir 快照物理目录。
     * @param manifestJson 原始 manifest JSON（写入标记供离线校验）。
     * @return 是否写入成功。
     */
    static bool writeFreezeMarker(const QString &snapshotDir, const QString &manifestJson);

    /**
     * @brief 检查冻结标记是否已存在（表示该目录已是冻结副本）。
     */
    static bool isFrozenCopyPresent(const QString &snapshotDir);

    IpcClient *m_ipcClient = nullptr;
    /// 待响应预览请求映射：requestId → snapshotId
    QMap<QString, QString> m_pendingPreviews;
    /// 最近一次错误码（"OK" 表示无错误）
    QString m_lastErrorCode = QStringLiteral("OK");
    /// 最近一次错误消息
    QString m_lastError;
};

#endif // SNAPSHOTSERVICE_H
