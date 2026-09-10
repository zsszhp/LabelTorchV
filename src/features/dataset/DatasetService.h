#ifndef DATASETSERVICE_H
#define DATASETSERVICE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QtConcurrent>

class ImportScanner;
class IpcClient;

class DatasetService : public QObject
{
    Q_OBJECT

public:
    explicit DatasetService(QObject *parent = nullptr);

    /**
     * @brief 注入 IPC 客户端依赖（用于 dataset.stats / dataset.convert_to_yolo 等命令）。
     * @param client IpcClient 实例。
     */
    void setIpcClient(IpcClient *client);

    /**
     * @brief Import a dataset from YOLO txt format directories.
     *
     * Creates a dataset record, runs scan, inserts matched samples,
     * and extracts class schema from all label files.
     * Import status transitions: idle -> scanning -> importing -> completed/failed
     *
     * @param projectId The project this dataset belongs to.
     * @param name Human-readable dataset name.
     * @param imageDir Directory containing image files.
     * @param labelDir Directory containing YOLO txt label files.
     * @return Dataset ID on success, empty string on failure.
     */
    Q_INVOKABLE QString importDataset(const QString &projectId, const QString &name,
                                      const QString &imageDir, const QString &labelDir);

    /**
     * @brief Import a dataset from COCO JSON format.
     *
     * Reads a COCO JSON label file, matches images by file_name,
     * converts COCO bbox to YOLO format, and stores samples.
     *
     * @param projectId The project this dataset belongs to.
     * @param name Human-readable dataset name.
     * @param imageDir Directory containing image files.
     * @param jsonLabelPath Path to the COCO JSON label file.
     * @return Dataset ID on success, empty string on failure.
     */
    Q_INVOKABLE QString importDatasetJson(const QString &projectId, const QString &name,
                                           const QString &imageDir, const QString &jsonLabelPath);

    /**
     * @brief 一键导入数据集（V2 自动探测版本）
     *
     * 统一的一键式导入入口，代替原有的 importDataset 和 importDatasetJson 的显式区分。
     * 根据智能探测到的格式标识自动选择导入流程。
     *
     * @param projectId 当前项目 ID
     * @param datasetName 数据集名称（默认填充为文件夹名）
     * @param folderPath 选择的文件夹路径
     * @param detectedFormat 智能探测到的格式标识 ("yolo_txt" | "coco_json" | "labelme_json" | "anomaly_unsupervised")
     * @param labelDirOrPath 标签目录路径或 JSON 文件路径（由 scanFolder 探测结果提供）
     * @param autoMergeClasses 是否自动将探测到的新类别合并入当前项目的分类体系
     * @return 导入成功返回 datasetId (UUID)，失败返回空字符串
     */
    Q_INVOKABLE QString importDatasetV2(const QString &projectId,
                                        const QString &datasetName,
                                        const QString &folderPath,
                                        const QString &detectedFormat,
                                        const QString &labelDirOrPath = QString(),
                                        bool autoMergeClasses = true);

    /**
     * @brief 分别指定图片和标签路径导入数据集
     *
     * 支持图片和标签在不同目录的场景，按文件名 stem 自动匹配。
     * 如果只提供图片路径不提供标签路径，则导入为无标签数据集。
     *
     * @param projectId 当前项目 ID
     * @param datasetName 数据集名称
     * @param imageDir 图片目录路径
     * @param labelDir 标签目录路径（可为空，表示无标签导入）
     * @return 导入成功返回 datasetId (UUID)，失败返回空字符串
     */
    Q_INVOKABLE QString importDatasetSeparate(const QString &projectId,
                                               const QString &datasetName,
                                               const QString &imageDir,
                                               const QString &labelDir);

    /**
     * @brief List all datasets for a project.
     * @param projectId The project to list datasets for.
     * @return QVariantList of QVariantMap entries with dataset fields.
     */
    Q_INVOKABLE QVariantList listDatasets(const QString &projectId);

    /**
     * @brief Get details of a specific dataset.
     * @param datasetId The dataset ID.
     * @return QVariantMap with dataset fields, or empty on not found.
     */
    Q_INVOKABLE QVariantMap getDataset(const QString &datasetId);

    /**
     * @brief Delete a dataset and all its samples.
     * @param datasetId The dataset ID.
     * @return true on success, false on failure.
     */
    Q_INVOKABLE bool deleteDataset(const QString &datasetId);

    /**
     * @brief Get sample statistics for a dataset.
     * Returns QVariantMap with:
     * - "totalSamples": total count
     * - "validSamples": count with validation_status='valid'
     * - "invalidSamples": count with validation_status != 'valid'
     * - "labeledSamples": count with non-null label_path
     * - "unlabeledSamples": count with null label_path
     * - "classDistribution": QVariantMap of class_id -> count
     * - "annotationDensity": QVariantMap with min/max/avg/median annotations per sample
     */
    Q_INVOKABLE QVariantMap getSampleStats(const QString &datasetId);

    /**
     * @brief 异步获取样本统计信息
     *
     * 在后台线程执行 getSampleStats，通过 sampleStatsReady 信号回传结果。
     * 避免在 UI 主线程执行耗时的标签文件扫描操作（M14）。
     * @param datasetId 数据集 ID
     */
    Q_INVOKABLE void getSampleStatsAsync(const QString &datasetId);

    /**
     * @brief Detect anomalies in the dataset.
     * Returns QVariantMap with:
     * - "emptyLabels": QVariantList of sample IDs with empty label files
     * - "classErrors": QVariantList of sample IDs with class_id outside valid range
     * - "sizeAnomalies": QVariantList of sample IDs with unusual image dimensions (IQR 离群值检测)
     * - "duplicateImages": QVariantList of sample IDs with duplicate hash values
     * - "totalAnomalies": total count of all anomaly items
     */
    Q_INVOKABLE QVariantMap detectAnomalies(const QString &datasetId);

    /**
     * @brief 异步检测数据集异常
     *
     * 在后台线程执行 detectAnomalies，通过 anomaliesDetected 信号回传结果。
     * 避免在 UI 主线程执行耗时的标签文件扫描操作（M14）。
     * @param datasetId 数据集 ID
     */
    Q_INVOKABLE void detectAnomaliesAsync(const QString &datasetId);

    /**
     * @brief Get class distribution for a dataset.
     * Returns QVariantList of QVariantMap entries, each with "classId" and "count".
     * Ordered by count descending.
     */
    Q_INVOKABLE QVariantList getClassDistribution(const QString &datasetId);

    /**
     * @brief 异步获取类别分布
     *
     * 在后台线程执行 getClassDistribution，通过 classDistributionReady 信号回传结果。
     * 避免在 UI 主线程执行耗时的标签文件扫描操作（M14）。
     * @param datasetId 数据集 ID
     */
    Q_INVOKABLE void getClassDistributionAsync(const QString &datasetId);

    Q_INVOKABLE QVariantList listSamples(const QString &datasetId, int offset = 0, int limit = 100);
    Q_INVOKABLE int getSampleCount(const QString &datasetId);

    Q_INVOKABLE QString appendImport(const QString &datasetId, const QString &imageDir, const QString &labelDir);
    Q_INVOKABLE bool resplitDataset(const QString &datasetId, double valRatio = 0.2, int seed = 42);
    Q_INVOKABLE bool updateClassName(const QString &taxonomyId, int classId, const QString &name);

    /**
     * @brief 扫描文件夹并自动探测数据集格式（代理 ImportScanner::scanFolder）
     * @param folderPath 用户选中的文件夹绝对路径
     * @return QVariantMap 探测结果
     */
    Q_INVOKABLE QVariantMap scanFolder(const QString &folderPath);

    /**
     * @brief 分别指定图片和标签路径进行扫描匹配
     * @param imageDir 图片目录路径
     * @param labelDir 标签目录路径
     * @return QVariantMap 扫描结果，包含 detectedFormat/imageCount/labelCount 等
     */
    Q_INVOKABLE QVariantMap scanSeparate(const QString &imageDir, const QString &labelDir);

    /**
     * @brief 异步扫描文件夹并自动探测数据集格式
     * 使用 QtConcurrent::run 在后台线程执行扫描，通过信号通知结果
     * @param folderPath 用户选中的文件夹绝对路径
     */
    Q_INVOKABLE void scanFolderAsync(const QString &folderPath);

    /**
     * @brief 异步分别指定图片和标签路径进行扫描匹配
     * 使用 QtConcurrent::run 在后台线程执行扫描，通过信号通知结果
     * @param imageDir 图片目录路径
     * @param labelDir 标签目录路径
     */
    Q_INVOKABLE void scanSeparateAsync(const QString &imageDir, const QString &labelDir);

    /**
     * @brief 异步获取数据集增强统计（P1-4，supervision 集成）。
     *
     * 通过 IPC 调用 dataset.stats，使用 sv.DetectionDataset.from_yolo 加载数据集，
     * 计算 class_distribution / box_size_stats / annotated_samples /
     * unlabeled_samples / avg_boxes_per_sample 等深度统计。
     * 完成后发射 enhancedStatsReady 信号。
     *
     * @param datasetId 数据集 ID。
     * @return 请求 ID（非空表示已成功派发），空串表示失败。
     */
    Q_INVOKABLE QString getEnhancedStats(const QString &datasetId);

    /**
     * @brief 异步将数据集转换为 YOLO 格式（P2-5，supervision 集成）。
     *
     * 通过 IPC 调用 dataset.convert_to_yolo，支持 coco / pascal_voc / yolo 三种源格式，
     * 使用 sv.DetectionDataset.from_coco / from_pascal_voc / from_yolo 加载，
     * 通过 dataset.as_yolo 输出到 {projectRoot}/cache/yolo_converted/{datasetId}/。
     *
     * @param datasetId 数据集 ID。
     * @param sourceFormat 源格式：coco / pascal_voc / yolo。
     * @return 请求 ID（非空表示已成功派发），空串表示失败。
     */
    Q_INVOKABLE QString convertToYolo(const QString &datasetId, const QString &sourceFormat);

signals:
    /** @brief scanFolderAsync 扫描完成信号 */
    void scanFolderFinished(const QVariantMap &result);

    /** @brief scanSeparateAsync 扫描完成信号 */
    void scanSeparateFinished(const QVariantMap &result);

    /** @brief getSampleStatsAsync 统计完成信号 */
    void sampleStatsReady(const QString &datasetId, const QVariantMap &stats);

    /** @brief detectAnomaliesAsync 异常检测完成信号 */
    void anomaliesDetected(const QString &datasetId, const QVariantMap &anomalies);

    /** @brief getClassDistributionAsync 类别分布完成信号 */
    void classDistributionReady(const QString &datasetId, const QVariantList &distribution);

    /** @brief P1-4 增强统计完成信号 */
    void enhancedStatsReady(const QString &datasetId, bool success,
                            const QVariantMap &stats, const QString &error);

    /** @brief P2-5 格式转换完成信号 */
    void convertToYoloFinished(const QString &datasetId, bool success,
                               const QString &outputDir, const QString &error);

private slots:
    /// 处理 IPC 响应（dataset.stats / dataset.convert_to_yolo）
    void onIpcResponseReceived(const QJsonObject &response);

private:
    bool updateImportStatus(const QString &datasetId, const QString &status);
    bool insertSamples(const QString &datasetId, const QVariantList &samples);
    bool extractAndStoreSchema(const QString &datasetId, const QVariantList &samples);
    bool extractAndStoreSchemaFromCategories(const QString &datasetId, const QVariantMap &categories);
    bool importAnomalyDataset(const QString &datasetId, const QString &folderPath);

    /**
     * @brief 将导入的类别名同步到项目的 taxonomy
     *
     * 从 imported_label_schemas 读取数据集的类别名，
     * 合并到项目 taxonomy 的 class_definitions_json 中。
     * 新类别追加到末尾，已有类别不重复添加。
     *
     * @param datasetId 数据集 ID
     * @return true 同步成功，false 失败
     */
    bool syncClassesToTaxonomy(const QString &datasetId);

    /**
     * @brief 导入 LabelMe JSON 格式数据集
     *
     * LabelMe 格式：每张图片对应一个 JSON 标签文件，包含 shapes 数组
     * 导入时将 LabelMe 坐标转换为 YOLO 归一化格式并生成 txt 标签文件
     *
     * @param datasetId 已创建的数据集 ID
     * @param imageDir 图片目录
     * @param labelDir LabelMe JSON 标签目录
     * @return true 导入成功
     */
    bool importLabelMeDataset(const QString &datasetId, const QString &imageDir, const QString &labelDir);

    /**
     * @brief 导入 ImageNet 风格分类数据集
     *
     * 分类数据集目录结构：每个子文件夹为一个类别，文件夹内的图片为该类别样本。
     * 样本的 label_path 字段存储类别名（而非标签文件路径）。
     *
     * @param datasetId 已创建的数据集 ID
     * @param folderPath 数据集根目录（包含类别子文件夹）
     * @return true 导入成功
     */
    bool importClassifyFolderDataset(const QString &datasetId, const QString &folderPath);

    ImportScanner *m_scanner;
    /// 当前导入流程的数据集格式标识，供 extractAndStoreSchemaFromCategories 写入正确的 source_format
    QString m_currentImportFormat;

    IpcClient *m_ipcClient = nullptr;
    /// 待响应增强统计请求映射：requestId → datasetId
    QMap<QString, QString> m_pendingEnhancedStats;
    /// 待响应格式转换请求映射：requestId → datasetId
    QMap<QString, QString> m_pendingConverts;
};

#endif // DATASETSERVICE_H
