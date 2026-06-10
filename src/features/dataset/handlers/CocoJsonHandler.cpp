#include "CocoJsonHandler.h"
#include "ImportScanner.h"
#include "ScanContext.h"
#include "utils/Log.h"

#include <QDir>
#include <QMap>

bool CocoJsonHandler::canHandle(const QString &folderPath) const
{
    QDir rootDir(folderPath);

    // 检查嵌套结构中的 COCO JSON
    QString labelsPath = folderPath + QStringLiteral("/labels");
    if (QDir(labelsPath).exists()) {
        QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(QDir(labelsPath), true);
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
                    return true;
                }
            }
        }
    }

    // 检查扁平结构中的 COCO JSON
    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(rootDir, false);
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
                return true;
            }
        }
    }

    return false;
}

QVariantMap CocoJsonHandler::scanFolder(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "CocoJsonHandler::scanFolder folderPath=" << folderPath;

    // 先尝试嵌套布局
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

QVariantMap CocoJsonHandler::scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "CocoJsonHandler::scan imageDir=" << imageDir << "labelDir=" << labelDir;

    QVariantMap result;
    QVariantList samples;

    QDir imgDir(imageDir);
    QDir lblDir(labelDir);

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(imgDir, true);
    QMap<QString, QFileInfo> imageByFileName;
    QMap<QString, QFileInfo> imageByBaseFileName;
    for (const auto &fi : imageFiles) {
        imageByFileName[fi.fileName()] = fi;
        imageByBaseFileName[fi.fileName()] = fi;
    }

    QFileInfoList jsonFiles;
    QFileInfoList allLabelFiles = ImportScanner::collectLabelFilesStatic(lblDir, true);
    for (const auto &fi : allLabelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")
            && !ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
            jsonFiles.append(fi);
        }
    }

    if (jsonFiles.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "CocoJsonHandler::scan: no COCO JSON files found";
        result = ScanContext::makeEmptyScanResult();
        result["error"] = QStringLiteral("标签目录中未找到 COCO JSON 文件: %1").arg(labelDir);
        return result;
    }

    // 合并所有 JSON 文件的解析结果
    QMap<int, QVariantMap> allImagesMap;
    QMultiMap<int, QVariantMap> allAnnotationsMap;
    QMap<int, QString> allCategories;
    QSet<int> allClassIds;
    QStringList allErrors;
    bool anyValid = false;

    for (const auto &jsonFi : jsonFiles) {
        QVariantMap parseResult = ImportScanner::parseJsonLabelFileStatic(jsonFi.absoluteFilePath());

        if (!parseResult["valid"].toBool()) {
            QStringList fileErrors = parseResult["errors"].toStringList();
            for (const auto &err : fileErrors) {
                allErrors.append(QStringLiteral("[%1] %2").arg(jsonFi.fileName(), err));
            }
            continue;
        }

        anyValid = true;

        QMap<int, QVariantMap> imagesMap = parseResult["images"].value<QMap<int, QVariantMap>>();
        for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
            allImagesMap[it.key()] = it.value();
        }

        QMultiMap<int, QVariantMap> annotationsMap = parseResult["annotations"].value<QMultiMap<int, QVariantMap>>();
        for (auto it = annotationsMap.constBegin(); it != annotationsMap.constEnd(); ++it) {
            allAnnotationsMap.insert(it.key(), it.value());
        }

        QMap<int, QString> categories = parseResult["categories"].value<QMap<int, QString>>();
        for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
            allCategories[it.key()] = it.value();
        }

        QSet<int> classIds = parseResult["classIds"].value<QSet<int>>();
        for (int cid : classIds) {
            allClassIds.insert(cid);
        }
    }

    if (!anyValid) {
        ltWarning(LT_LOG_DATASET()) << "CocoJsonHandler::scan: all JSON files are invalid";
        result = ScanContext::makeEmptyScanResult();
        result["errors"] = allErrors;
        result["error"] = QStringLiteral("所有 JSON 标签文件均无效");
        return result;
    }

    int matched = 0;
    int unmatchedImages = 0;
    int processed = 0;
    int totalEntries = allImagesMap.size() + imageByFileName.size();
    QSet<QString> matchedImageFiles;

    for (auto it = allImagesMap.constBegin(); it != allImagesMap.constEnd(); ++it) {
        processed++;
        emit scanner->scanProgress(processed, totalEntries);

        int imgId = it.key();
        QVariantMap imgInfo = it.value();
        QString fileName = imgInfo["file_name"].toString();

        QVariantMap sample;
        sample["imageId"] = imgId;

        QFileInfo matchedImgFi;
        QString matchKey;
        if (imageByFileName.contains(fileName)) {
            matchedImgFi = imageByFileName[fileName];
            matchKey = fileName;
        } else {
            QString baseName = QFileInfo(fileName).fileName();
            if (imageByBaseFileName.contains(baseName)) {
                matchedImgFi = imageByBaseFileName[baseName];
                matchKey = baseName;
            }
        }

        if (matchedImgFi.exists()) {
            sample["imagePath"] = matchedImgFi.absoluteFilePath();
            sample["labelPath"] = jsonFiles.first().absoluteFilePath();
            sample["stem"] = matchedImgFi.completeBaseName();
            matchedImageFiles.insert(matchKey);

            QList<QVariantMap> imgAnnotations;
            auto range = allAnnotationsMap.equal_range(imgId);
            for (auto annIt = range.first; annIt != range.second; ++annIt) {
                imgAnnotations.append(annIt.value());
            }

            QSet<int> imgClassIds;
            for (const auto &ann : imgAnnotations) {
                imgClassIds.insert(ann["category_id"].toInt());
            }

            QVariantList classIdList;
            for (int cid : imgClassIds) {
                classIdList.append(cid);
            }

            QVariantList annList;
            for (const auto &ann : imgAnnotations) {
                annList.append(ann);
            }

            sample["status"] = QStringLiteral("matched");
            sample["valid"] = true;
            sample["classIds"] = classIdList;
            sample["annotations"] = annList;
            matched++;
        } else {
            sample["status"] = QStringLiteral("unmatched_label");
            sample["valid"] = false;
            sample["fileName"] = fileName;
            unmatchedImages++;
        }

        samples.append(sample);
    }

    int unmatchedLabels = 0;
    for (auto imgIt = imageByFileName.constBegin(); imgIt != imageByFileName.constEnd(); ++imgIt) {
        if (!matchedImageFiles.contains(imgIt.key())) {
            processed++;
            emit scanner->scanProgress(processed, totalEntries);

            QVariantMap sample;
            sample["imagePath"] = imgIt.value().absoluteFilePath();
            sample["stem"] = imgIt.value().completeBaseName();
            sample["status"] = QStringLiteral("unmatched_image");
            sample["valid"] = false;
            samples.append(sample);
            unmatchedLabels++;
        }
    }

    QVariantMap categoriesMap;
    for (auto catIt = allCategories.constBegin(); catIt != allCategories.constEnd(); ++catIt) {
        categoriesMap[QString::number(catIt.key())] = catIt.value();
    }

    result["total"] = allImagesMap.size() + imageByFileName.size();
    result["matched"] = matched;
    result["unmatchedImages"] = unmatchedImages;
    result["unmatchedLabels"] = unmatchedLabels;
    result["samples"] = samples;
    result["categories"] = categoriesMap;

    if (!allErrors.isEmpty()) {
        result["parseErrors"] = allErrors;
    }

    ltInfo(LT_LOG_DATASET()) << "COCO JSON scan completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels
                             << "categories:" << allCategories.size();

    return result;
}

QVariantMap CocoJsonHandler::detectNestedLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "CocoJsonHandler::detectNestedLayout folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QString imagesPath = folderPath + QStringLiteral("/images");
    QString labelsPath = folderPath + QStringLiteral("/labels");

    if (!QDir(imagesPath).exists() || !QDir(labelsPath).exists()) {
        return result;
    }

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(QDir(imagesPath), true);
    if (imageFiles.isEmpty()) {
        return result;
    }

    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(QDir(labelsPath), true);

    // 查找 COCO JSON 文件
    QString jsonLabelPath;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")
            && !ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
            jsonLabelPath = fi.absoluteFilePath();
            break;
        }
    }

    if (jsonLabelPath.isEmpty()) {
        return result;
    }

    result["detectedFormat"] = QStringLiteral("coco_json");
    result["labelDirOrPath"] = jsonLabelPath;

    QVariantMap jsonResult = ImportScanner::parseJsonLabelFileStatic(jsonLabelPath);
    if (jsonResult["valid"].toBool()) {
        QSet<int> jsonClassIds = jsonResult["classIds"].value<QSet<int>>();
        QVariantMap categories = jsonResult["categories"].toMap();
        QMap<int, QVariantMap> imagesMap = jsonResult["images"].value<QMap<int, QVariantMap>>();
        QMultiMap<int, QVariantMap> annotationsMap = jsonResult["annotations"].value<QMultiMap<int, QVariantMap>>();

        QSet<QString> imageFileNames;
        for (const auto &fi : imageFiles) {
            imageFileNames.insert(fi.fileName());
        }

        int labelCount = 0;
        int unmatchedImages = 0;
        QSet<QString> matchedImageFiles;

        for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
            QString fileName = it.value()["file_name"].toString();
            QString baseFileName = QFileInfo(fileName).fileName();
            if (imageFileNames.contains(fileName) || imageFileNames.contains(baseFileName)) {
                matchedImageFiles.insert(fileName);
                if (annotationsMap.contains(it.key())) {
                    labelCount++;
                }
            }
        }

        for (const auto &fi : imageFiles) {
            if (!matchedImageFiles.contains(fi.fileName())) {
                unmatchedImages++;
            }
        }

        result["isValid"] = true;
        result["imageDir"] = imagesPath;
        result["imageCount"] = imageFiles.size();
        result["labelCount"] = labelCount;
        result["unmatchedImagesCount"] = unmatchedImages;
        result["classIds"] = ScanContext::sortClassIds(jsonClassIds);
        result["classes"] = categories;

        ltInfo(LT_LOG_DATASET()) << "Nested COCO JSON layout detected: images=" << imageFiles.size()
                                 << "labeled=" << labelCount
                                 << "classes=" << ScanContext::sortClassIds(jsonClassIds).size();
    } else {
        result["isValid"] = true;
        result["imageDir"] = imagesPath;
        result["imageCount"] = imageFiles.size();
        result["labelCount"] = 0;
        result["unmatchedImagesCount"] = imageFiles.size();
        result["classIds"] = QVariantList();
        result["classes"] = QVariantMap();
    }

    return result;
}

QVariantMap CocoJsonHandler::detectFlatLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "CocoJsonHandler::detectFlatLayout folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QDir rootDir(folderPath);

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(rootDir, false);
    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(rootDir, false);

    if (imageFiles.isEmpty()) {
        return result;
    }

    // 查找 COCO JSON 文件
    QString jsonLabelPath;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")
            && !ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
            jsonLabelPath = fi.absoluteFilePath();
            break;
        }
    }

    if (jsonLabelPath.isEmpty()) {
        return result;
    }

    result["detectedFormat"] = QStringLiteral("coco_json");
    result["labelDirOrPath"] = jsonLabelPath;

    QVariantMap jsonResult = ImportScanner::parseJsonLabelFileStatic(jsonLabelPath);
    if (jsonResult["valid"].toBool()) {
        QSet<int> jsonClassIds = jsonResult["classIds"].value<QSet<int>>();
        QVariantMap categories = jsonResult["categories"].toMap();
        QMap<int, QVariantMap> imagesMap = jsonResult["images"].value<QMap<int, QVariantMap>>();
        QMultiMap<int, QVariantMap> annotationsMap = jsonResult["annotations"].value<QMultiMap<int, QVariantMap>>();

        QSet<QString> imageFileNames;
        for (const auto &fi : imageFiles) {
            imageFileNames.insert(fi.fileName());
        }

        int labelCount = 0;
        int unmatchedImages = 0;
        QSet<QString> matchedImageFiles;

        for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
            QString fileName = it.value()["file_name"].toString();
            QString baseFileName = QFileInfo(fileName).fileName();
            if (imageFileNames.contains(fileName) || imageFileNames.contains(baseFileName)) {
                matchedImageFiles.insert(fileName);
                if (annotationsMap.contains(it.key())) {
                    labelCount++;
                }
            }
        }

        for (const auto &fi : imageFiles) {
            if (!matchedImageFiles.contains(fi.fileName())) {
                unmatchedImages++;
            }
        }

        result["isValid"] = true;
        result["imageDir"] = folderPath;
        result["imageCount"] = imageFiles.size();
        result["labelCount"] = labelCount;
        result["unmatchedImagesCount"] = unmatchedImages;
        result["classIds"] = ScanContext::sortClassIds(jsonClassIds);
        result["classes"] = categories;

        ltInfo(LT_LOG_DATASET()) << "Flat COCO JSON layout detected: images=" << imageFiles.size()
                                 << "labeled=" << labelCount
                                 << "classes=" << ScanContext::sortClassIds(jsonClassIds).size();
    } else {
        result["isValid"] = true;
        result["imageDir"] = folderPath;
        result["imageCount"] = imageFiles.size();
        result["labelCount"] = 0;
        result["unmatchedImagesCount"] = imageFiles.size();
        result["classIds"] = QVariantList();
        result["classes"] = QVariantMap();
    }

    return result;
}
