#include "AnnotationService.h"
#include "labelio/YoloTxtReader.h"
#include "labelio/YoloTxtWriter.h"
#include "geometry/AxisAlignedBox.h"
#include "database/Database.h"
#include "utils/Id.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

AnnotationService::AnnotationService(QObject *parent)
    : QObject(parent)
{
}

QVariantList AnnotationService::loadAnnotations(const QString &labelPath)
{
    QVariantList result;

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
