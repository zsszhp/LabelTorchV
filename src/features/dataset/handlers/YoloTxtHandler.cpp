#include "YoloTxtHandler.h"
#include "ImportScanner.h"
#include "ScanContext.h"
#include "utils/Log.h"

#include <QDir>
#include <QMap>

bool YoloTxtHandler::canHandle(const QString &folderPath) const
{
    QDir rootDir(folderPath);

    // 检查是否有 images/ 和 labels/ 子目录（嵌套 YOLO）
    QString imagesPath = folderPath + QStringLiteral("/images");
    QString labelsPath = folderPath + QStringLiteral("/labels");
    bool hasImagesDir = QDir(imagesPath).exists();
    bool hasLabelsDir = QDir(labelsPath).exists();

    if (hasImagesDir || hasLabelsDir) {
        if (hasImagesDir) {
            QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(QDir(imagesPath), true);
            if (!imageFiles.isEmpty()) return true;
        }
    }

    // 检查扁平结构：根目录下有图片文件
    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(rootDir, false);
    if (!imageFiles.isEmpty()) {
        return true;
    }

    return false;
}

QVariantMap YoloTxtHandler::scanFolder(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "YoloTxtHandler::scanFolder folderPath=" << folderPath;

    // 先尝试嵌套 YOLO 布局
    QVariantMap nestedResult = detectNestedLayout(scanner, folderPath);
    if (nestedResult["isValid"].toBool()) {
        return nestedResult;
    }

    // 再尝试扁平布局
    QVariantMap flatResult = detectFlatLayout(scanner, folderPath);
    if (flatResult["isValid"].toBool()) {
        return flatResult;
    }

    return ScanContext::makeEmptyFolderResult();
}

QVariantMap YoloTxtHandler::scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "YoloTxtHandler::scan imageDir=" << imageDir << "labelDir=" << labelDir;

    QVariantMap result;
    QVariantList samples;

    QDir imgDir(imageDir);

    if (!imgDir.exists()) {
        ltError(LT_LOG_DATASET()) << "图片目录不存在:" << imageDir;
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["error"] = QStringLiteral("图片目录不存在: %1").arg(imageDir);
        return result;
    }

    bool hasLabelDir = !labelDir.isEmpty() && QDir(labelDir).exists();

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(imgDir, true);
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }

    QFileInfoList labelFiles;
    if (hasLabelDir) {
        labelFiles = ImportScanner::collectLabelFilesStatic(QDir(labelDir), true);
    }

    QMap<QString, QFileInfo> labelByStem;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("txt")) {
            labelByStem[fi.completeBaseName()] = fi;
        }
    }

    int totalImages = imageByStem.size();
    int totalLabels = labelByStem.size();
    int totalFiles = totalImages + totalLabels;
    int matched = 0;
    int unmatchedImages = 0;
    int unmatchedLabels = 0;

    QSet<QString> allStems;
    for (const auto &stem : imageByStem.keys()) allStems.insert(stem);
    for (const auto &stem : labelByStem.keys()) allStems.insert(stem);

    int processed = 0;
    for (const auto &stem : allStems) {
        processed++;
        emit scanner->scanProgress(processed, totalFiles);

        bool hasImage = imageByStem.contains(stem);
        bool hasLabel = labelByStem.contains(stem);

        QVariantMap sample;
        sample["stem"] = stem;

        if (hasImage) {
            sample["imagePath"] = imageByStem[stem].absoluteFilePath();
        }

        if (hasLabel) {
            sample["labelPath"] = labelByStem[stem].absoluteFilePath();
        }

        if (hasImage && hasLabel) {
            matched++;

            QSet<int> classIds;
            QStringList parseErrors;
            bool valid = ImportScanner::parseLabelFileStatic(labelByStem[stem].absoluteFilePath(), classIds, parseErrors);

            sample["status"] = valid ? QStringLiteral("matched") : QStringLiteral("invalid_label");
            sample["valid"] = valid;

            QVariantList classIdList;
            for (int cid : classIds) {
                classIdList.append(cid);
            }
            sample["classIds"] = classIdList;

            if (!valid) {
                sample["errors"] = parseErrors;
            }
        } else if (hasImage) {
            unmatchedImages++;
            sample["status"] = QStringLiteral("unmatched_image");
            sample["valid"] = true;
            sample["classIds"] = QVariantList();
        } else {
            unmatchedLabels++;
            sample["status"] = QStringLiteral("unmatched_label");
            sample["valid"] = false;
        }

        samples.append(sample);
    }

    result["total"] = totalImages + totalLabels;
    result["matched"] = matched;
    result["unmatchedImages"] = unmatchedImages;
    result["unmatchedLabels"] = unmatchedLabels;
    result["samples"] = samples;

    ltInfo(LT_LOG_DATASET()) << "YOLO txt scan completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels;

    return result;
}

QVariantMap YoloTxtHandler::detectNestedLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "YoloTxtHandler::detectNestedLayout folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QString imagesPath = folderPath + QStringLiteral("/images");
    QString labelsPath = folderPath + QStringLiteral("/labels");

    bool hasImagesDir = QDir(imagesPath).exists();
    bool hasLabelsDir = QDir(labelsPath).exists();

    if (!hasImagesDir && !hasLabelsDir) {
        return result;
    }

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(QDir(imagesPath), true);
    if (imageFiles.isEmpty()) {
        return result;
    }

    QFileInfoList labelFiles;
    int labelCount = 0;
    QSet<int> classIds;

    if (hasLabelsDir) {
        labelFiles = ImportScanner::collectLabelFilesStatic(QDir(labelsPath), true);

        // 检查是否有 JSON 标签（如果有，则不是纯 YOLO txt 格式）
        bool hasJsonLabels = false;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                hasJsonLabels = true;
                break;
            }
        }

        if (hasJsonLabels) {
            // 有 JSON 标签，不是纯 YOLO txt，返回无效
            return result;
        }

        // YOLO TXT 格式
        result["detectedFormat"] = QStringLiteral("yolo_txt");
        result["labelDirOrPath"] = labelsPath;

        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("txt")) {
                QSet<int> fileClassIds;
                QStringList errors;
                ImportScanner::parseLabelFileStatic(fi.absoluteFilePath(), fileClassIds, errors);
                classIds.unite(fileClassIds);
                labelCount++;
            }
        }
    } else {
        result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
        result["labelDirOrPath"] = QString();
    }

    // 按 stem 匹配
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }

    QMap<QString, QFileInfo> labelByStem;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("txt")) {
            labelByStem[fi.completeBaseName()] = fi;
        }
    }

    int unmatchedImages = 0;
    for (const auto &stem : imageByStem.keys()) {
        if (!labelByStem.contains(stem)) {
            unmatchedImages++;
        }
    }

    result["isValid"] = true;
    result["imageDir"] = imagesPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelByStem.size();
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = ScanContext::sortClassIds(classIds);

    ltInfo(LT_LOG_DATASET()) << "Nested YOLO layout detected: images=" << imageFiles.size()
                             << "labels=" << labelByStem.size()
                             << "format=" << result["detectedFormat"].toString();
    return result;
}

QVariantMap YoloTxtHandler::detectFlatLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "YoloTxtHandler::detectFlatLayout folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QDir rootDir(folderPath);

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(rootDir, false);
    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(rootDir, false);

    if (imageFiles.isEmpty()) {
        return result;
    }

    // 检查是否有 JSON 标签（如果有，不是纯 YOLO txt）
    bool hasJsonLabels = false;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            hasJsonLabels = true;
            break;
        }
    }

    QSet<int> classIds;
    int labelCount = 0;

    if (hasJsonLabels) {
        // 有 JSON 标签，不是纯 YOLO txt，返回无效
        return result;
    }

    // 检查是否有 TXT 标签
    bool hasTxtLabels = false;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("txt")) {
            hasTxtLabels = true;
            break;
        }
    }

    if (hasTxtLabels) {
        result["detectedFormat"] = QStringLiteral("yolo_txt");
        result["labelDirOrPath"] = folderPath;

        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("txt")) {
                QSet<int> fileClassIds;
                QStringList errors;
                ImportScanner::parseLabelFileStatic(fi.absoluteFilePath(), fileClassIds, errors);
                classIds.unite(fileClassIds);
                labelCount++;
            }
        }
    } else {
        result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
        result["labelDirOrPath"] = QString();
    }

    // 按 stem 匹配
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }

    QMap<QString, QFileInfo> labelByStem;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("txt")) {
            labelByStem[fi.completeBaseName()] = fi;
        }
    }

    int unmatchedImages = 0;
    for (const auto &stem : imageByStem.keys()) {
        if (!labelByStem.contains(stem)) {
            unmatchedImages++;
        }
    }

    result["isValid"] = true;
    result["imageDir"] = folderPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelByStem.size();
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = ScanContext::sortClassIds(classIds);

    ltInfo(LT_LOG_DATASET()) << "Flat layout detected: images=" << imageFiles.size()
                             << "labels=" << labelByStem.size()
                             << "format=" << result["detectedFormat"].toString();
    return result;
}
