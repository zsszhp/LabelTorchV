#include "AnnotationService.h"
#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"
#include "geometry/AxisAlignedBox.h"
#include "geometry/RotatedBox.h"
#include "geometry/Polygon.h"
#include "database/Database.h"
#include "utils/Id.h"
#include "utils/Log.h"
#include "utils/UserAction.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <exception>

AnnotationService::AnnotationService(QObject *parent)
    : QObject(parent)
{
    ltTrace(LT_LOG_ANNOTATION()) << "parent=" << parent;
}

QVariantList AnnotationService::loadAnnotations(const QString &labelPath)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "shapeType=" << m_shapeType;

    QVariantList result;

    if (m_shapeType == 1) {
        // OBB mode
        return loadOBBAnnotations(labelPath);
    }

    if (m_shapeType == 2) {
        // Polygon mode
        return loadPolygonAnnotations(labelPath);
    }

    // HBB mode (default)
    QVector<AxisAlignedBox> boxes = YoloTxtReader::read(labelPath);
    result.reserve(boxes.size());

    for (const AxisAlignedBox &box : boxes) {
        QVariantMap m;
        m[QStringLiteral("id")]          = box.id;
        m[QStringLiteral("classIndex")]  = box.classIndex;
        m[QStringLiteral("className")]   = box.className;
        m[QStringLiteral("cx")]          = box.cx;
        m[QStringLiteral("cy")]          = box.cy;
        m[QStringLiteral("w")]           = box.w;
        m[QStringLiteral("h")]           = box.h;
        m[QStringLiteral("confidence")]  = box.confidence;
        m[QStringLiteral("sourceType")]  = box.sourceType;
        m[QStringLiteral("isConfirmed")] = box.isConfirmed;
        result.append(m);
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded" << boxes.size() << "HBB annotations from" << labelPath;
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "loadAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath << "shapeType=" << m_shapeType;
    m_lastError = QStringLiteral("加载标注异常: %1").arg(e.what());
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "loadAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath << "shapeType=" << m_shapeType;
    m_lastError = QStringLiteral("加载标注未知异常");
    return {};
}

bool AnnotationService::saveAnnotations(const QString &labelPath, const QString &datasetId,
                                        const QString &sampleId, const QVariantList &annotations)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "datasetId=" << datasetId
                                 << "sampleId=" << sampleId << "count=" << annotations.size()
                                 << "shapeType=" << m_shapeType;

    // 数据集锁定检查（对标 DLTools「数据集已被锁定, 操作失败」）：
    // sampleId 可用时按样本查，否则按标签路径反查样本所属数据集
    {
        QString lockDatasetId = datasetId;
        if (lockDatasetId.isEmpty() && !sampleId.isEmpty()) {
            QSqlQuery dsQuery(Database::instance().database());
            dsQuery.prepare("SELECT dataset_id FROM dataset_samples WHERE id = ?");
            dsQuery.addBindValue(sampleId);
            if (dsQuery.exec() && dsQuery.next()) lockDatasetId = dsQuery.value(0).toString();
            else if (dsQuery.lastError().isValid())
                ltWarning(LT_LOG_ANNOTATION()) << "saveAnnotations lock lookup by sampleId failed:"
                                               << dsQuery.lastError().text() << "sampleId=" << sampleId;
        }
        if (lockDatasetId.isEmpty() && !labelPath.isEmpty()) {
            QSqlQuery dsQuery(Database::instance().database());
            dsQuery.prepare("SELECT dataset_id FROM dataset_samples WHERE label_path = ? LIMIT 1");
            dsQuery.addBindValue(labelPath);
            if (dsQuery.exec() && dsQuery.next()) lockDatasetId = dsQuery.value(0).toString();
            else if (dsQuery.lastError().isValid())
                ltWarning(LT_LOG_ANNOTATION()) << "saveAnnotations lock lookup by labelPath failed:"
                                               << dsQuery.lastError().text() << "labelPath=" << labelPath;
        }
        if (!lockDatasetId.isEmpty()) {
            QSqlQuery lockQuery(Database::instance().database());
            lockQuery.prepare("SELECT locked FROM datasets WHERE id = ?");
            lockQuery.addBindValue(lockDatasetId);
            if (lockQuery.exec() && lockQuery.next() && lockQuery.value(0).toInt() == 1) {
                ltWarning(LT_LOG_ANNOTATION()) << "saveAnnotations rejected: 数据集已被锁定"
                                               << lockDatasetId;
                m_lastError = QStringLiteral("数据集已被锁定, 操作失败");
                return false;
            }
        }
    }
    m_lastError.clear();

    if (m_shapeType == 1) {
        // OBB mode
        return saveOBBAnnotations(labelPath, datasetId, sampleId, annotations);
    }

    if (m_shapeType == 2) {
        // Polygon mode
        return savePolygonAnnotations(labelPath, datasetId, sampleId, annotations);
    }

    // HBB mode (default)
    UserAction::log(QStringLiteral("annotation.save"), sampleId,
                    {{QStringLiteral("kind"), QStringLiteral("hbb")},
                     {QStringLiteral("count"), annotations.size()},
                     {QStringLiteral("datasetId"), datasetId}});
    // Convert QVariantList -> QVector<AxisAlignedBox>
    QVector<AxisAlignedBox> boxes;
    boxes.reserve(annotations.size());

    for (const QVariant &item : annotations) {
        QVariantMap m = item.toMap();
        AxisAlignedBox box;
        box.id          = m[QStringLiteral("id")].toString();
        box.classIndex  = m[QStringLiteral("classIndex")].toInt();
        box.className   = m[QStringLiteral("className")].toString();
        box.cx          = static_cast<float>(m[QStringLiteral("cx")].toDouble());
        box.cy          = static_cast<float>(m[QStringLiteral("cy")].toDouble());
        box.w           = static_cast<float>(m[QStringLiteral("w")].toDouble());
        box.h           = static_cast<float>(m[QStringLiteral("h")].toDouble());
        box.confidence  = static_cast<float>(m[QStringLiteral("confidence")].toDouble());
        box.sourceType  = m[QStringLiteral("sourceType")].toString();
        box.isConfirmed = m[QStringLiteral("isConfirmed")].toBool();
        boxes.append(box);
    }

    // Write to file atomically
    if (!YoloTxtWriter::write(labelPath, boxes)) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to write annotations to:" << labelPath;
        return false;
    }

    // Record a revision (empty before-snapshot for save operations)
    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   annotations);
    if (revId.isEmpty()) {
        ltWarning(LT_LOG_ANNOTATION()) << "File saved but revision record failed for sample:" << sampleId;
        // File was written successfully; revision failure is non-fatal
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Saved" << boxes.size() << "HBB annotations to" << labelPath;
    return true;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "saveAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath << "count=" << annotations.size()
                                 << "shapeType=" << m_shapeType;
    m_lastError = QStringLiteral("保存标注异常: %1").arg(e.what());
    return false;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "saveAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath << "count=" << annotations.size()
                                 << "shapeType=" << m_shapeType;
    m_lastError = QStringLiteral("保存标注未知异常");
    return false;
}

QVariantList AnnotationService::loadOBBAnnotations(const QString &labelPath)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath;

    QVariantList result;

    QVector<RotatedBox> boxes = YoloTxtReader::readOBB(labelPath);
    result.reserve(boxes.size());

    for (const RotatedBox &box : boxes) {
        QVariantMap m;
        m[QStringLiteral("id")]          = box.id;
        m[QStringLiteral("classIndex")]  = box.classIndex;
        m[QStringLiteral("className")]   = box.className;
        m[QStringLiteral("cx")]          = box.cx;
        m[QStringLiteral("cy")]          = box.cy;
        m[QStringLiteral("w")]           = box.w;
        m[QStringLiteral("h")]           = box.h;
        m[QStringLiteral("angle")]       = box.angle;
        m[QStringLiteral("confidence")]  = box.confidence;
        m[QStringLiteral("sourceType")]  = box.sourceType;
        m[QStringLiteral("isConfirmed")] = box.isConfirmed;
        result.append(m);
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded" << boxes.size() << "OBB annotations from" << labelPath;
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "loadOBBAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载OBB标注异常: %1").arg(e.what());
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "loadOBBAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载OBB标注未知异常");
    return {};
}

bool AnnotationService::saveOBBAnnotations(const QString &labelPath, const QString &datasetId,
                                            const QString &sampleId, const QVariantList &annotations)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "datasetId=" << datasetId
                                 << "sampleId=" << sampleId << "count=" << annotations.size();
    UserAction::log(QStringLiteral("annotation.save"), sampleId,
                    {{QStringLiteral("kind"), QStringLiteral("obb")},
                     {QStringLiteral("count"), annotations.size()},
                     {QStringLiteral("datasetId"), datasetId}});

    // Convert QVariantList -> QVector<RotatedBox>
    QVector<RotatedBox> boxes;
    boxes.reserve(annotations.size());

    for (const QVariant &item : annotations) {
        QVariantMap m = item.toMap();
        RotatedBox box;
        box.id          = m[QStringLiteral("id")].toString();
        box.classIndex  = m[QStringLiteral("classIndex")].toInt();
        box.className   = m[QStringLiteral("className")].toString();
        box.cx          = static_cast<float>(m[QStringLiteral("cx")].toDouble());
        box.cy          = static_cast<float>(m[QStringLiteral("cy")].toDouble());
        box.w           = static_cast<float>(m[QStringLiteral("w")].toDouble());
        box.h           = static_cast<float>(m[QStringLiteral("h")].toDouble());
        box.angle       = static_cast<float>(m[QStringLiteral("angle")].toDouble());
        box.confidence  = static_cast<float>(m[QStringLiteral("confidence")].toDouble());
        box.sourceType  = m[QStringLiteral("sourceType")].toString();
        box.isConfirmed = m[QStringLiteral("isConfirmed")].toBool();
        boxes.append(box);
    }

    // Write to file atomically in OBB format
    if (!YoloTxtWriter::writeOBB(labelPath, boxes)) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to write OBB annotations to:" << labelPath;
        return false;
    }

    // Record a revision
    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   annotations);
    if (revId.isEmpty()) {
        ltWarning(LT_LOG_ANNOTATION()) << "OBB file saved but revision record failed for sample:" << sampleId;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Saved" << boxes.size() << "OBB annotations to" << labelPath;
    return true;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "saveOBBAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath << "count=" << annotations.size();
    m_lastError = QStringLiteral("保存OBB标注异常: %1").arg(e.what());
    return false;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "saveOBBAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath << "count=" << annotations.size();
    m_lastError = QStringLiteral("保存OBB标注未知异常");
    return false;
}

QVariantMap AnnotationService::loadClassificationLabels(const QString &labelPath)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath;

    QVariantMap result;

    QFile file(labelPath);
    if (!file.exists()) {
        result[QStringLiteral("labelType")] = QStringLiteral("single");
        result[QStringLiteral("classId")] = -1;
        result[QStringLiteral("className")] = QString();
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to open classification label file:" << labelPath;
        result[QStringLiteral("labelType")] = QStringLiteral("single");
        result[QStringLiteral("classId")] = -1;
        result[QStringLiteral("className")] = QString();
        return result;
    }

    QTextStream in(&file);
    QString line = in.readLine().trimmed();
    file.close();

    if (line.isEmpty()) {
        result[QStringLiteral("labelType")] = QStringLiteral("single");
        result[QStringLiteral("classId")] = -1;
        result[QStringLiteral("className")] = QString();
        return result;
    }

    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    if (parts.size() == 1) {
        // Single-label classification
        bool ok = false;
        int classId = parts[0].toInt(&ok);
        if (!ok) {
            ltWarning(LT_LOG_ANNOTATION()) << "Invalid class_id in classification label file:" << labelPath;
            classId = -1;
        }
        result[QStringLiteral("labelType")] = QStringLiteral("single");
        result[QStringLiteral("classId")] = classId;
        result[QStringLiteral("className")] = QString();
    } else {
        // Multi-label classification
        QVariantList classIds;
        QVariantList classNames;
        for (const QString &part : parts) {
            bool ok = false;
            int classId = part.toInt(&ok);
            if (ok) {
                classIds.append(classId);
                classNames.append(QString());
            }
        }
        result[QStringLiteral("labelType")] = QStringLiteral("multi");
        result[QStringLiteral("classIds")] = classIds;
        result[QStringLiteral("classNames")] = classNames;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded classification labels from" << labelPath
                                << "type=" << result[QStringLiteral("labelType")].toString();
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "loadClassificationLabels EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载分类标签异常: %1").arg(e.what());
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "loadClassificationLabels UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载分类标签未知异常");
    return {};
}

bool AnnotationService::saveClassificationLabels(const QString &labelPath, const QString &datasetId,
                                                  const QString &sampleId, const QVariantMap &labels)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "datasetId=" << datasetId
                                 << "sampleId=" << sampleId;
    UserAction::log(QStringLiteral("annotation.save"), sampleId,
                    {{QStringLiteral("kind"), QStringLiteral("classification")},
                     {QStringLiteral("datasetId"), datasetId}});

    QString labelType = labels[QStringLiteral("labelType")].toString();
    QString content;

    if (labelType == QLatin1String("multi")) {
        // Multi-label: space-separated class_ids
        QVariantList classIds = labels[QStringLiteral("classIds")].toList();
        QStringList idStrings;
        for (const QVariant &id : classIds) {
            idStrings.append(QString::number(id.toInt()));
        }
        content = idStrings.join(QLatin1Char(' '));
    } else {
        // Single-label: single class_id
        int classId = labels[QStringLiteral("classId")].toInt();
        content = QString::number(classId);
    }

    // --- Atomic write: temp file + rename (shared helper) ---
    if (!writeAtomically(labelPath, content, QStringLiteral("classification label"))) {
        return false;
    }

    // Record a revision
    QVariantList afterSnapshot;
    QVariantMap revEntry;
    revEntry[QStringLiteral("labelType")] = labelType;
    if (labelType == QLatin1String("multi")) {
        revEntry[QStringLiteral("classIds")] = labels[QStringLiteral("classIds")];
    } else {
        revEntry[QStringLiteral("classId")] = labels[QStringLiteral("classId")];
    }
    afterSnapshot.append(revEntry);

    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   afterSnapshot);
    if (revId.isEmpty()) {
        ltWarning(LT_LOG_ANNOTATION()) << "Classification label saved but revision record failed for sample:" << sampleId;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Saved classification labels to" << labelPath << "type=" << labelType;
    return true;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "saveClassificationLabels EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("保存分类标签异常: %1").arg(e.what());
    return false;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "saveClassificationLabels UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("保存分类标签未知异常");
    return false;
}

QVariantMap AnnotationService::loadAnomalyLabels(const QString &labelPath)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath;

    QVariantMap result;

    QFile file(labelPath);
    if (!file.exists()) {
        result[QStringLiteral("isAnomalous")] = false;
        result[QStringLiteral("anomalyType")] = QString();
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to open anomaly label file:" << labelPath;
        result[QStringLiteral("isAnomalous")] = false;
        result[QStringLiteral("anomalyType")] = QString();
        return result;
    }

    QTextStream in(&file);
    QString line = in.readLine().trimmed();
    file.close();

    if (line.isEmpty()) {
        result[QStringLiteral("isAnomalous")] = false;
        result[QStringLiteral("anomalyType")] = QString();
        return result;
    }

    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);

    // First token: "0" (normal) or "1" (anomalous)
    bool ok = false;
    int labelValue = parts[0].toInt(&ok);
    if (!ok) {
        ltWarning(LT_LOG_ANNOTATION()) << "Invalid anomaly label in file:" << labelPath;
        result[QStringLiteral("isAnomalous")] = false;
        result[QStringLiteral("anomalyType")] = QString();
        return result;
    }

    result[QStringLiteral("isAnomalous")] = (labelValue == 1);

    // Second token (optional): anomaly type
    if (parts.size() > 1) {
        result[QStringLiteral("anomalyType")] = parts[1];
    } else {
        result[QStringLiteral("anomalyType")] = QString();
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded anomaly labels from" << labelPath
                                << "isAnomalous=" << result[QStringLiteral("isAnomalous")].toBool();
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "loadAnomalyLabels EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载异常检测标签异常: %1").arg(e.what());
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "loadAnomalyLabels UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载异常检测标签未知异常");
    return {};
}

bool AnnotationService::saveAnomalyLabels(const QString &labelPath, const QString &datasetId,
                                           const QString &sampleId, bool isAnomalous)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "datasetId=" << datasetId
                                 << "sampleId=" << sampleId << "isAnomalous=" << isAnomalous;
    UserAction::log(QStringLiteral("annotation.save"), sampleId,
                    {{QStringLiteral("kind"), QStringLiteral("anomaly")},
                     {QStringLiteral("isAnomalous"), isAnomalous},
                     {QStringLiteral("datasetId"), datasetId}});

    // Content: "0" for normal, "1" for anomalous
    QString content = isAnomalous ? QStringLiteral("1") : QStringLiteral("0");

    // --- Atomic write: temp file + rename (shared helper) ---
    if (!writeAtomically(labelPath, content, QStringLiteral("anomaly label"))) {
        return false;
    }

    // Record a revision
    QVariantList afterSnapshot;
    QVariantMap revEntry;
    revEntry[QStringLiteral("labelType")] = QStringLiteral("anomaly");
    revEntry[QStringLiteral("isAnomalous")] = isAnomalous;
    afterSnapshot.append(revEntry);

    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   afterSnapshot);
    if (revId.isEmpty()) {
        ltWarning(LT_LOG_ANNOTATION()) << "Anomaly label saved but revision record failed for sample:" << sampleId;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Saved anomaly labels to" << labelPath << "isAnomalous=" << isAnomalous;
    return true;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "saveAnomalyLabels EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath << "isAnomalous=" << isAnomalous;
    m_lastError = QStringLiteral("保存异常检测标签异常: %1").arg(e.what());
    return false;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "saveAnomalyLabels UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath << "isAnomalous=" << isAnomalous;
    m_lastError = QStringLiteral("保存异常检测标签未知异常");
    return false;
}

bool AnnotationService::writeAtomically(const QString &filePath, const QString &content, const QString &context)
{
    // --- Atomic write: temp file + rename (shared by classification/anomaly label writers) ---
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            ltError(LT_LOG_ANNOTATION()) << "cannot create directory for" << context << ":" << dir.absolutePath();
            return false;
        }
    }

    const QString tempPath = filePath + QStringLiteral(".tmp");
    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot open temp file for" << context << ":" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        out << content << QLatin1Char('\n');
        out.flush();
        if (!tempFile.flush()) {
            ltError(LT_LOG_ANNOTATION()) << "flush failed for" << context << "temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot remove existing" << context << "file:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (!QFile::rename(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "cannot rename temp file to" << context << ":" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

void AnnotationService::setShapeType(int shapeType)
{
    ltTrace(LT_LOG_ANNOTATION()) << "shapeType=" << shapeType;
    // 防御：负值/越界值会被钳制到合法区间，防止 names[shapeType % 3] 负下标越界读
    if (shapeType < 0 || shapeType > 2) {
        ltWarning(LT_LOG_ANNOTATION()) << "setShapeType: invalid value" << shapeType
                                       << "- clamped to 0 (HBB)";
        shapeType = 0;
    }
    m_shapeType = shapeType;
    const char* names[] = {"HBB", "OBB", "Polygon"};
    ltInfo(LT_LOG_ANNOTATION()) << "Shape type set to" << names[shapeType];
}

QString AnnotationService::createRevision(const QString &datasetId, const QString &sampleId,
                                          const QString &sourceType,
                                          const QVariantList &beforeSnapshot,
                                          const QVariantList &afterSnapshot)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "datasetId=" << datasetId << "sampleId=" << sampleId
                                 << "sourceType=" << sourceType
                                 << "beforeCount=" << beforeSnapshot.size()
                                 << "afterCount=" << afterSnapshot.size();

    QString revisionId = Id::generate();

    // Serialize before snapshot to JSON
    QJsonArray beforeArray;
    for (const QVariant &item : beforeSnapshot) {
        QVariantMap m = item.toMap();
        QJsonObject obj;
        obj[QStringLiteral("id")]          = m[QStringLiteral("id")].toString();
        obj[QStringLiteral("classIndex")]  = m[QStringLiteral("classIndex")].toInt();
        obj[QStringLiteral("className")]   = m[QStringLiteral("className")].toString();
        obj[QStringLiteral("cx")]          = m[QStringLiteral("cx")].toDouble();
        obj[QStringLiteral("cy")]          = m[QStringLiteral("cy")].toDouble();
        obj[QStringLiteral("w")]           = m[QStringLiteral("w")].toDouble();
        obj[QStringLiteral("h")]           = m[QStringLiteral("h")].toDouble();
        obj[QStringLiteral("angle")]       = m[QStringLiteral("angle")].toDouble();
        obj[QStringLiteral("confidence")]  = m[QStringLiteral("confidence")].toDouble();
        obj[QStringLiteral("sourceType")]  = m[QStringLiteral("sourceType")].toString();
        obj[QStringLiteral("isConfirmed")] = m[QStringLiteral("isConfirmed")].toBool();
        beforeArray.append(obj);
    }
    QString beforeJson = QJsonDocument(beforeArray).toJson(QJsonDocument::Compact);

    // Serialize after snapshot to JSON
    QJsonArray afterArray;
    for (const QVariant &item : afterSnapshot) {
        QVariantMap m = item.toMap();
        QJsonObject obj;
        obj[QStringLiteral("id")]          = m[QStringLiteral("id")].toString();
        obj[QStringLiteral("classIndex")]  = m[QStringLiteral("classIndex")].toInt();
        obj[QStringLiteral("className")]   = m[QStringLiteral("className")].toString();
        obj[QStringLiteral("cx")]          = m[QStringLiteral("cx")].toDouble();
        obj[QStringLiteral("cy")]          = m[QStringLiteral("cy")].toDouble();
        obj[QStringLiteral("w")]           = m[QStringLiteral("w")].toDouble();
        obj[QStringLiteral("h")]           = m[QStringLiteral("h")].toDouble();
        obj[QStringLiteral("angle")]       = m[QStringLiteral("angle")].toDouble();
        obj[QStringLiteral("confidence")]  = m[QStringLiteral("confidence")].toDouble();
        obj[QStringLiteral("sourceType")]  = m[QStringLiteral("sourceType")].toString();
        obj[QStringLiteral("isConfirmed")] = m[QStringLiteral("isConfirmed")].toBool();
        afterArray.append(obj);
    }
    QString afterJson = QJsonDocument(afterArray).toJson(QJsonDocument::Compact);

    // Insert into annotation_revisions table
    QSqlQuery query(Database::instance().database());
    query.prepare("INSERT INTO annotation_revisions "
                  "(id, dataset_id, sample_id, source_type, before_snapshot_json, after_snapshot_json) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(revisionId);
    query.addBindValue(datasetId);
    query.addBindValue(sampleId);
    query.addBindValue(sourceType);
    query.addBindValue(beforeSnapshot.isEmpty() ? QVariant() : beforeJson);
    query.addBindValue(afterJson);

    if (!query.exec()) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to create revision:" << query.lastError().text();
        return {};
    }

    ltDebug(LT_LOG_ANNOTATION()) << "Revision created:" << revisionId
             << "sample:" << sampleId << "source:" << sourceType;
    return revisionId;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "createRevision EXCEPTION:" << e.what()
                                 << "datasetId=" << datasetId << "sampleId=" << sampleId;
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "createRevision UNKNOWN EXCEPTION"
                                 << "datasetId=" << datasetId << "sampleId=" << sampleId;
    return {};
}

QVariantList AnnotationService::listSamples(const QString &datasetId, int offset, int limit)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "datasetId=" << datasetId
                                  << "offset=" << offset << "limit=" << limit;

    QVariantList result;
    if (datasetId.isEmpty()) return result;

    // P1-22：分页查询——limit<=0 时保持旧行为返回全量，调用方应优先传分页参数
    QSqlQuery query(Database::instance().database());
    if (limit > 0) {
        query.prepare("SELECT id, dataset_id, image_path, label_path, width, height, "
                      "hash, validation_status, error_code, split "
                      "FROM dataset_samples WHERE dataset_id = ? "
                      "ORDER BY image_path LIMIT ? OFFSET ?");
        query.addBindValue(datasetId);
        query.addBindValue(limit);
        query.addBindValue(offset > 0 ? offset : 0);
    } else {
        query.prepare("SELECT id, dataset_id, image_path, label_path, width, height, "
                      "hash, validation_status, error_code, split "
                      "FROM dataset_samples WHERE dataset_id = ? ORDER BY image_path");
        query.addBindValue(datasetId);
    }

    if (!query.exec()) {
        ltError(LT_LOG_ANNOTATION()) << "listSamples failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        QVariantMap s;
        s[QStringLiteral("id")]               = query.value(0);
        s[QStringLiteral("datasetId")]         = query.value(1);
        s[QStringLiteral("imagePath")]         = query.value(2);
        s[QStringLiteral("labelPath")]         = query.value(3);
        s[QStringLiteral("width")]             = query.value(4);
        s[QStringLiteral("height")]            = query.value(5);
        s[QStringLiteral("hash")]              = query.value(6);
        s[QStringLiteral("validationStatus")]  = query.value(7);
        s[QStringLiteral("errorCode")]         = query.value(8);
        s[QStringLiteral("split")]             = query.value(9);
        result.append(s);
    }

    ltDebug(LT_LOG_ANNOTATION()) << "Listed" << result.size() << "samples for dataset" << datasetId;
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "listSamples EXCEPTION:" << e.what()
                                 << "datasetId=" << datasetId << "offset=" << offset << "limit=" << limit;
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "listSamples UNKNOWN EXCEPTION"
                                 << "datasetId=" << datasetId << "offset=" << offset << "limit=" << limit;
    return {};
}

int AnnotationService::countSamples(const QString &datasetId)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "datasetId=" << datasetId;
    if (datasetId.isEmpty()) return 0;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT COUNT(*) FROM dataset_samples WHERE dataset_id = ?");
    query.addBindValue(datasetId);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }
    ltError(LT_LOG_ANNOTATION()) << "countSamples failed:" << query.lastError().text();
    return 0;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "countSamples EXCEPTION:" << e.what()
                                 << "datasetId=" << datasetId;
    return 0;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "countSamples UNKNOWN EXCEPTION"
                                 << "datasetId=" << datasetId;
    return 0;
}

QVariantMap AnnotationService::getSample(const QString &sampleId)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "sampleId=" << sampleId;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, dataset_id, image_path, label_path, width, height, "
                  "hash, validation_status, error_code "
                  "FROM dataset_samples WHERE id = ?");
    query.addBindValue(sampleId);

    if (query.exec() && query.next()) {
        QVariantMap s;
        s[QStringLiteral("id")]               = query.value(0);
        s[QStringLiteral("datasetId")]         = query.value(1);
        s[QStringLiteral("imagePath")]         = query.value(2);
        s[QStringLiteral("labelPath")]         = query.value(3);
        s[QStringLiteral("width")]             = query.value(4);
        s[QStringLiteral("height")]            = query.value(5);
        s[QStringLiteral("hash")]              = query.value(6);
        s[QStringLiteral("validationStatus")]  = query.value(7);
        s[QStringLiteral("errorCode")]         = query.value(8);
        return s;
    }

    ltWarning(LT_LOG_ANNOTATION()) << "Sample not found or query error:" << sampleId
               << query.lastError().text();
    return {};
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "getSample EXCEPTION:" << e.what() << "sampleId=" << sampleId;
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "getSample UNKNOWN EXCEPTION" << "sampleId=" << sampleId;
    return {};
}

// ---------------------------------------------------------------------------
// Polygon methods
// ---------------------------------------------------------------------------

QVariantList AnnotationService::loadPolygonAnnotations(const QString &labelPath)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath;

    QVariantList result;

    QVector<Polygon> polygons = YoloTxtReader::readPolygon(labelPath);
    result.reserve(polygons.size());

    for (const Polygon &poly : polygons) {
        QVariantMap m;
        m[QStringLiteral("id")]          = poly.id;
        m[QStringLiteral("classIndex")]  = poly.classIndex;
        m[QStringLiteral("className")]   = poly.className;
        m[QStringLiteral("shapeType")]   = 2;  // Polygon

        // 将顶点列表转换为QVariantList [{x, y}, ...]
        QVariantList pts;
        for (const QPointF &pt : poly.points) {
            QVariantMap pm;
            pm[QStringLiteral("x")] = pt.x();
            pm[QStringLiteral("y")] = pt.y();
            pts.append(pm);
        }
        m[QStringLiteral("points")]      = pts;

        // 计算包围盒（用于兼容性）
        if (!poly.points.isEmpty()) {
            float minX = 1.0f, minY = 1.0f, maxX = 0.0f, maxY = 0.0f;
            for (const QPointF &pt : poly.points) {
                minX = qMin(minX, static_cast<float>(pt.x()));
                minY = qMin(minY, static_cast<float>(pt.y()));
                maxX = qMax(maxX, static_cast<float>(pt.x()));
                maxY = qMax(maxY, static_cast<float>(pt.y()));
            }
            m[QStringLiteral("cx")] = (minX + maxX) / 2.0f;
            m[QStringLiteral("cy")] = (minY + maxY) / 2.0f;
            m[QStringLiteral("w")]  = maxX - minX;
            m[QStringLiteral("h")]  = maxY - minY;
        } else {
            m[QStringLiteral("cx")] = 0.0f;
            m[QStringLiteral("cy")] = 0.0f;
            m[QStringLiteral("w")]  = 0.0f;
            m[QStringLiteral("h")]  = 0.0f;
        }

        m[QStringLiteral("angle")]       = 0.0f;
        m[QStringLiteral("confidence")]  = poly.confidence;
        m[QStringLiteral("sourceType")]  = poly.sourceType;
        m[QStringLiteral("isConfirmed")] = poly.isConfirmed;
        result.append(m);
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Loaded" << polygons.size() << "Polygon annotations from" << labelPath;
    return result;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "loadPolygonAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载多边形标注异常: %1").arg(e.what());
    return {};
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "loadPolygonAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath;
    m_lastError = QStringLiteral("加载多边形标注未知异常");
    return {};
}

bool AnnotationService::savePolygonAnnotations(const QString &labelPath, const QString &datasetId,
                                                const QString &sampleId, const QVariantList &annotations)
try {
    ltTrace(LT_LOG_ANNOTATION()) << "labelPath=" << labelPath << "datasetId=" << datasetId
                                 << "sampleId=" << sampleId << "count=" << annotations.size();

    UserAction::log(QStringLiteral("annotation.save"), sampleId,
                    {{QStringLiteral("kind"), QStringLiteral("polygon")},
                     {QStringLiteral("count"), annotations.size()},
                     {QStringLiteral("datasetId"), datasetId}});

    // 将QVariantList转换为QVector<Polygon>
    QVector<Polygon> polygons;
    polygons.reserve(annotations.size());

    for (const QVariant &item : annotations) {
        QVariantMap m = item.toMap();
        Polygon poly;
        poly.id          = m[QStringLiteral("id")].toString();
        poly.classIndex  = m[QStringLiteral("classIndex")].toInt();
        poly.className   = m[QStringLiteral("className")].toString();
        poly.confidence  = static_cast<float>(m[QStringLiteral("confidence")].toDouble());
        poly.sourceType  = m[QStringLiteral("sourceType")].toString();
        poly.isConfirmed = m[QStringLiteral("isConfirmed")].toBool();

        // 从points字段提取顶点
        QVariantList pts = m[QStringLiteral("points")].toList();
        for (const QVariant &ptVar : pts) {
            QVariantMap pm = ptVar.toMap();
            poly.points.append(QPointF(
                static_cast<float>(pm[QStringLiteral("x")].toDouble()),
                static_cast<float>(pm[QStringLiteral("y")].toDouble())
            ));
        }

        polygons.append(poly);
    }

    // 原子写入
    if (!YoloTxtWriter::writePolygon(labelPath, polygons)) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to write Polygon annotations to:" << labelPath;
        return false;
    }

    // 记录修订
    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   annotations);
    if (revId.isEmpty()) {
        ltWarning(LT_LOG_ANNOTATION()) << "Polygon file saved but revision record failed for sample:" << sampleId;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Saved" << polygons.size() << "Polygon annotations to" << labelPath;
    return true;
}
catch (const std::exception &e) {
    ltError(LT_LOG_ANNOTATION()) << "savePolygonAnnotations EXCEPTION:" << e.what()
                                 << "labelPath=" << labelPath << "count=" << annotations.size();
    m_lastError = QStringLiteral("保存多边形标注异常: %1").arg(e.what());
    return false;
}
catch (...) {
    ltError(LT_LOG_ANNOTATION()) << "savePolygonAnnotations UNKNOWN EXCEPTION"
                                 << "labelPath=" << labelPath << "count=" << annotations.size();
    m_lastError = QStringLiteral("保存多边形标注未知异常");
    return false;
}
