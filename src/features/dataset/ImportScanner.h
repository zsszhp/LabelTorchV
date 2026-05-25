#ifndef IMPORTSCANNER_H
#define IMPORTSCANNER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QSet>
#include <QMap>

class ImportScanner : public QObject
{
    Q_OBJECT

public:
    explicit ImportScanner(QObject *parent = nullptr);

    /**
     * @brief Scan directories for image-label pairs.
     *
     * Auto-detects label format: COCO JSON (.json) takes priority over YOLO txt (.txt).
     * When both formats exist in labelDir, JSON is used.
     *
     * @param imageDir Directory containing image files.
     * @param labelDir Directory containing label files (.txt or .json).
     * @return QVariantMap with keys: total, matched, unmatchedImages, unmatchedLabels,
     *         samples (QVariantList of {imagePath, labelPath, status, classIds, valid}),
     *         categories (QVariantMap, only for JSON format: category_id -> name)
     */
    Q_INVOKABLE QVariantMap scan(const QString &imageDir, const QString &labelDir);

    /**
     * @brief Validate a single OBB label line (9 values: class_id x1 y1 x2 y2 x3 y3 x4 y4).
     *
     * Splits on whitespace, expects exactly 9 parts.
     * First part must be a non-negative integer. Parts 1-8 must be valid floats in [0,1].
     *
     * @param line A trimmed label line string.
     * @return QVariantMap with "valid" (bool), "error" (string), "classId" (int).
     */
    static QVariantMap validateOBBLine(const QString &line);

signals:
    void scanProgress(int current, int total);
    void scanCompleted();

private:
    /**
     * @brief Parse a YOLO txt label file and extract class IDs.
     * @param filePath Path to the .txt label file.
     * @param classIds Output set of class IDs found in the file.
     * @param errors Output list of per-line error descriptions.
     * @return true if the file is valid (all lines have correct format), false otherwise.
     */
    bool parseLabelFile(const QString &filePath, QSet<int> &classIds, QStringList &errors);

    /**
     * @brief Parse a COCO JSON label file and extract annotations.
     *
     * Reads a JSON file in COCO format (images, annotations, categories).
     * Converts COCO bbox [x_min, y_min, width, height] (pixel coords)
     * to YOLO format [cx, cy, w, h] (normalized 0-1).
     *
     * @param filePath Path to the .json label file.
     * @return QVariantMap with keys:
     *         "valid" (bool), "classIds" (QSet<int>), "errors" (QStringList),
     *         "categories" (QMap<int, QString>),
     *         "images" (QMap<int, QVariantMap>: image_id -> {file_name, width, height}),
     *         "annotations" (QMultiMap<int, QVariantMap>: image_id -> {category_id, cx, cy, w, h})
     */
    QVariantMap parseJsonLabelFile(const QString &filePath);

    /**
     * @brief Scan using COCO JSON label format.
     *
     * Reads JSON label file(s) from labelDir, matches images by file_name,
     * creates sample records with converted annotations.
     *
     * @param imageDir Directory containing image files.
     * @param labelDir Directory containing COCO JSON label files.
     * @return QVariantMap with same keys as scan(), plus "categories".
     */
    QVariantMap scanWithJsonLabels(const QString &imageDir, const QString &labelDir);

    static bool isImageFile(const QString &fileName);
    static bool isLabelFile(const QString &fileName);
};

#endif // IMPORTSCANNER_H
