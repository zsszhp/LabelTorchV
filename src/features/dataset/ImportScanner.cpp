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
    QDir lblDir(labelDir);

    if (!imgDir.exists()) {
        ltError(LT_LOG_DATASET()) << "Image directory does not exist:" << imageDir;
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["error"] = QStringLiteral("Image directory does not exist: %1").arg(imageDir);
        emit scanCompleted();
        return result;
    }

    if (!lblDir.exists()) {
        ltError(LT_LOG_DATASET()) << "Label directory does not exist:" << labelDir;
        result["total"] = 0;
        result["matched"] = 0;
        result["unmatchedImages"] = 0;
        result["unmatchedLabels"] = 0;
        result["samples"] = samples;
        result["error"] = QStringLiteral("Label directory does not exist: %1").arg(labelDir);
        emit scanCompleted();
        return result;
    }

    ltInfo(LT_LOG_DATASET()) << "Scan start: imageDir=" << imageDir << "labelDir=" << labelDir;

    // 检测标签目录中是否存在 JSON 文件，优先使用 JSON 格式
    QFileInfoList labelFiles = lblDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    bool hasJsonLabels = false;
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            hasJsonLabels = true;
            break;
        }
    }

    // 如果存在 JSON 标签文件，优先使用 JSON 扫描流程
    if (hasJsonLabels) {
        ltInfo(LT_LOG_DATASET()) << "Detected JSON label files, using COCO JSON scan flow";
        QVariantMap jsonResult = scanWithJsonLabels(imageDir, labelDir);
        emit scanCompleted();
        return jsonResult;
    }

    // 以下为原有 YOLO txt 扫描流程

    // Collect image files by stem
    QFileInfoList imageFiles = imgDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        if (isImageFile(fi.fileName())) {
            imageByStem[fi.completeBaseName()] = fi;
        }
    }

    // Collect label files by stem（复用上方已获取的 labelFiles 列表）
    QMap<QString, QFileInfo> labelByStem;
    for (const auto &fi : labelFiles) {
        if (isLabelFile(fi.fileName())) {
            labelByStem[fi.completeBaseName()] = fi;
        }
    }

    int totalImages = imageByStem.size();
    int totalLabels = labelByStem.size();
    int totalFiles = totalImages + totalLabels;
    int matched = 0;
    int unmatchedImages = 0;
    int unmatchedLabels = 0;

    // Build the combined set of all stems
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

            // Parse the label file
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
            unmatchedImages++;
            sample["status"] = QStringLiteral("unmatched_image");
            sample["valid"] = false;
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

    ltInfo(LT_LOG_DATASET()) << "Scan completed - matched:" << matched
                             << "unmatched images:" << unmatchedImages
                             << "unmatched labels:" << unmatchedLabels;

    emit scanCompleted();
    return result;
}

bool ImportScanner::parseLabelFile(const QString &filePath, QSet<int> &classIds, QStringList &errors)
{
    ltTrace(LT_LOG_DATASET()) << "parseLabelFile filePath=" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errors.append(QStringLiteral("Cannot open file"));
        ltWarning(LT_LOG_DATASET()) << "parseLabelFile: cannot open file:" << filePath;
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
        if (parts.size() != 5) {
            errors.append(QStringLiteral("Line %1: expected 5 values, got %2")
                              .arg(lineNumber).arg(parts.size()));
            allValid = false;
            continue;
        }

        // Parse class_id (must be non-negative integer)
        bool ok = false;
        int classId = parts[0].toInt(&ok);
        if (!ok || classId < 0) {
            errors.append(QStringLiteral("Line %1: invalid class_id '%2'")
                              .arg(lineNumber).arg(parts[0]));
            allValid = false;
            continue;
        }

        // Parse cx, cy, w, h (must be floats in [0, 1])
        bool coordsValid = true;
        for (int i = 1; i < 5; ++i) {
            bool convOk = false;
            double val = parts[i].toDouble(&convOk);
            if (!convOk || val < 0.0 || val > 1.0) {
                errors.append(QStringLiteral("Line %1: coordinate '%2' out of range [0,1]")
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
        || ext == QStringLiteral("bmp");
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

    // 收集图片文件，按文件名建立索引（用于 JSON 中的 file_name 匹配）
    QFileInfoList imageFiles = imgDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QMap<QString, QFileInfo> imageByFileName;
    for (const auto &fi : imageFiles) {
        if (isImageFile(fi.fileName())) {
            // 使用文件名（不含路径）作为 key，支持跨目录匹配
            imageByFileName[fi.fileName()] = fi;
        }
    }

    // 查找标签目录中的所有 JSON 文件
    QFileInfoList jsonFiles;
    QFileInfoList allLabelFiles = lblDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
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
        if (imageByFileName.contains(fileName)) {
            QFileInfo imgFi = imageByFileName[fileName];
            sample["imagePath"] = imgFi.absoluteFilePath();
            sample["labelPath"] = jsonFiles.first().absoluteFilePath();
            sample["stem"] = imgFi.completeBaseName();
            matchedImageFiles.insert(fileName);

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
        QString jsonLabelPath;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                hasJsonLabels = true;
                jsonLabelPath = fi.absoluteFilePath();
                break;
            }
        }

        if (hasJsonLabels) {
            // COCO JSON 格式
            result["detectedFormat"] = QStringLiteral("coco_json");
            result["labelDirOrPath"] = jsonLabelPath;

            QVariantMap jsonResult = parseJsonLabelFile(jsonLabelPath);
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
                    if (imageFileNames.contains(fileName)) {
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
    for (const auto &fi : labelFiles) {
        if (fi.suffix().toLower() == QStringLiteral("json")) {
            hasJsonLabels = true;
            break;
        }
    }

    QSet<int> classIds;
    int labelCount = 0;

    if (hasJsonLabels) {
        // 查找第一个 JSON 标签文件
        QString jsonLabelPath;
        for (const auto &fi : labelFiles) {
            if (fi.suffix().toLower() == QStringLiteral("json")) {
                jsonLabelPath = fi.absoluteFilePath();
                break;
            }
        }

        result["detectedFormat"] = QStringLiteral("coco_json");
        result["labelDirOrPath"] = jsonLabelPath;

        QVariantMap jsonResult = parseJsonLabelFile(jsonLabelPath);
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
                if (imageFileNames.contains(fileName)) {
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
