#include "ImportScanner.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QMap>

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

    // Collect image files by stem
    QFileInfoList imageFiles = imgDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QMap<QString, QFileInfo> imageByStem;
    for (const auto &fi : imageFiles) {
        if (isImageFile(fi.fileName())) {
            imageByStem[fi.completeBaseName()] = fi;
        }
    }

    // Collect label files by stem
    QFileInfoList labelFiles = lblDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
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

    return QFileInfo(fileName).suffix().toLower() == QStringLiteral("txt");
}
