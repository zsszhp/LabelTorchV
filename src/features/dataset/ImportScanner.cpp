#include "ImportScanner.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMap>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImageReader>

ImportScanner::ImportScanner(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_DATASET()) << "ImportScanner parent=" << parent;
}

QVariantMap ImportScanner::scan(const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "scan imageDir=" << imageDir << "labelDir=" << labelDir;

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
        emit scanCompleted();
        return result;
    }

    // 标签目录可以为空（无标签导入场景：异常检测或待标注数据）
    bool hasLabelDir = !labelDir.isEmpty() && QDir(labelDir).exists();

    ltInfo(LT_LOG_DATASET()) << "扫描开始: imageDir=" << imageDir
                             << "labelDir=" << labelDir
                             << "hasLabelDir=" << hasLabelDir;

    // 检测标签目录中是否存在 JSON 文件，优先使用 JSON 格式
    bool hasJsonLabels = false;
    bool hasLabelMeJson = false;
    bool hasCocoJson = false;
    QFileInfoList labelFiles;
    if (hasLabelDir) {
        labelFiles = collectLabelFiles(QDir(labelDir), true);
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                hasJsonLabels = true;
                if (isLabelMeJsonFile(fi.absoluteFilePath())) {
                    hasLabelMeJson = true;
                } else {
                    hasCocoJson = true;
                }
            }
        }
    }

    // LabelMe JSON 优先（每图一个 JSON 文件，更常见的标注格式）
    if (hasLabelMeJson) {
        ltInfo(LT_LOG_DATASET()) << "检测到 LabelMe JSON 标签文件，使用 LabelMe JSON 扫描流程";
        QVariantMap labelMeResult = scanWithLabelMeJsonLabels(imageDir, labelDir);
        emit scanCompleted();
        return labelMeResult;
    }

    // COCO JSON 格式（单文件包含所有图片和标注）
    if (hasCocoJson) {
        ltInfo(LT_LOG_DATASET()) << "检测到 COCO JSON 标签文件，使用 COCO JSON 扫描流程";
        QVariantMap jsonResult = scanWithJsonLabels(imageDir, labelDir);
        emit scanCompleted();
        return jsonResult;
    }

    // 以下为 YOLO txt 扫描流程（递归收集图片和标签文件）
    QFileInfoList imageFiles = collectImageFiles(imgDir, true);
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByStem[fi.completeBaseName()] = fi;
    }

    // 收集标签文件（递归）
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

    // 合并所有 stem
    QSet<QString> allStems;
    for (const auto &stem : imageByStem.keys()) allStems.insert(stem);
    for (const auto &stem : labelByStem.keys()) allStems.insert(stem);

    int processed = 0;
    for (const auto &stem : allStems) {
        processed++;
        emit scanProgress(processed, totalFiles);

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

            // 解析标签文件
            QSet<int> classIds;
            QStringList parseErrors;
            bool valid = parseLabelFile(labelByStem[stem].absoluteFilePath(), classIds, parseErrors);

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
            // 有图片无标签：仍为有效样本（可用于异常检测或待标注数据）
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

    ltInfo(LT_LOG_DATASET()) << "扫描完成 - 匹配:" << matched
                             << "无标签图片:" << unmatchedImages
                             << "无图片标签:" << unmatchedLabels;

    emit scanCompleted();
    return result;
}

bool ImportScanner::parseLabelFile(const QString &filePath, QSet<int> &classIds, QStringList &errors)
{
    ltTrace(LT_LOG_DATASET()) << "parseLabelFile filePath=" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors.append(QStringLiteral("无法打开文件"));
        ltWarning(LT_LOG_DATASET()) << "parseLabelFile: 无法打开文件:" << filePath;
        return false;
    }

    bool allValid = true;
    int lineNumber = 0;
    QTextStream in(&file);

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        lineNumber++;

        if (line.isEmpty()) continue;

        QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);

        // 支持 HBB 格式（5值: class_id cx cy w h）和 OBB 格式（9值: class_id x1 y1 x2 y2 x3 y3 x4 y4）
        if (parts.size() != 5 && parts.size() != 9) {
            errors.append(QStringLiteral("第 %1 行: 期望5值(HBB)或9值(OBB)，实际%2个值")
                              .arg(lineNumber).arg(parts.size()));
            allValid = false;
            continue;
        }

        // 解析 class_id（必须为非负整数）
        bool ok = false;
        int classId = parts[0].toInt(&ok);
        if (!ok || classId < 0) {
            errors.append(QStringLiteral("第 %1 行: 无效的 class_id '%2'")
                              .arg(lineNumber).arg(parts[0]));
            allValid = false;
            continue;
        }

        // 验证坐标值（必须在 [0, 1] 范围内）
        bool coordsValid = true;
        int coordCount = parts.size() - 1;
        for (int i = 1; i <= coordCount; ++i) {
            bool convOk = false;
            double val = parts[i].toDouble(&convOk);
            if (!convOk || val < 0.0 || val > 1.0) {
                errors.append(QStringLiteral("第 %1 行: 坐标 '%2' 超出范围 [0,1]")
                                  .arg(lineNumber).arg(parts[i]));
                coordsValid = false;
                allValid = false;
                break;
            }
        }

        if (coordsValid) {
            classIds.insert(classId);
        }
    }

    ltDebug(LT_LOG_DATASET()) << "parseLabelFile: filePath=" << filePath
                              << "valid=" << allValid << "classIds=" << classIds.size();
    return allValid;
}

QVariantMap ImportScanner::validateOBBLine(const QString &line)
{
    ltTrace(LT_LOG_DATASET()) << "validateOBBLine line=" << line.left(50);

    QVariantMap result;
    result["valid"] = false;
    result["error"] = QString();
    result["classId"] = -1;

    if (line.isEmpty()) {
        result["error"] = QStringLiteral("Empty line");
        return result;
    }

    QStringList parts = line.split(QChar(' '), Qt::SkipEmptyParts);
    if (parts.size() != 9) {
        result["error"] = QStringLiteral("Expected 9 values (OBB), got %1").arg(parts.size());
        return result;
    }

    // Parse class_id (must be non-negative integer)
    bool ok = false;
    int classId = parts[0].toInt(&ok);
    if (!ok || classId < 0) {
        result["error"] = QStringLiteral("Invalid class_id '%1'").arg(parts[0]);
        return result;
    }

    // Parse x1 y1 x2 y2 x3 y3 x4 y4 (must be floats in [0, 1])
    for (int i = 1; i < 9; ++i) {
        bool convOk = false;
        double val = parts[i].toDouble(&convOk);
        if (!convOk || val < 0.0 || val > 1.0) {
            result["error"] = QStringLiteral("Coordinate '%1' out of range [0,1]").arg(parts[i]);
            return result;
        }
    }

    result["valid"] = true;
    result["classId"] = classId;
    return result;
}

bool ImportScanner::isImageFile(const QString &fileName)
{
    ltTrace(LT_LOG_DATASET()) << "isImageFile fileName=" << fileName;

    QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == QStringLiteral("jpg")
        || ext == QStringLiteral("jpeg")
        || ext == QStringLiteral("png")
        || ext == QStringLiteral("bmp")
        || ext == QStringLiteral("tif")
        || ext == QStringLiteral("tiff")
        || ext == QStringLiteral("webp")
        || ext == QStringLiteral("pbm")
        || ext == QStringLiteral("pgm")
        || ext == QStringLiteral("ppm");
}

bool ImportScanner::isLabelFile(const QString &fileName)
{
    ltTrace(LT_LOG_DATASET()) << "isLabelFile fileName=" << fileName;

    QString ext = QFileInfo(fileName).suffix().toLower();
    return ext == QStringLiteral("txt") || ext == QStringLiteral("json");
}

QVariantMap ImportScanner::parseJsonLabelFile(const QString &filePath)
{
    ltTrace(LT_LOG_DATASET()) << "parseJsonLabelFile filePath=" << filePath;

    QVariantMap result;
    QSet<int> classIds;
    QStringList errors;
    QMap<int, QString> categories;
    QMap<int, QVariantMap> imagesMap;
    QMultiMap<int, QVariantMap> annotationsMap;

    // 读取 JSON 文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors.append(QStringLiteral("无法打开文件: %1").arg(filePath));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: cannot open file:" << filePath;
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["errors"] = errors;
        result["categories"] = QVariant::fromValue(categories);
        result["images"] = QVariant::fromValue(imagesMap);
        result["annotations"] = QVariant::fromValue(annotationsMap);
        return result;
    }

    // 解析 JSON 文档
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errors.append(QStringLiteral("JSON 解析错误: %1").arg(parseError.errorString()));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: JSON parse error:" << parseError.errorString();
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["errors"] = errors;
        result["categories"] = QVariant::fromValue(categories);
        result["images"] = QVariant::fromValue(imagesMap);
        result["annotations"] = QVariant::fromValue(annotationsMap);
        return result;
    }

    if (!doc.isObject()) {
        errors.append(QStringLiteral("JSON 根元素不是对象"));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: JSON root is not an object";
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["errors"] = errors;
        result["categories"] = QVariant::fromValue(categories);
        result["images"] = QVariant::fromValue(imagesMap);
        result["annotations"] = QVariant::fromValue(annotationsMap);
        return result;
    }

    QJsonObject root = doc.object();

    // 解析 categories 数组：提取 category_id -> name 映射
    if (root.contains(QStringLiteral("categories")) && root[QStringLiteral("categories")].isArray()) {
        QJsonArray catsArray = root[QStringLiteral("categories")].toArray();
        for (const QJsonValue &catVal : catsArray) {
            if (!catVal.isObject()) {
                errors.append(QStringLiteral("categories 中包含非对象元素"));
                continue;
            }
            QJsonObject catObj = catVal.toObject();
            int catId = catObj[QStringLiteral("id")].toInt(-1);
            QString catName = catObj[QStringLiteral("name")].toString();
            if (catId < 0) {
                errors.append(QStringLiteral("无效的 category_id: %1").arg(catId));
                continue;
            }
            categories[catId] = catName;
        }
    } else {
        errors.append(QStringLiteral("JSON 中缺少 categories 数组"));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: missing categories array";
    }

    // 解析 images 数组：提取 image_id -> {file_name, width, height} 映射
    if (root.contains(QStringLiteral("images")) && root[QStringLiteral("images")].isArray()) {
        QJsonArray imgsArray = root[QStringLiteral("images")].toArray();
        for (const QJsonValue &imgVal : imgsArray) {
            if (!imgVal.isObject()) {
                errors.append(QStringLiteral("images 中包含非对象元素"));
                continue;
            }
            QJsonObject imgObj = imgVal.toObject();
            int imgId = imgObj[QStringLiteral("id")].toInt(-1);
            if (imgId < 0) {
                errors.append(QStringLiteral("无效的 image_id: %1").arg(imgId));
                continue;
            }
            QVariantMap imgInfo;
            imgInfo["file_name"] = imgObj[QStringLiteral("file_name")].toString();
            imgInfo["width"] = imgObj[QStringLiteral("width")].toInt(0);
            imgInfo["height"] = imgObj[QStringLiteral("height")].toInt(0);
            imagesMap[imgId] = imgInfo;
        }
    } else {
        errors.append(QStringLiteral("JSON 中缺少 images 数组"));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: missing images array";
    }

    // 解析 annotations 数组：将 COCO bbox [x_min, y_min, w, h] 转换为 YOLO [cx, cy, w, h]（归一化）
    if (root.contains(QStringLiteral("annotations")) && root[QStringLiteral("annotations")].isArray()) {
        QJsonArray annsArray = root[QStringLiteral("annotations")].toArray();
        for (const QJsonValue &annVal : annsArray) {
            if (!annVal.isObject()) {
                errors.append(QStringLiteral("annotations 中包含非对象元素"));
                continue;
            }
            QJsonObject annObj = annVal.toObject();
            int imgId = annObj[QStringLiteral("image_id")].toInt(-1);
            int catId = annObj[QStringLiteral("category_id")].toInt(-1);

            if (imgId < 0 || catId < 0) {
                errors.append(QStringLiteral("无效的 annotation: image_id=%1, category_id=%2")
                                  .arg(imgId).arg(catId));
                continue;
            }

            // 获取该标注对应的图片尺寸，用于归一化
            if (!imagesMap.contains(imgId)) {
                errors.append(QStringLiteral("annotation 引用了不存在的 image_id: %1").arg(imgId));
                continue;
            }

            int imgWidth = imagesMap[imgId]["width"].toInt();
            int imgHeight = imagesMap[imgId]["height"].toInt();
            if (imgWidth <= 0 || imgHeight <= 0) {
                errors.append(QStringLiteral("image_id=%1 的尺寸无效: %2x%3")
                                  .arg(imgId).arg(imgWidth).arg(imgHeight));
                continue;
            }

            // 解析 COCO bbox: [x_min, y_min, width, height]（像素坐标）
            QJsonArray bbox = annObj[QStringLiteral("bbox")].toArray();
            if (bbox.size() != 4) {
                errors.append(QStringLiteral("annotation image_id=%1 的 bbox 格式错误，期望4个值，实际%2个")
                                  .arg(imgId).arg(bbox.size()));
                continue;
            }

            double xMin = bbox[0].toDouble(-1.0);
            double yMin = bbox[1].toDouble(-1.0);
            double bW = bbox[2].toDouble(-1.0);
            double bH = bbox[3].toDouble(-1.0);

            // 验证 bbox 值的合理性
            if (xMin < 0.0 || yMin < 0.0 || bW <= 0.0 || bH <= 0.0) {
                errors.append(QStringLiteral("annotation image_id=%1 的 bbox 值无效: [%2, %3, %4, %5]")
                                  .arg(imgId).arg(xMin).arg(yMin).arg(bW).arg(bH));
                continue;
            }

            // COCO bbox -> YOLO 格式（归一化到 0-1）
            double cx = (xMin + bW / 2.0) / static_cast<double>(imgWidth);
            double cy = (yMin + bH / 2.0) / static_cast<double>(imgHeight);
            double w = bW / static_cast<double>(imgWidth);
            double h = bH / static_cast<double>(imgHeight);

            // 检查归一化后的值是否在合理范围 [0, 1]（允许微小溢出）
            if (cx < 0.0 || cx > 1.0 || cy < 0.0 || cy > 1.0 || w <= 0.0 || w > 1.0 || h <= 0.0 || h > 1.0) {
                errors.append(QStringLiteral("annotation image_id=%1 归一化后坐标异常: cx=%2 cy=%3 w=%4 h=%5")
                                  .arg(imgId).arg(cx).arg(cy).arg(w).arg(h));
                continue;
            }

            QVariantMap annInfo;
            annInfo["category_id"] = catId;
            annInfo["cx"] = cx;
            annInfo["cy"] = cy;
            annInfo["w"] = w;
            annInfo["h"] = h;
            annotationsMap.insert(imgId, annInfo);

            classIds.insert(catId);
        }
    } else {
        errors.append(QStringLiteral("JSON 中缺少 annotations 数组"));
        ltWarning(LT_LOG_DATASET()) << "parseJsonLabelFile: missing annotations array";
    }

    // 判断是否有效：至少要有 images 和 annotations，且无严重解析错误
    bool valid = !imagesMap.isEmpty() && !annotationsMap.isEmpty();

    ltDebug(LT_LOG_DATASET()) << "parseJsonLabelFile: filePath=" << filePath
                              << "valid=" << valid
                              << "images=" << imagesMap.size()
                              << "annotations=" << annotationsMap.size()
                              << "categories=" << categories.size()
                              << "errors=" << errors.size();

    result["valid"] = valid;
    result["classIds"] = QVariant::fromValue(classIds);
    result["errors"] = errors;
    result["categories"] = QVariant::fromValue(categories);
    result["images"] = QVariant::fromValue(imagesMap);
    result["annotations"] = QVariant::fromValue(annotationsMap);
    return result;
}

QVariantMap ImportScanner::scanWithJsonLabels(const QString &imageDir, const QString &labelDir)
{
    ltTrace(LT_LOG_DATASET()) << "scanWithJsonLabels imageDir=" << imageDir << "labelDir=" << labelDir;

    QVariantMap result;
    QVariantList samples;

    QDir imgDir(imageDir);
    QDir lblDir(labelDir);

    // 递归收集图片文件，按文件名建立索引（用于 JSON 中的 file_name 匹配）
    QFileInfoList imageFiles = collectImageFiles(imgDir, true);
    QMap<QString, QFileInfo> imageByFileName;
    QMap<QString, QFileInfo> imageByBaseFileName;
    for (const auto &fi : imageFiles) {
        imageByFileName[fi.fileName()] = fi;
        imageByBaseFileName[fi.fileName()] = fi;
    }

    // 递归查找标签目录中的所有 JSON 文件
    QFileInfoList jsonFiles;
    QFileInfoList allLabelFiles = collectLabelFiles(lblDir, true);
    for (const auto &fi : allLabelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            jsonFiles.append(fi);
        }
    }

    if (jsonFiles.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "scanWithJsonLabels: no JSON files found in labelDir";
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["error"] = QStringLiteral("标签目录中未找到 JSON 文件: %1").arg(labelDir);
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
        QVariantMap parseResult = parseJsonLabelFile(jsonFi.absoluteFilePath());

        if (!parseResult["valid"].toBool()) {
            QStringList fileErrors = parseResult["errors"].toStringList();
            for (const auto &err : fileErrors) {
                allErrors.append(QStringLiteral("[%1] %2").arg(jsonFi.fileName(), err));
            }
            continue;
        }

        anyValid = true;

        // 合并 images
        QMap<int, QVariantMap> imagesMap = parseResult["images"].value<QMap<int, QVariantMap>>();
        for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
            allImagesMap[it.key()] = it.value();
        }

        // 合并 annotations
        QMultiMap<int, QVariantMap> annotationsMap = parseResult["annotations"].value<QMultiMap<int, QVariantMap>>();
        for (auto it = annotationsMap.constBegin(); it != annotationsMap.constEnd(); ++it) {
            allAnnotationsMap.insert(it.key(), it.value());
        }

        // 合并 categories
        QMap<int, QString> categories = parseResult["categories"].value<QMap<int, QString>>();
        for (auto it = categories.constBegin(); it != categories.constEnd(); ++it) {
            allCategories[it.key()] = it.value();
        }

        // 合并 classIds
        QSet<int> classIds = parseResult["classIds"].value<QSet<int>>();
        for (int cid : classIds) {
            allClassIds.insert(cid);
        }
    }

    if (!anyValid) {
        ltWarning(LT_LOG_DATASET()) << "scanWithJsonLabels: all JSON files are invalid";
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["errors"] = allErrors;
        result["error"] = QStringLiteral("所有 JSON 标签文件均无效");
        return result;
    }

    // 按 image_id 遍历，匹配图片文件
    int matched = 0;
    int unmatchedImages = 0;
    int processed = 0;
    int totalEntries = allImagesMap.size() + imageByFileName.size();

    // 记录已匹配的图片文件名，用于后续统计未匹配图片
    QSet<QString> matchedImageFiles;

    for (auto it = allImagesMap.constBegin(); it != allImagesMap.constEnd(); ++it) {
        processed++;
        emit scanProgress(processed, totalEntries);

        int imgId = it.key();
        QVariantMap imgInfo = it.value();
        QString fileName = imgInfo["file_name"].toString();

        QVariantMap sample;
        sample["imageId"] = imgId;

        // 在图片目录中查找匹配的文件（按 file_name 匹配）
        // COCO JSON 的 file_name 可能是纯文件名 "img001.jpg" 或含子目录 "train/img001.jpg"
        QFileInfo matchedImgFi;
        QString matchKey;
        if (imageByFileName.contains(fileName)) {
            // 精确匹配：file_name 与图片文件名一致
            matchedImgFi = imageByFileName[fileName];
            matchKey = fileName;
        } else {
            // 回退匹配：file_name 含子目录路径时，提取纯文件名匹配
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

            // 收集该图片的所有标注
            QList<QVariantMap> imgAnnotations;
            auto range = allAnnotationsMap.equal_range(imgId);
            for (auto annIt = range.first; annIt != range.second; ++annIt) {
                imgAnnotations.append(annIt.value());
            }

            // 收集该图片的 classIds
            QSet<int> imgClassIds;
            for (const auto &ann : imgAnnotations) {
                imgClassIds.insert(ann["category_id"].toInt());
            }

            QVariantList classIdList;
            for (int cid : imgClassIds) {
                classIdList.append(cid);
            }

            // 将标注列表转换为 QVariantList 存储
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
            // JSON 中有该图片记录，但图片目录中找不到对应文件
            sample["status"] = QStringLiteral("unmatched_label");
            sample["valid"] = false;
            sample["fileName"] = fileName;
            unmatchedImages++;
        }

        samples.append(sample);
    }

    // 统计图片目录中未被 JSON 匹配的图片
    int unmatchedLabels = 0;
    for (auto imgIt = imageByFileName.constBegin(); imgIt != imageByFileName.constEnd(); ++imgIt) {
        if (!matchedImageFiles.contains(imgIt.key())) {
            processed++;
            emit scanProgress(processed, totalEntries);

            QVariantMap sample;
            sample["imagePath"] = imgIt.value().absoluteFilePath();
            sample["stem"] = imgIt.value().completeBaseName();
            sample["status"] = QStringLiteral("unmatched_image");
            sample["valid"] = false;
            samples.append(sample);
            unmatchedLabels++;
        }
    }

    // 构建 categories 的 QVariantMap（category_id -> name）
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

    // 如果有解析错误，附加到结果中
    if (!allErrors.isEmpty()) {
        result["parseErrors"] = allErrors;
    }

    ltInfo(LT_LOG_DATASET()) << "scanWithJsonLabels completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels
                             << "categories:" << allCategories.size();

    return result;
}

QVariantMap ImportScanner::scanFolder(const QString &folderPath)
{
    ltInfo(LT_LOG_DATASET()) << "scanFolder folderPath=" << folderPath;

    QVariantMap result;
    result["isValid"] = false;
    result["detectedFormat"] = QString();
    result["imageDir"] = QString();
    result["labelDirOrPath"] = QString();
    result["imageCount"] = 0;
    result["labelCount"] = 0;
    result["unmatchedImagesCount"] = 0;
    result["classIds"] = QVariantList();
    result["classes"] = QVariantMap();

    if (folderPath.isEmpty()) {
        result["error"] = QStringLiteral("文件夹路径为空");
        return result;
    }

    QDir rootDir(folderPath);
    if (!rootDir.exists()) {
        result["error"] = QStringLiteral("文件夹不存在: %1").arg(folderPath);
        return result;
    }

    // 按优先级依次探测：Anomalib → 嵌套 YOLO → 扁平结构
    QVariantMap anomalibResult = detectAnomalibLayout(folderPath);
    if (anomalibResult["isValid"].toBool()) {
        ltInfo(LT_LOG_DATASET()) << "scanFolder: detected Anomalib layout";
        return anomalibResult;
    }

    QVariantMap nestedResult = detectNestedYoloLayout(folderPath);
    if (nestedResult["isValid"].toBool()) {
        ltInfo(LT_LOG_DATASET()) << "scanFolder: detected nested YOLO layout";
        return nestedResult;
    }

    QVariantMap flatResult = detectFlatLayout(folderPath);
    if (flatResult["isValid"].toBool()) {
        ltInfo(LT_LOG_DATASET()) << "scanFolder: detected flat layout";
        return flatResult;
    }

    // 所有探测均失败
    result["error"] = QStringLiteral("未在当前目录下探测到符合规范的图片与标签文件，请检查目录结构。");
    ltWarning(LT_LOG_DATASET()) << "scanFolder: no valid dataset layout detected in" << folderPath;
    return result;
}

QVariantMap ImportScanner::detectAnomalibLayout(const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "detectAnomalibLayout folderPath=" << folderPath;

    QVariantMap result;
    result["isValid"] = false;

    QDir rootDir(folderPath);

    // 检查 train/good 目录是否存在
    QString trainGoodPath = folderPath + QStringLiteral("/train/good");
    if (!QDir(trainGoodPath).exists()) {
        return result;
    }

    // 统计各子目录中的图片数量
    int trainGoodCount = collectImageFiles(QDir(trainGoodPath), true).size();
    int testGoodCount = 0;
    int testDefectiveCount = 0;

    QString testGoodPath = folderPath + QStringLiteral("/test/good");
    QString testDefectivePath = folderPath + QStringLiteral("/test/defective");

    if (QDir(testGoodPath).exists()) {
        testGoodCount = collectImageFiles(QDir(testGoodPath), true).size();
    }
    if (QDir(testDefectivePath).exists()) {
        testDefectiveCount = collectImageFiles(QDir(testDefectivePath), true).size();
    }

    int totalImages = trainGoodCount + testGoodCount + testDefectiveCount;
    if (totalImages == 0) {
        return result;
    }

    result["isValid"] = true;
    result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
    result["imageDir"] = folderPath;
    result["labelDirOrPath"] = QString();
    result["imageCount"] = totalImages;
    result["labelCount"] = 0;
    result["unmatchedImagesCount"] = 0;
    result["classIds"] = QVariantList();
    result["classes"] = QVariantMap();

    // 额外提供异常检测结构的统计信息
    QVariantMap layoutStats;
    layoutStats["trainGood"] = trainGoodCount;
    layoutStats["testGood"] = testGoodCount;
    layoutStats["testDefective"] = testDefectiveCount;
    result["layoutStats"] = layoutStats;

    ltInfo(LT_LOG_DATASET()) << "Anomalib layout detected: train/good=" << trainGoodCount
                             << "test/good=" << testGoodCount
                             << "test/defective=" << testDefectiveCount;
    return result;
}

QVariantMap ImportScanner::detectNestedYoloLayout(const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "detectNestedYoloLayout folderPath=" << folderPath;

    QVariantMap result;
    result["isValid"] = false;

    QDir rootDir(folderPath);

    // 检查 images/ 和 labels/ 子目录是否存在
    QString imagesPath = folderPath + QStringLiteral("/images");
    QString labelsPath = folderPath + QStringLiteral("/labels");

    bool hasImagesDir = QDir(imagesPath).exists();
    bool hasLabelsDir = QDir(labelsPath).exists();

    if (!hasImagesDir && !hasLabelsDir) {
        return result;
    }

    // 收集 images/ 下所有图片（递归搜索 train/val 子目录）
    QFileInfoList imageFiles = collectImageFiles(QDir(imagesPath), true);
    if (imageFiles.isEmpty()) {
        return result;
    }

    // 收集 labels/ 下所有标签文件
    QFileInfoList labelFiles;
    int labelCount = 0;
    QSet<int> classIds;

    if (hasLabelsDir) {
        labelFiles = collectLabelFiles(QDir(labelsPath), true);

        // 检查是否有 JSON 标签
        bool hasJsonLabels = false;
        bool hasLabelMeJson = false;
        QString jsonLabelPath;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                hasJsonLabels = true;
                if (isLabelMeJsonFile(fi.absoluteFilePath())) {
                    hasLabelMeJson = true;
                }
                if (jsonLabelPath.isEmpty()) {
                    jsonLabelPath = fi.absoluteFilePath();
                }
            }
        }

        if (hasJsonLabels) {
            if (hasLabelMeJson) {
                // LabelMe JSON 格式（每图一个 JSON 文件）
                result["detectedFormat"] = QStringLiteral("labelme_json");
                result["labelDirOrPath"] = labelsPath;

                // 解析所有 LabelMe JSON 文件获取统计信息
                int labelCount = 0;
                QSet<int> classIds;
                QMap<QString, int> labelToClassId;
                int nextClassId = 0;

                for (const auto &fi : labelFiles) {
                    if (fi.suffix().toLower() != QStringLiteral("json")) continue;
                    if (!isLabelMeJsonFile(fi.absoluteFilePath())) continue;

                    QVariantMap parseResult = parseLabelMeJsonFile(fi.absoluteFilePath());
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

                for (int cid : labelToClassId.values()) {
                    classIds.insert(cid);
                }

                QVariantList classIdList;
                QList<int> sortedIds = classIds.values();
                std::sort(sortedIds.begin(), sortedIds.end());
                for (int cid : sortedIds) {
                    classIdList.append(cid);
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
                    if (!isLabelMeJsonFile(fi.absoluteFilePath())) continue;
                    QString stem = fi.completeBaseName();
                    if (!imageByStem.contains(stem)) {
                        unmatchedImages++;
                    }
                }

                result["isValid"] = true;
                result["imageDir"] = imagesPath;
                result["imageCount"] = imageFiles.size();
                result["labelCount"] = labelCount;
                result["unmatchedImagesCount"] = unmatchedImages;
                result["classIds"] = classIdList;
                result["classes"] = categoriesMap;

                ltInfo(LT_LOG_DATASET()) << "Nested LabelMe JSON layout detected: images=" << imageFiles.size()
                                         << "labeled=" << labelCount
                                         << "classes=" << classIdList.size();
                return result;
            }

            // COCO JSON 格式
            result["detectedFormat"] = QStringLiteral("coco_json");
            result["labelDirOrPath"] = jsonLabelPath;

            QVariantMap jsonResult = parseJsonLabelFile(jsonLabelPath);
            if (jsonResult["valid"].toBool()) {
                QSet<int> jsonClassIds = jsonResult["classIds"].value<QSet<int>>();
                QVariantMap categories = jsonResult["categories"].toMap();
                QMap<int, QVariantMap> imagesMap = jsonResult["images"].value<QMap<int, QVariantMap>>();
                QMultiMap<int, QVariantMap> annotationsMap = jsonResult["annotations"].value<QMultiMap<int, QVariantMap>>();

                // 建立图片文件名集合（纯文件名），用于匹配 JSON 中的 file_name
                QSet<QString> imageFileNames;
                for (const auto &fi : imageFiles) {
                    imageFileNames.insert(fi.fileName());
                }

                int labelCount = 0;
                int unmatchedImages = 0;
                QSet<QString> matchedImageFiles;

                for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
                    QString fileName = it.value()["file_name"].toString();
                    // 支持纯文件名和含子目录路径的 file_name
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

                QVariantList classIdList;
                QList<int> sortedIds = jsonClassIds.values();
                std::sort(sortedIds.begin(), sortedIds.end());
                for (int cid : sortedIds) {
                    classIdList.append(cid);
                }

                result["isValid"] = true;
                result["imageDir"] = imagesPath;
                result["imageCount"] = imageFiles.size();
                result["labelCount"] = labelCount;
                result["unmatchedImagesCount"] = unmatchedImages;
                result["classIds"] = classIdList;
                result["classes"] = categories;

                ltInfo(LT_LOG_DATASET()) << "Nested COCO JSON layout detected and parsed successfully: images=" << imageFiles.size()
                                         << "labeled=" << labelCount
                                         << "classes=" << classIdList.size();
                return result;
            } else {
                result["isValid"] = true;
                result["imageDir"] = imagesPath;
                result["imageCount"] = imageFiles.size();
                result["labelCount"] = 0;
                result["unmatchedImagesCount"] = imageFiles.size();
                result["classIds"] = QVariantList();
                result["classes"] = QVariantMap();
                return result;
            }
        } else {
            // YOLO TXT 格式
            result["detectedFormat"] = QStringLiteral("yolo_txt");
            result["labelDirOrPath"] = labelsPath;

            // 提取类别 ID
            for (const auto &fi : labelFiles) {
                if (fi.suffix().toLower() == QStringLiteral("txt")) {
                    QSet<int> fileClassIds;
                    QStringList errors;
                    parseLabelFile(fi.absoluteFilePath(), fileClassIds, errors);
                    classIds.unite(fileClassIds);
                    labelCount++;
                }
            }
        }
    } else {
        // 仅有 images 目录，无标签，可能是异常检测正常集
        result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
        result["labelDirOrPath"] = QString();
    }

    // 按文件名 stem 匹配图片和标签
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

    QVariantList classIdList;
    QList<int> sortedIds = classIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classIdList.append(cid);
    }

    result["isValid"] = true;
    result["imageDir"] = imagesPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelByStem.size();
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = classIdList;

    ltInfo(LT_LOG_DATASET()) << "Nested YOLO layout detected: images=" << imageFiles.size()
                             << "labels=" << labelByStem.size()
                             << "format=" << result["detectedFormat"].toString();
    return result;
}

QVariantMap ImportScanner::detectFlatLayout(const QString &folderPath)
{
    ltTrace(LT_LOG_DATASET()) << "detectFlatLayout folderPath=" << folderPath;

    QVariantMap result;
    result["isValid"] = false;

    QDir rootDir(folderPath);

    // 收集根目录下所有图片和标签文件
    QFileInfoList imageFiles = collectImageFiles(rootDir, false);
    QFileInfoList labelFiles = collectLabelFiles(rootDir, false);

    if (imageFiles.isEmpty()) {
        return result;
    }

    // 检查是否有 JSON 标签
    bool hasJsonLabels = false;
    bool hasLabelMeJson = false;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            hasJsonLabels = true;
            if (isLabelMeJsonFile(fi.absoluteFilePath())) {
                hasLabelMeJson = true;
            }
            break;
        }
    }

    QSet<int> classIds;
    int labelCount = 0;

    if (hasJsonLabels) {
        if (hasLabelMeJson) {
            // LabelMe JSON 格式（每图一个 JSON 文件）
            result["detectedFormat"] = QStringLiteral("labelme_json");
            result["labelDirOrPath"] = folderPath;

            QMap<QString, int> labelToClassId;
            int nextClassId = 0;

            for (const auto &fi : labelFiles) {
                if (fi.suffix().toLower() != QStringLiteral("json")) continue;
                if (!isLabelMeJsonFile(fi.absoluteFilePath())) continue;

                QVariantMap parseResult = parseLabelMeJsonFile(fi.absoluteFilePath());
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

            for (int cid : labelToClassId.values()) {
                classIds.insert(cid);
            }

            QVariantList classIdList;
            QList<int> sortedIds = classIds.values();
            std::sort(sortedIds.begin(), sortedIds.end());
            for (int cid : sortedIds) {
                classIdList.append(cid);
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
                if (!isLabelMeJsonFile(fi.absoluteFilePath())) continue;
                QString stem = fi.completeBaseName();
                if (!imageByStem.contains(stem)) {
                    unmatchedImages++;
                }
            }

            result["isValid"] = true;
            result["imageDir"] = folderPath;
            result["imageCount"] = imageFiles.size();
            result["labelCount"] = labelCount;
            result["unmatchedImagesCount"] = unmatchedImages;
            result["classIds"] = classIdList;
            result["classes"] = categoriesMap;

            ltInfo(LT_LOG_DATASET()) << "Flat LabelMe JSON layout detected: images=" << imageFiles.size()
                                     << "labeled=" << labelCount
                                     << "classes=" << classIdList.size();
            return result;
        }

        // COCO JSON 格式
        // 查找第一个非 LabelMe 的 JSON 标签文件
        QString jsonLabelPath;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                if (!isLabelMeJsonFile(fi.absoluteFilePath())) {
                    jsonLabelPath = fi.absoluteFilePath();
                    break;
                }
            }
        }

        if (jsonLabelPath.isEmpty()) {
            // 所有 JSON 都是 LabelMe 格式但上面没匹配到（不应该到这里）
            result["detectedFormat"] = QStringLiteral("labelme_json");
            result["labelDirOrPath"] = folderPath;
            result["isValid"] = true;
            result["imageDir"] = folderPath;
            result["imageCount"] = imageFiles.size();
            result["labelCount"] = 0;
            result["unmatchedImagesCount"] = imageFiles.size();
            result["classIds"] = QVariantList();
            result["classes"] = QVariantMap();
            return result;
        }

        result["detectedFormat"] = QStringLiteral("coco_json");
        result["labelDirOrPath"] = jsonLabelPath;

        QVariantMap jsonResult = parseJsonLabelFile(jsonLabelPath);
        if (jsonResult["valid"].toBool()) {
            QSet<int> jsonClassIds = jsonResult["classIds"].value<QSet<int>>();
            QVariantMap categories = jsonResult["categories"].toMap();
            QMap<int, QVariantMap> imagesMap = jsonResult["images"].value<QMap<int, QVariantMap>>();
            QMultiMap<int, QVariantMap> annotationsMap = jsonResult["annotations"].value<QMultiMap<int, QVariantMap>>();

            // 建立图片文件名集合（纯文件名），用于匹配 JSON 中的 file_name
            QSet<QString> imageFileNames;
            for (const auto &fi : imageFiles) {
                imageFileNames.insert(fi.fileName());
            }

            int labelCount = 0;
            int unmatchedImages = 0;
            QSet<QString> matchedImageFiles;

            for (auto it = imagesMap.constBegin(); it != imagesMap.constEnd(); ++it) {
                QString fileName = it.value()["file_name"].toString();
                // 支持纯文件名和含子目录路径的 file_name
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

            QVariantList classIdList;
            QList<int> sortedIds = jsonClassIds.values();
            std::sort(sortedIds.begin(), sortedIds.end());
            for (int cid : sortedIds) {
                classIdList.append(cid);
            }

            result["isValid"] = true;
            result["imageDir"] = folderPath;
            result["imageCount"] = imageFiles.size();
            result["labelCount"] = labelCount;
            result["unmatchedImagesCount"] = unmatchedImages;
            result["classIds"] = classIdList;
            result["classes"] = categories;

            ltInfo(LT_LOG_DATASET()) << "Flat COCO JSON layout detected and parsed successfully: images=" << imageFiles.size()
                                     << "labeled=" << labelCount
                                     << "classes=" << classIdList.size();
            return result;
        } else {
            result["isValid"] = true;
            result["imageDir"] = folderPath;
            result["imageCount"] = imageFiles.size();
            result["labelCount"] = 0;
            result["unmatchedImagesCount"] = imageFiles.size();
            result["classIds"] = QVariantList();
            result["classes"] = QVariantMap();
            return result;
        }
    } else {
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

            // 提取类别 ID
            for (const auto &fi : labelFiles) {
                if (fi.suffix().toLower() == QStringLiteral("txt")) {
                    QSet<int> fileClassIds;
                    QStringList errors;
                    parseLabelFile(fi.absoluteFilePath(), fileClassIds, errors);
                    classIds.unite(fileClassIds);
                    labelCount++;
                }
            }
        } else {
            // 仅有图片，无标签，判定为异常检测单分类正常集
            result["detectedFormat"] = QStringLiteral("anomaly_unsupervised");
            result["labelDirOrPath"] = QString();
        }
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

    QVariantList classIdList;
    QList<int> sortedIds = classIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classIdList.append(cid);
    }

    result["isValid"] = true;
    result["imageDir"] = folderPath;
    result["imageCount"] = imageFiles.size();
    result["labelCount"] = labelByStem.size();
    result["unmatchedImagesCount"] = unmatchedImages;
    result["classIds"] = classIdList;

    ltInfo(LT_LOG_DATASET()) << "Flat layout detected: images=" << imageFiles.size()
                             << "labels=" << labelByStem.size()
                             << "format=" << result["detectedFormat"].toString();
    return result;
}

QFileInfoList ImportScanner::collectImageFiles(const QDir &dir, bool recursive)
{
    QFileInfoList result;
    if (!dir.exists()) return result;

    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &fi : entries) {
        if (isImageFile(fi.fileName())) {
            result.append(fi);
        }
    }

    if (recursive) {
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &subDir : subDirs) {
            result.append(collectImageFiles(QDir(subDir.absoluteFilePath()), true));
        }
    }

    return result;
}

QFileInfoList ImportScanner::collectLabelFiles(const QDir &dir, bool recursive)
{
    QFileInfoList result;
    if (!dir.exists()) return result;

    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto &fi : entries) {
        if (isLabelFile(fi.fileName())) {
            result.append(fi);
        }
    }

    if (recursive) {
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &subDir : subDirs) {
            result.append(collectLabelFiles(QDir(subDir.absoluteFilePath()), true));
        }
    }

    return result;
}

bool ImportScanner::isLabelMeJsonFile(const QString &filePath)
{
    ltTrace(LT_LOG_DATASET()) << "isLabelMeJsonFile filePath=" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    // LabelMe 格式特征：包含 "shapes" 数组和 "imagePath" 字符串
    return root.contains(QStringLiteral("shapes"))
           && root[QStringLiteral("shapes")].isArray()
           && root.contains(QStringLiteral("imagePath"));
}

QVariantMap ImportScanner::parseLabelMeJsonFile(const QString &filePath)
{
    ltTrace(LT_LOG_DATASET()) << "parseLabelMeJsonFile filePath=" << filePath;

    QVariantMap result;
    QStringList errors;
    QSet<int> classIds;
    QVariantList shapesList;
    QMap<QString, int> labelToClassId;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors.append(QStringLiteral("无法打开文件: %1").arg(filePath));
        ltWarning(LT_LOG_DATASET()) << "parseLabelMeJsonFile: cannot open file:" << filePath;
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["shapes"] = shapesList;
        result["errors"] = errors;
        return result;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errors.append(QStringLiteral("JSON 解析错误: %1").arg(parseError.errorString()));
        ltWarning(LT_LOG_DATASET()) << "parseLabelMeJsonFile: JSON parse error:" << parseError.errorString();
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["shapes"] = shapesList;
        result["errors"] = errors;
        return result;
    }

    if (!doc.isObject()) {
        errors.append(QStringLiteral("JSON 根元素不是对象"));
        ltWarning(LT_LOG_DATASET()) << "parseLabelMeJsonFile: JSON root is not an object";
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["shapes"] = shapesList;
        result["errors"] = errors;
        return result;
    }

    QJsonObject root = doc.object();

    // 提取图片信息
    QString imagePath = root[QStringLiteral("imagePath")].toString();
    int imageWidth = root[QStringLiteral("imageWidth")].toInt(0);
    int imageHeight = root[QStringLiteral("imageHeight")].toInt(0);

    if (imageWidth <= 0 || imageHeight <= 0) {
        QFileInfo jsonFi(filePath);
        // Try relative to JSON directory
        QString absImgPath = jsonFi.dir().absoluteFilePath(imagePath);
        if (!QFile::exists(absImgPath)) {
            // Try with JSON base name in same directory
            QString baseName = jsonFi.completeBaseName();
            QDir jsonDir = jsonFi.dir();
            QStringList filters;
            filters << baseName + ".jpg" << baseName + ".png" << baseName + ".jpeg" << baseName + ".bmp";
            QFileInfoList list = jsonDir.entryInfoList(filters, QDir::Files);
            if (!list.isEmpty()) {
                absImgPath = list.first().absoluteFilePath();
            }
        }
        if (QFile::exists(absImgPath)) {
            QImageReader reader(absImgPath);
            QSize size = reader.size();
            if (size.isValid()) {
                imageWidth = size.width();
                imageHeight = size.height();
            }
        }
    }

    result["imagePath"] = imagePath;
    result["imageWidth"] = imageWidth;
    result["imageHeight"] = imageHeight;

    // 解析 shapes 数组
    if (!root.contains(QStringLiteral("shapes")) || !root[QStringLiteral("shapes")].isArray()) {
        errors.append(QStringLiteral("JSON 中缺少 shapes 数组"));
        ltWarning(LT_LOG_DATASET()) << "parseLabelMeJsonFile: missing shapes array";
        result["valid"] = false;
        result["classIds"] = QVariant::fromValue(classIds);
        result["shapes"] = shapesList;
        result["errors"] = errors;
        return result;
    }

    QJsonArray shapesArray = root[QStringLiteral("shapes")].toArray();
    int nextClassId = 0;

    for (const QJsonValue &shapeVal : shapesArray) {
        if (!shapeVal.isObject()) {
            errors.append(QStringLiteral("shapes 中包含非对象元素"));
            continue;
        }

        QJsonObject shapeObj = shapeVal.toObject();
        QString label = shapeObj[QStringLiteral("label")].toString();
        QString shapeType = shapeObj[QStringLiteral("shape_type")].toString();
        QJsonArray pointsArray = shapeObj[QStringLiteral("points")].toArray();

        // 为每个唯一 label 分配递增的 classId
        if (!labelToClassId.contains(label)) {
            labelToClassId[label] = nextClassId++;
        }
        int classId = labelToClassId[label];
        classIds.insert(classId);

        QVariantMap shapeInfo;
        shapeInfo["label"] = label;
        shapeInfo["classId"] = classId;
        shapeInfo["shapeType"] = shapeType;

        // 解析 points 数组
        QVariantList pointsList;
        for (const QJsonValue &ptVal : pointsArray) {
            if (ptVal.isArray()) {
                QJsonArray ptArray = ptVal.toArray();
                if (ptArray.size() >= 2) {
                    QVariantMap pt;
                    pt["x"] = ptArray[0].toDouble(0.0);
                    pt["y"] = ptArray[1].toDouble(0.0);
                    pointsList.append(pt);
                }
            }
        }
        shapeInfo["points"] = pointsList;

        // 将 LabelMe 坐标转换为 YOLO 归一化格式
        if (imageWidth > 0 && imageHeight > 0 && pointsList.size() >= 2) {
            double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            bool coordsOk = false;

            if (shapeType == QStringLiteral("rectangle") && pointsList.size() == 2) {
                x1 = pointsList[0].toMap()["x"].toDouble();
                y1 = pointsList[0].toMap()["y"].toDouble();
                x2 = pointsList[1].toMap()["x"].toDouble();
                y2 = pointsList[1].toMap()["y"].toDouble();
                coordsOk = true;
            } else if (shapeType == QStringLiteral("polygon") && pointsList.size() >= 3) {
                double minX = pointsList[0].toMap()["x"].toDouble();
                double maxX = minX;
                double minY = pointsList[0].toMap()["y"].toDouble();
                double maxY = minY;

                for (int i = 1; i < pointsList.size(); ++i) {
                    double px = pointsList[i].toMap()["x"].toDouble();
                    double py = pointsList[i].toMap()["y"].toDouble();
                    if (px < minX) minX = px;
                    if (px > maxX) maxX = px;
                    if (py < minY) minY = py;
                    if (py > maxY) maxY = py;
                }
                x1 = minX;
                y1 = minY;
                x2 = maxX;
                y2 = maxY;
                coordsOk = true;
            }

            if (coordsOk) {
                double bW = qAbs(x2 - x1);
                double bH = qAbs(y2 - y1);

                if (bW > 0 && bH > 0) {
                    double cx = ((x1 + x2) / 2.0) / static_cast<double>(imageWidth);
                    double cy = ((y1 + y2) / 2.0) / static_cast<double>(imageHeight);
                    double w = bW / static_cast<double>(imageWidth);
                    double h = bH / static_cast<double>(imageHeight);

                    shapeInfo["cx"] = cx;
                    shapeInfo["cy"] = cy;
                    shapeInfo["w"] = w;
                    shapeInfo["h"] = h;
                }
            }
        }

        shapesList.append(shapeInfo);
    }

    bool valid = !shapesList.isEmpty();

    // 构建 label -> classId 映射
    QVariantMap labelMap;
    for (auto it = labelToClassId.constBegin(); it != labelToClassId.constEnd(); ++it) {
        labelMap[it.key()] = it.value();
    }

    result["valid"] = valid;
    result["classIds"] = QVariant::fromValue(classIds);
    result["shapes"] = shapesList;
    result["labelToClassId"] = labelMap;
    result["errors"] = errors;

    ltDebug(LT_LOG_DATASET()) << "parseLabelMeJsonFile: filePath=" << filePath
                              << "valid=" << valid
                              << "shapes=" << shapesList.size()
                              << "labels=" << labelToClassId.size()
                              << "errors=" << errors.size();

    return result;
}

QVariantMap ImportScanner::scanWithLabelMeJsonLabels(const QString &imageDir, const QString &labelDir)
{
    ltInfo(LT_LOG_DATASET()) << "scanWithLabelMeJsonLabels imageDir=" << imageDir << "labelDir=" << labelDir;

    QVariantMap result;
    QVariantList samples;

    QDir imgDir(imageDir);
    QDir lblDir(labelDir);

    // 递归收集图片文件，按文件名建立索引
    QFileInfoList imageFiles = collectImageFiles(imgDir, true);
    QMap<QString, QFileInfo> imageByFileName;
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        imageByFileName[fi.fileName()] = fi;
        imageByStem[fi.completeBaseName()] = fi;
    }

    // 递归查找标签目录中的所有 LabelMe JSON 文件
    QFileInfoList jsonFiles;
    QFileInfoList allLabelFiles = collectLabelFiles(lblDir, true);
    for (const auto &fi : allLabelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json") && isLabelMeJsonFile(fi.absoluteFilePath())) {
            jsonFiles.append(fi);
        }
    }

    if (jsonFiles.isEmpty()) {
        ltWarning(LT_LOG_DATASET()) << "scanWithLabelMeJsonLabels: no LabelMe JSON files found in labelDir";
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["error"] = QStringLiteral("标签目录中未找到 LabelMe JSON 文件: %1").arg(labelDir);
        return result;
    }

    // 合并所有 JSON 文件的解析结果
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
        emit scanProgress(processed, totalEntries);

        QVariantMap parseResult = parseLabelMeJsonFile(jsonFi.absoluteFilePath());

        if (!parseResult["valid"].toBool()) {
            QStringList fileErrors = parseResult["errors"].toStringList();
            for (const auto &err : fileErrors) {
                allErrors.append(QStringLiteral("[%1] %2").arg(jsonFi.fileName(), err));
            }
            continue;
        }

        // 合并 label -> classId 映射
        QVariantMap labelMap = parseResult["labelToClassId"].toMap();
        for (auto it = labelMap.constBegin(); it != labelMap.constEnd(); ++it) {
            QString label = it.key();
            if (!globalLabelToClassId.contains(label)) {
                globalLabelToClassId[label] = nextGlobalClassId++;
            }
        }

        // 获取该 JSON 文件对应的图片路径
        QString jsonImagePath = parseResult["imagePath"].toString();
        int imageWidth = parseResult["imageWidth"].toInt();
        int imageHeight = parseResult["imageHeight"].toInt();
        QVariantList shapes = parseResult["shapes"].toList();

        // 匹配图片文件：优先按 imagePath 匹配，其次按 JSON 文件名 stem 匹配
        QFileInfo matchedImgFi;
        QString matchKey;

        if (!jsonImagePath.isEmpty()) {
            // 从 imagePath 提取纯文件名
            QString baseFileName = QFileInfo(jsonImagePath).fileName();
            if (imageByFileName.contains(baseFileName)) {
                matchedImgFi = imageByFileName[baseFileName];
                matchKey = baseFileName;
            }
        }

        // 回退：按 JSON 文件名 stem 匹配图片
        if (!matchedImgFi.exists()) {
            QString jsonStem = jsonFi.completeBaseName();
            if (imageByStem.contains(jsonStem)) {
                matchedImgFi = imageByStem[jsonStem];
                matchKey = matchedImgFi.fileName();
            }
        }

        QVariantMap sample;
        sample["labelPath"] = jsonFi.absoluteFilePath();

        // 重新映射 shapes 中的 classId 为全局 classId
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

    // 统计图片目录中未被 JSON 匹配的图片
    int unmatchedLabels = 0;
    for (auto imgIt = imageByFileName.constBegin(); imgIt != imageByFileName.constEnd(); ++imgIt) {
        if (!matchedImageFiles.contains(imgIt.key())) {
            processed++;
            emit scanProgress(processed, totalEntries);

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

    // 构建 categories 映射（classId -> label）
    QVariantMap categoriesMap;
    for (auto it = globalLabelToClassId.constBegin(); it != globalLabelToClassId.constEnd(); ++it) {
        categoriesMap[QString::number(it.value())] = it.key();
    }

    QVariantList classIdList;
    QList<int> sortedIds = allClassIds.values();
    std::sort(sortedIds.begin(), sortedIds.end());
    for (int cid : sortedIds) {
        classIdList.append(cid);
    }

    result["total"] = jsonFiles.size() + imageFiles.size();
    result["matched"] = matched;
    result["unmatchedImages"] = unmatchedImages;
    result["unmatchedLabels"] = unmatchedLabels;
    result["samples"] = samples;
    result["categories"] = categoriesMap;
    result["classIds"] = classIdList;

    if (!allErrors.isEmpty()) {
        result["parseErrors"] = allErrors;
    }

    ltInfo(LT_LOG_DATASET()) << "scanWithLabelMeJsonLabels completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels
                             << "categories:" << globalLabelToClassId.size();

    return result;
}
