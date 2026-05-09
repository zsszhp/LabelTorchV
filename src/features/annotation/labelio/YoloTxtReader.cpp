#include "YoloTxtReader.h"

#include <QFile>
#include <QTextStream>
#include <QUuid>
#include <QDebug>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// HBB methods
// ---------------------------------------------------------------------------

QVector<AxisAlignedBox> YoloTxtReader::read(const QString &filePath)
{
    QVector<AxisAlignedBox> result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "YoloTxtReader: cannot open file:" << filePath;
        return result;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        AxisAlignedBox ann = parseLine(line);
        if (ann.classIndex >= 0) {
            result.append(ann);
        } else {
            qWarning() << "YoloTxtReader: skipping invalid line:" << line;
        }
    }

    file.close();
    return result;
}

AxisAlignedBox YoloTxtReader::parseLine(const QString &line)
{
    AxisAlignedBox ann;
    ann.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 5) {
        ann.classIndex = -1;   // mark as invalid
        return ann;
    }

    // --- class_id : must be a non-negative integer ---
    bool ok = false;
    int classId = parts[0].toInt(&ok);
    if (!ok || classId < 0) {
        ann.classIndex = -1;
        return ann;
    }
    ann.classIndex = classId;

    // --- cx, cy, w, h : must be floats in [0, 1] ---
    float coords[4];
    for (int i = 0; i < 4; ++i) {
        bool convOk = false;
        coords[i] = parts[i + 1].toFloat(&convOk);
        if (!convOk || coords[i] < 0.0f || coords[i] > 1.0f) {
            ann.classIndex = -1;
            return ann;
        }
    }

    ann.cx = coords[0];
    ann.cy = coords[1];
    ann.w  = coords[2];
    ann.h  = coords[3];

    return ann;
}

// ---------------------------------------------------------------------------
// OBB methods
// ---------------------------------------------------------------------------

QVector<RotatedBox> YoloTxtReader::readOBB(const QString &filePath)
{
    QVector<RotatedBox> result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "YoloTxtReader: cannot open file for OBB:" << filePath;
        return result;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        RotatedBox ann = parseOBBLine(line);
        if (ann.classIndex >= 0) {
            result.append(ann);
        } else {
            qWarning() << "YoloTxtReader: skipping invalid OBB line:" << line;
        }
    }

    file.close();
    return result;
}

RotatedBox YoloTxtReader::parseOBBLine(const QString &line)
{
    RotatedBox ann;
    ann.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() != 9) {
        ann.classIndex = -1;
        return ann;
    }

    // --- class_id ---
    bool ok = false;
    int classId = parts[0].toInt(&ok);
    if (!ok || classId < 0) {
        ann.classIndex = -1;
        return ann;
    }
    ann.classIndex = classId;

    // --- 4 corners: (x1,y1), (x2,y2), (x3,y3), (x4,y4) ---
    float coords[8];
    for (int i = 0; i < 8; ++i) {
        bool convOk = false;
        coords[i] = parts[i + 1].toFloat(&convOk);
        if (!convOk || coords[i] < 0.0f || coords[i] > 1.0f) {
            ann.classIndex = -1;
            return ann;
        }
    }

    float x1 = coords[0], y1 = coords[1];
    float x2 = coords[2], y2 = coords[3];
    float x3 = coords[4], y3 = coords[5];
    float x4 = coords[6], y4 = coords[7];

    // Center = average of 4 corners
    ann.cx = (x1 + x2 + x3 + x4) / 4.0f;
    ann.cy = (y1 + y2 + y3 + y4) / 4.0f;

    // Edge vectors from corner 1 to corner 2, and corner 1 to corner 4
    float dx1 = x2 - x1;
    float dy1 = y2 - y1;
    float dx2 = x4 - x1;
    float dy2 = y4 - y1;

    // Width and height from edge lengths
    ann.w = std::sqrt(dx1 * dx1 + dy1 * dy1);
    ann.h = std::sqrt(dx2 * dx2 + dy2 * dy2);

    // Angle from the first edge vector
    ann.angle = static_cast<float>(std::atan2(static_cast<double>(dy1),
                                               static_cast<double>(dx1)) * 180.0 / M_PI);

    return ann;
}

Geometry::ShapeType YoloTxtReader::detectFormat(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "YoloTxtReader: cannot open file for format detection:" << filePath;
        return Geometry::HBB;  // default fallback
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        file.close();

        if (parts.size() == 9)
            return Geometry::OBB;
        if (parts.size() == 5)
            return Geometry::HBB;

        // Unknown format, default to HBB
        return Geometry::HBB;
    }

    file.close();
    return Geometry::HBB;  // empty file defaults to HBB
}
