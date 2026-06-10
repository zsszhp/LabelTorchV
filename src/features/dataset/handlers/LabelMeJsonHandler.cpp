#include "LabelMeJsonHandler.h"
#include "ImportScanner.h"
#include "ScanContext.h"
#include "utils/Log.h"

#include <QDir>
#include <QMap>

bool LabelMeJsonHandler::canHandle(const QString &folderPath) const
{
    QDir rootDir(folderPath);

    // 检查嵌套结构中的 LabelMe JSON
    QString labelsPath = folderPath + QStringLiteral("/labels");
    if (QDir(labelsPath).exists()) {
        QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(QDir(labelsPath), true);
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                if (ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
                    return true;
                }
            }
        }
    }

    // 检查扁平结构中的 LabelMe JSON
    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(rootDir, false);
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            if (ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
                return true;
            }
        }
    }

    return false;
}

QVariantMap LabelMeJsonHandler::scanFolder(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "LabelMeJsonHandler::scanFolder folderPath=" << folderPath;

    QVariantMap nestedResult = detectNestedLayout(scanner, folderPath);
    if (nestedResult["isValid"].toBool()) {
        return nestedResult;
    }

    QVariantMap flatResult = detectFlatLayout(scanner, folderPath);
    if (flatResult["isValid"].toBool()) {
        return flatResult;
    }

    return ScanContext::makeEmptyFolderResult();
}

QVariantMap LabelMeJsonHandler::scan(ImportScanner *scanner, const QString &imageDir, const QString &labelDir)
{
    ltInfo(LT_LOG_DATASET()) << "LabelMeJsonHandler::scan imageDir=" << imageDir << "labelDir=" << labelDir;

    QVariantMap result;
    QVariantList samples;

    QDir imgDir(imageDir);
    QDir lblDir(labelDir);

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(imgDir, true);
    QMap<QString, QFileInfo> imageByFileName;
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByFileName[fi.fileName()] = fi;
        imageByStem[fi.completeBaseName()] = fi;
    }

    QFileInfoList jsonFiles;
    QFileInfoList allLabelFiles = ImportScanner::collectLabelFilesStatic(lblDir, true);
    for (const auto &fi : allLabelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")
            && ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) {
            jsonFiles.append(fi);
        }
    }

    if (jsonFiles.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "LabelMeJsonHandler::scan: no LabelMe JSON files found";
        result = ScanContext::makeEmptyScanResult();
        result["error"] = QStringLiteral("标签目录中未找到 LabelMe JSON 文件: %1").arg(labelDir);
        return result;
    }

    QMap<QString, int> globalLabelToClassId;
    int nextGlobalClassId = 0;
    QSet<int> allClassIds;
    QStringList allErrors;
    int matched = 0;
    int unmatchedImages = 0;
    QSet<QString> matchedImageFiles;

    int processed = 0;
    int totalEntries = jsonFiles.size() + imageFiles.size();

    for (const auto &jsonFi : jsonFiles) {
        processed++;
        emit scanner->scanProgress(processed, totalEntries);

        QVariantMap parseResult = ImportScanner::parseLabelMeJsonFileStatic(jsonFi.absoluteFilePath());

        if (!parseResult["valid"].toBool()) {
            QStringList fileErrors = parseResult["errors"].toStringList();
            for (const auto &err : fileErrors) {
                allErrors.append(QStringLiteral("[%1] %2").arg(jsonFi.fileName(), err));
            }
            continue;
        }

        QVariantMap labelMap = parseResult["labelToClassId"].toMap();
        for (auto it = labelMap.constBegin(); it != labelMap.constEnd(); ++it) {
            QString label = it.key();
            if (!globalLabelToClassId.contains(label)) {
                globalLabelToClassId[label] = nextGlobalClassId++;
            }
        }

        QString jsonImagePath = parseResult["imagePath"].toString();
        int imageWidth = parseResult["imageWidth"].toInt();
        int imageHeight = parseResult["imageHeight"].toInt();
        QVariantList shapes = parseResult["shapes"].toList();

        QFileInfo matchedImgFi;
        QString matchKey;

        if (!jsonImagePath.isEmpty()) {
            QString baseFileName = QFileInfo(jsonImagePath).fileName();
            if (imageByFileName.contains(baseFileName)) {
                matchedImgFi = imageByFileName[baseFileName];
                matchKey = baseFileName;
            }
        }

        if (!matchedImgFi.exists()) {
            QString jsonStem = jsonFi.completeBaseName();
            if (imageByStem.contains(jsonStem)) {
                matchedImgFi = imageByStem[jsonStem];
                matchKey = matchedImgFi.fileName();
            }
        }

        QVariantMap sample;
        sample["labelPath"] = jsonFi.absoluteFilePath();

        QVariantList remappedShapes;
        QSet<int> imgClassIds;
        for (const auto &shapeVal : shapes) {
            QVariantMap shapeMap = shapeVal.toMap();
            QString label = shapeMap["label"].toString();
            if (globalLabelToClassId.contains(label)) {
                int globalId = globalLabelToClassId[label];
                shapeMap["classId"] = globalId;
                imgClassIds.insert(globalId);
            }
            remappedShapes.append(shapeMap);
        }

        QVariantList classIdList;
        QList<int> sortedImgClassIds = imgClassIds.values();
        std::sort(sortedImgClassIds.begin(), sortedImgClassIds.end());
        for (int cid : sortedImgClassIds) {
            classIdList.append(cid);
            allClassIds.insert(cid);
        }

        if (matchedImgFi.exists()) {
            sample["imagePath"] = matchedImgFi.absoluteFilePath();
            sample["stem"] = matchedImgFi.completeBaseName();
            sample["status"] = QStringLiteral("matched");
            sample["valid"] = true;
            sample["classIds"] = classIdList;
            sample["annotations"] = remappedShapes;
            sample["imageWidth"] = imageWidth;
            sample["imageHeight"] = imageHeight;
            matchedImageFiles.insert(matchKey);
            matched++;
        } else {
            sample["status"] = QStringLiteral("unmatched_label");
            sample["valid"] = false;
            sample["fileName"] = jsonImagePath.isEmpty() ? jsonFi.fileName() : jsonImagePath;
            sample["classIds"] = classIdList;
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
            sample["valid"] = true;
            sample["classIds"] = QVariantList();
            samples.append(sample);
            unmatchedLabels++;
        }
    }

    QVariantMap categoriesMap;
    for (auto it = globalLabelToClassId.constBegin(); it != globalLabelToClassId.constEnd(); ++it) {
        categoriesMap[QString::number(it.value())] = it.key();
    }

    result["total"] = jsonFiles.size() + imageFiles.size();
    result["matched"] = matched;
    result["unmatchedImages"] = unmatchedImages;
    result["unmatchedLabels"] = unmatchedLabels;
    result["samples"] = samples;
    result["categories"] = categoriesMap;
    result["classIds"] = ScanContext::sortClassIds(allClassIds);

    if (!allErrors.isEmpty()) {
        result["parseErrors"] = allErrors;
    }

    ltInfo(LT_LOG_DATASET()) << "LabelMe JSON scan completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels
                             << "categories:" << globalLabelToClassId.size();

    return result;
}

QVariantMap LabelMeJsonHandler::detectNestedLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "LabelMeJsonHandler::detectNestedLayout folderPath=" << folderPath;

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

    // 解析所有 LabelMe JSON 文件获取统计信息
    int labelCount = 0;
    QSet<int> classIds;
    QMap<QString, int> labelToClassId;
    int nextClassId = 0;

    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() != QStringLiteral("json")) continue;
        if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) continue;

        QVariantMap parseResult = ImportScanner::parseLabelMeJsonFileStatic(fi.absoluteFilePath());
        if (parseResult["valid"].toBool()) {
            labelCount++;
            QVariantMap localLabelMap = parseResult["labelToClassId"].toMap();
            for (auto it = localLabelMap.constBegin(); it != localLabelMap.constEnd(); ++it) {
                if (!labelToClassId.contains(it.key())) {
                    labelToClassId[it.key()] = nextClassId++;
                }
            }
        }
    }

    if (labelCount == 0) {
        return result;
    }

    for (int cid : labelToClassId.values()) {
        classIds.insert(cid);
    }

    QVariantMap categoriesMap;
    for (auto it = labelToClassId.constBegin(); it != labelToClassId.constEnd(); ++it) {
        categoriesMap[QString::number(it.value())] = it.key();
    }

    int unmatchedImages = 0;
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() != QStringLiteral("json")) continue;
        if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) continue;
        QString stem = fi.completeBaseName();
        if (!imageByStem.contains(stem)) {
            unmatchedImages++;
        }
    }

    result["isValid"] = true;
    result["detectedFormat"] = QStringLiteral("labelme_json");
    result["imageDir"] = imagesPath;
    result["labelDirOrPath"] = labelsPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelCount;
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = ScanContext::sortClassIds(classIds);
    result["classes"] = categoriesMap;

    ltInfo(LT_LOG_DATASET()) << "Nested LabelMe JSON layout detected: images=" << imageFiles.size()
                             << "labeled=" << labelCount
                             << "classes=" << ScanContext::sortClassIds(classIds).size();
    return result;
}

QVariantMap LabelMeJsonHandler::detectFlatLayout(ImportScanner *scanner, const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "LabelMeJsonHandler::detectFlatLayout folderPath=" << folderPath;

    QVariantMap result = ScanContext::makeEmptyFolderResult();

    QDir rootDir(folderPath);

    QFileInfoList imageFiles = ImportScanner::collectImageFilesStatic(rootDir, false);
    QFileInfoList labelFiles = ImportScanner::collectLabelFilesStatic(rootDir, false);

    if (imageFiles.isEmpty()) {
        return result;
    }

    int labelCount = 0;
    QSet<int> classIds;
    QMap<QString, int> labelToClassId;
    int nextClassId = 0;

    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() != QStringLiteral("json")) continue;
        if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) continue;

        QVariantMap parseResult = ImportScanner::parseLabelMeJsonFileStatic(fi.absoluteFilePath());
        if (parseResult["valid"].toBool()) {
            labelCount++;
            QVariantMap localLabelMap = parseResult["labelToClassId"].toMap();
            for (auto it = localLabelMap.constBegin(); it != localLabelMap.constEnd(); ++it) {
                if (!labelToClassId.contains(it.key())) {
                    labelToClassId[it.key()] = nextClassId++;
                }
            }
        }
    }

    if (labelCount == 0) {
        return result;
    }

    for (int cid : labelToClassId.values()) {
        classIds.insert(cid);
    }

    QVariantMap categoriesMap;
    for (auto it = labelToClassId.constBegin(); it != labelToClassId.constEnd(); ++it) {
        categoriesMap[QString::number(it.value())] = it.key();
    }

    int unmatchedImages = 0;
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() != QStringLiteral("json")) continue;
        if (!ImportScanner::isLabelMeJsonFileStatic(fi.absoluteFilePath())) continue;
        QString stem = fi.completeBaseName();
        if (!imageByStem.contains(stem)) {
            unmatchedImages++;
        }
    }

    result["isValid"] = true;
    result["detectedFormat"] = QStringLiteral("labelme_json");
    result["imageDir"] = folderPath;
    result["labelDirOrPath"] = folderPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelCount;
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = ScanContext::sortClassIds(classIds);
    result["classes"] = categoriesMap;

    ltInfo(LT_LOG_DATASET()) << "Flat LabelMe JSON layout detected: images=" << imageFiles.size()
                             << "labeled=" << labelCount
                             << "classes=" << ScanContext::sortClassIds(classIds).size();
    return result;
}
