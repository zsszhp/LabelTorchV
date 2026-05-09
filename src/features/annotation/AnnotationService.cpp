#include "AnnotationService.h"
#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"
#include "geometry/AxisAlignedBox.h"
#include "geometry/RotatedBox.h"
#include "database/Database.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

AnnotationService::AnnotationService(QObject *parent)
    : QObject(parent)
{
}

QVariantList AnnotationService::loadAnnotations(const QString &labelPath)
{
    QVariantList result;

    if (m_shapeType == 1) {
        // OBB mode
        return loadOBBAnnotations(labelPath);
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

    return result;
}

bool AnnotationService::saveAnnotations(const QString &labelPath, const QString &datasetId,
                                        const QString &sampleId, const QVariantList &annotations)
{
    if (m_shapeType == 1) {
        // OBB mode
        return saveOBBAnnotations(labelPath, datasetId, sampleId, annotations);
    }

    // HBB mode (default)
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
        qWarning() << "AnnotationService: Failed to write annotations to:" << labelPath;
        return false;
    }

    // Record a revision (empty before-snapshot for save operations)
    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   annotations);
    if (revId.isEmpty()) {
        qWarning() << "AnnotationService: File saved but revision record failed for sample:" << sampleId;
        // File was written successfully; revision failure is non-fatal
    }

    return true;
}

QVariantList AnnotationService::loadOBBAnnotations(const QString &labelPath)
{
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

    return result;
}

bool AnnotationService::saveOBBAnnotations(const QString &labelPath, const QString &datasetId,
                                            const QString &sampleId, const QVariantList &annotations)
{
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
        qWarning() << "AnnotationService: Failed to write OBB annotations to:" << labelPath;
        return false;
    }

    // Record a revision
    QString revId = createRevision(datasetId, sampleId,
                                   QStringLiteral("manual"),
                                   QVariantList(),
                                   annotations);
    if (revId.isEmpty()) {
        qWarning() << "AnnotationService: OBB file saved but revision record failed for sample:" << sampleId;
    }

    return true;
}

QVariantMap AnnotationService::loadClassificationLabels(const QString &labelPath)
{
    QVariantMap result;

    QFile file(labelPath);
    if (!file.exists()) {
        result[QStringLiteral("labelType")] = QStringLiteral("single");
        result[QStringLiteral("classId")] = -1;
        result[QStringLiteral("className")] = QString();
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "AnnotationService: Failed to open classification label file:" << labelPath;
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
            qWarning() << "AnnotationService: Invalid class_id in classification label file:" << labelPath;
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

    return result;
}

bool AnnotationService::saveClassificationLabels(const QString &labelPath, const QString &datasetId,
                                                  const QString &sampleId, const QVariantMap &labels)
{
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

    // --- Atomic write: temp file + rename (same pattern as YoloTxtWriter) ---
    QFileInfo fi(labelPath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            qWarning() << "AnnotationService: cannot create directory for classification label:" << dir.absolutePath();
            return false;
        }
    }

    const QString tempPath = labelPath + QStringLiteral(".tmp");
    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "AnnotationService: cannot open temp file for classification label:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        out << content << QLatin1Char('\n');
        out.flush();
        if (!tempFile.flush()) {
            qWarning() << "AnnotationService: flush failed for classification temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (QFile::exists(labelPath)) {
        if (!QFile::remove(labelPath)) {
            qWarning() << "AnnotationService: cannot remove existing classification label file:" << labelPath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (!QFile::rename(tempPath, labelPath)) {
        qWarning() << "AnnotationService: cannot rename temp file to classification label:" << labelPath;
        QFile::remove(tempPath);
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
        qWarning() << "AnnotationService: Classification label saved but revision record failed for sample:" << sampleId;
    }

    return true;
}

QVariantMap AnnotationService::loadAnomalyLabels(const QString &labelPath)
{
    QVariantMap result;

    QFile file(labelPath);
    if (!file.exists()) {
        result[QStringLiteral("isAnomalous")] = false;
        result[QStringLiteral("anomalyType")] = QString();
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "AnnotationService: Failed to open anomaly label file:" << labelPath;
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
        qWarning() << "AnnotationService: Invalid anomaly label in file:" << labelPath;
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

    return result;
}

bool AnnotationService::saveAnomalyLabels(const QString &labelPath, const QString &datasetId,
                                           const QString &sampleId, bool isAnomalous)
{
    // Content: "0" for normal, "1" for anomalous
    QString content = isAnomalous ? QStringLiteral("1") : QStringLiteral("0");

    // --- Atomic write: temp file + rename (same pattern as saveClassificationLabels) ---
    QFileInfo fi(labelPath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            qWarning() << "AnnotationService: cannot create directory for anomaly label:" << dir.absolutePath();
            return false;
        }
    }

    const QString tempPath = labelPath + QStringLiteral(".tmp");
    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "AnnotationService: cannot open temp file for anomaly label:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        out << content << QLatin1Char('\n');
        out.flush();
        if (!tempFile.flush()) {
            qWarning() << "AnnotationService: flush failed for anomaly temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (QFile::exists(labelPath)) {
        if (!QFile::remove(labelPath)) {
            qWarning() << "AnnotationService: cannot remove existing anomaly label file:" << labelPath;
            QFile::remove(tempPath);
            return false;
        }
    }

    if (!QFile::rename(tempPath, labelPath)) {
        qWarning() << "AnnotationService: cannot rename temp file to anomaly label:" << labelPath;
        QFile::remove(tempPath);
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
        qWarning() << "AnnotationService: Anomaly label saved but revision record failed for sample:" << sampleId;
    }

    return true;
}

void AnnotationService::setShapeType(int shapeType)
{
    m_shapeType = shapeType;
    qDebug() << "AnnotationService: shape type set to" << (shapeType == 1 ? "OBB" : "HBB");
}

QString AnnotationService::createRevision(const QString &datasetId, const QString &sampleId,
                                          const QString &sourceType,
                                          const QVariantList &beforeSnapshot,
                                          const QVariantList &afterSnapshot)
{
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
        qWarning() << "AnnotationService: Failed to create revision:" << query.lastError().text();
        return {};
    }

    qDebug() << "AnnotationService: Revision created:" << revisionId
             << "sample:" << sampleId << "source:" << sourceType;
    return revisionId;
}

QVariantList AnnotationService::listSamples(const QString &datasetId)
{
    QVariantList result;

    QSqlQuery query(Database::instance().database());
    query.prepare("SELECT id, dataset_id, image_path, label_path, width, height, "
                  "hash, validation_status, error_code "
                  "FROM dataset_samples WHERE dataset_id = ? ORDER BY image_path");
    query.addBindValue(datasetId);

    if (!query.exec()) {
        qWarning() << "AnnotationService::listSamples failed:" << query.lastError().text();
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
        result.append(s);
    }

    return result;
}

QVariantMap AnnotationService::getSample(const QString &sampleId)
{
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

    qWarning() << "AnnotationService::getSample: not found or error:" << sampleId
               << query.lastError().text();
    return {};
}
