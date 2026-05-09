#include "YoloTxtWriter.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// ---------------------------------------------------------------------------
// HBB methods
// ---------------------------------------------------------------------------

bool YoloTxtWriter::write(const QString &filePath, const QVector<AxisAlignedBox> &annotations)
{
    // Ensure parent directory exists
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            qWarning() << "YoloTxtWriter: cannot create directory:" << dir.absolutePath();
            return false;
        }
    }

    // --- Atomic write: temp file + rename ---
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "YoloTxtWriter: cannot open temp file for writing:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        for (const AxisAlignedBox &ann : annotations) {
            out << formatLine(ann) << QLatin1Char('\n');
        }

        out.flush();
        if (!tempFile.flush()) {
            qWarning() << "YoloTxtWriter: flush failed for temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }   // tempFile closed here

    // Remove the existing destination file (if any)
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            qWarning() << "YoloTxtWriter: cannot remove existing file:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    // Rename temp file to the final destination
    if (!QFile::rename(tempPath, filePath)) {
        qWarning() << "YoloTxtWriter: cannot rename temp file to:" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

QString YoloTxtWriter::formatLine(const AxisAlignedBox &ann)
{
    // Format: class_id cx cy w h   (6 decimal places)
    return QStringLiteral("%1 %2 %3 %4 %5")
        .arg(ann.classIndex)
        .arg(static_cast<double>(ann.cx), 0, 'f', 6)
        .arg(static_cast<double>(ann.cy), 0, 'f', 6)
        .arg(static_cast<double>(ann.w),  0, 'f', 6)
        .arg(static_cast<double>(ann.h),  0, 'f', 6);
}

// ---------------------------------------------------------------------------
// OBB methods
// ---------------------------------------------------------------------------

bool YoloTxtWriter::writeOBB(const QString &filePath, const QVector<RotatedBox> &annotations)
{
    // Ensure parent directory exists
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            qWarning() << "YoloTxtWriter: cannot create directory for OBB:" << dir.absolutePath();
            return false;
        }
    }

    // --- Atomic write: temp file + rename ---
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qWarning() << "YoloTxtWriter: cannot open temp file for OBB writing:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        for (const RotatedBox &ann : annotations) {
            out << formatOBBLine(ann) << QLatin1Char('\n');
        }

        out.flush();
        if (!tempFile.flush()) {
            qWarning() << "YoloTxtWriter: flush failed for OBB temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }   // tempFile closed here

    // Remove the existing destination file (if any)
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            qWarning() << "YoloTxtWriter: cannot remove existing file for OBB:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    // Rename temp file to the final destination
    if (!QFile::rename(tempPath, filePath)) {
        qWarning() << "YoloTxtWriter: cannot rename temp file to OBB:" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    return true;
}

QString YoloTxtWriter::formatOBBLine(const RotatedBox &ann)
{
    // Format: class_id x1 y1 x2 y2 x3 y3 x4 y4   (6 decimal places)
    // Get corner points from the RotatedBox
    QString corners = ann.toYoloOBB();
    if (corners.isEmpty())
        return {};

    return QStringLiteral("%1 %2")
        .arg(ann.classIndex)
        .arg(corners);
}
