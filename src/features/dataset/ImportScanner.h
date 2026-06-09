#ifndef IMPORTSCANNER_H
#define IMPORTSCANNER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QSet>
#include <QMap>
#include <QDir>
#include <QFileInfoList>

class ImportScanner : public QObject
{
    Q_OBJECT

public:
    explicit ImportScanner(QObject *parent = nullptr);

    /**
     * @brief 扫描指定图片和标签目录中的图像-标签对
     *
     * 自动检测标签格式：COCO JSON (.json) 优先于 YOLO txt (.txt)
     * 支持递归子目录扫描，标签目录可为空（无标签导入场景）
     *
     * @param imageDir 图片目录
     * @param labelDir 标签目录（可为空）
     * @return QVariantMap 包含 total, matched, unmatchedImages, unmatchedLabels,
     *         samples, categories 等键
     */
    Q_INVOKABLE QVariantMap scan(const QString &imageDir, const QString &labelDir);

    /**
     * @brief 扫描单个文件夹并自动识别格式与样本关系
     *
     * 自动探测三种主流数据集布局：
     * - 扁平结构（Flat Layout）：图片和标签在同一目录
     * - 标准YOLO结构（Nested YOLO Layout）：images/ + labels/ 子目录
     * - 异常检测结构（Anomalib Layout）：train/good + test/defective
     *
     * @param folderPath 用户选中的文件夹绝对路径
     * @return QVariantMap 包含：
     *         - "detectedFormat": QString ("yolo_txt" | "coco_json" | "anomaly_unsupervised" | "image_only")
     *         - "imageDir": QString (实际图片扫描基准路径)
     *         - "labelDirOrPath": QString (实际标签文件夹或 JSON 文件绝对路径)
     *         - "imageCount": int (探测到的有效图片总数)
     *         - "labelCount": int (探测到的有效标签文件或标注总数)
     *         - "unmatchedImagesCount": int (缺失标签的图片数)
     *         - "classIds": QVariantList (提取到的类别整数 ID 列表)
     *         - "classes": QVariantMap (COCO 中的 category_id -> name 映射)
     *         - "isValid": bool (是否是可导入的合法数据集)
     */
    Q_INVOKABLE QVariantMap scanFolder(const QString &folderPath);

    /**
     * @brief 验证单行 OBB 标签（9个值：class_id x1 y1 x2 y2 x3 y3 x4 y4）
     */
    static QVariantMap validateOBBLine(const QString &line);

    /**
     * @brief 递归收集目录下所有图片文件
     * @param dir 目标目录
     * @param recursive 是否递归子目录
     * @return QFileInfoList 图片文件列表
     */
    QFileInfoList collectImageFiles(const QDir &dir, bool recursive = true);

    /**
     * @brief 递归收集目录下所有标签文件
     * @param dir 目标目录
     * @param recursive 是否递归子目录
     * @return QFileInfoList 标签文件列表
     */
    QFileInfoList collectLabelFiles(const QDir &dir, bool recursive = true);

    static bool isImageFile(const QString &fileName);
    static bool isLabelFile(const QString &fileName);

    /**
     * @brief 递归收集目录下所有图片文件（静态版本，供外部调用）
     */
    static QFileInfoList collectImageFilesStatic(const QDir &dir, bool recursive = true);

    bool isLabelMeJsonFile(const QString &filePath);
    QVariantMap parseLabelMeJsonFile(const QString &filePath);
    QVariantMap scanWithLabelMeJsonLabels(const QString &imageDir, const QString &labelDir);

signals:
    void scanProgress(int current, int total);
    void scanCompleted();

private:
    /**
     * @brief 解析 YOLO txt 标签文件，提取类别 ID
     */
    bool parseLabelFile(const QString &filePath, QSet<int> &classIds, QStringList &errors);

    /**
     * @brief 解析 COCO JSON 标签文件
     */
    QVariantMap parseJsonLabelFile(const QString &filePath);

    /**
     * @brief 使用 COCO JSON 标签格式扫描
     */
    QVariantMap scanWithJsonLabels(const QString &imageDir, const QString &labelDir);

    /**
     * @brief 探测 Anomalib 异常检测目录结构
     * @param folderPath 根目录
     * @return QVariantMap 探测结果，包含 detectedFormat, imageCount 等
     */
    QVariantMap detectAnomalibLayout(const QString &folderPath);

    /**
     * @brief 探测标准 YOLO 嵌套目录结构
     * @param folderPath 根目录
     * @return QVariantMap 探测结果
     */
    QVariantMap detectNestedYoloLayout(const QString &folderPath);

    /**
     * @brief 探测扁平目录结构
     * @param folderPath 根目录
     * @return QVariantMap 探测结果
     */
    QVariantMap detectFlatLayout(const QString &folderPath);

    /**
     * @brief 探测 ImageNet 风格分类目录结构（每类一文件夹）
     * @param folderPath 根目录
     * @return QVariantMap 探测结果
     */
    QVariantMap detectClassifyLayout(const QString &folderPath);
};

#endif // IMPORTSCANNER_H
