#ifndef IMPORTSCANNER_H
#define IMPORTSCANNER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QSet>

class ImportScanner : public QObject
{
    Q_OBJECT

public:
    explicit ImportScanner(QObject *parent = nullptr);

    /**
     * @brief Scan directories for image-label pairs in YOLO txt format.
     *
     * Matches images (.jpg/.jpeg/.png/.bmp) with labels (.txt) by filename stem.
     * Parses each label file to extract class_ids and validate format
     * (each line: class_id cx cy w h, all floats 0-1).
     *
     * @param imageDir Directory containing image files.
     * @param labelDir Directory containing YOLO txt label files.
     * @return QVariantMap with keys: total, matched, unmatchedImages, unmatchedLabels,
     *         samples (QVariantList of {imagePath, labelPath, status, classIds, valid})
     */
    Q_INVOKABLE QVariantMap scan(const QString &imageDir, const QString &labelDir);

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

    static bool isImageFile(const QString &fileName);
    static bool isLabelFile(const QString &fileName);
};

#endif // IMPORTSCANNER_H
