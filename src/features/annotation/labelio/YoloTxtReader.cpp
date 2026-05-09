#include "YoloTxtReader.h"

#include <QFile>
#include <QTextStream>
#include <QUuid>
#include <QDebug>

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
