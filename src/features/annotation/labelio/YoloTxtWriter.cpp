#include "YoloTxtWriter.h"
#include "utils/Log.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

// ---------------------------------------------------------------------------
// HBB methods
// ---------------------------------------------------------------------------

bool YoloTxtWriter::write(const QString &filePath, const QVector<AxisAlignedBox> &annotations)
{
    ltTrace(LT_LOG_ANNOTATION()) << "filePath=" << filePath << "count=" << annotations.size();

    // Ensure parent directory exists
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            ltError(LT_LOG_ANNOTATION()) << "cannot create directory:" << dir.absolutePath();
            return false;
        }
    }

    // --- Atomic write: temp file + rename ---
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot open temp file for writing:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        for (const AxisAlignedBox &ann : annotations) {
            out << formatLine(ann) << QLatin1Char('\n');
        }

        out.flush();
        if (!tempFile.flush()) {
            ltError(LT_LOG_ANNOTATION()) << "flush failed for temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }   // tempFile closed here

    // Remove the existing destination file (if any)
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot remove existing file:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    // Rename temp file to the final destination
    if (!QFile::rename(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "cannot rename temp file to:" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Wrote" << annotations.size() << "HBB annotations to" << filePath;
    return true;
}

QString YoloTxtWriter::formatLine(const AxisAlignedBox &ann)
{
    ltTrace(LT_LOG_ANNOTATION()) << "classIndex=" << ann.classIndex;

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
    ltTrace(LT_LOG_ANNOTATION()) << "filePath=" << filePath << "count=" << annotations.size();

    // Ensure parent directory exists
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            ltError(LT_LOG_ANNOTATION()) << "cannot create directory for OBB:" << dir.absolutePath();
            return false;
        }
    }

    // --- Atomic write: temp file + rename ---
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot open temp file for OBB writing:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        for (const RotatedBox &ann : annotations) {
            out << formatOBBLine(ann) << QLatin1Char('\n');
        }

        out.flush();
        if (!tempFile.flush()) {
            ltError(LT_LOG_ANNOTATION()) << "flush failed for OBB temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }   // tempFile closed here

    // Remove the existing destination file (if any)
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot remove existing file for OBB:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    // Rename temp file to the final destination
    if (!QFile::rename(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "cannot rename temp file to OBB:" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Wrote" << annotations.size() << "OBB annotations to" << filePath;
    return true;
}

QString YoloTxtWriter::formatOBBLine(const RotatedBox &ann)
{
    ltTrace(LT_LOG_ANNOTATION()) << "classIndex=" << ann.classIndex;

    // Format: class_id x1 y1 x2 y2 x3 y3 x4 y4   (6 decimal places)
    // Get corner points from the RotatedBox
    QString corners = ann.toYoloOBB();
    if (corners.isEmpty())
        return {};

    return QStringLiteral("%1 %2")
        .arg(ann.classIndex)
        .arg(corners);
}

// ---------------------------------------------------------------------------
// Polygon methods
// ---------------------------------------------------------------------------

bool YoloTxtWriter::writePolygon(const QString &filePath, const QVector<Polygon> &annotations)
{
    ltTrace(LT_LOG_ANNOTATION()) << "filePath=" << filePath << "count=" << annotations.size();

    // 确保父目录存在
    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(QLatin1String("."))) {
            ltError(LT_LOG_ANNOTATION()) << "cannot create directory for Polygon:" << dir.absolutePath();
            return false;
        }
    }

    // --- 原子写入：临时文件 + 重命名 ---
    const QString tempPath = filePath + QStringLiteral(".tmp");

    {
        QFile tempFile(tempPath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot open temp file for Polygon writing:" << tempPath;
            return false;
        }

        QTextStream out(&tempFile);
        for (const Polygon &ann : annotations) {
            out << formatPolygonLine(ann) << QLatin1Char('\n');
        }

        out.flush();
        if (!tempFile.flush()) {
            ltError(LT_LOG_ANNOTATION()) << "flush failed for Polygon temp file:" << tempPath;
            QFile::remove(tempPath);
            return false;
        }
    }   // tempFile closed here

    // 删除已有目标文件
    if (QFile::exists(filePath)) {
        if (!QFile::remove(filePath)) {
            ltError(LT_LOG_ANNOTATION()) << "cannot remove existing file for Polygon:" << filePath;
            QFile::remove(tempPath);
            return false;
        }
    }

    // 重命名临时文件到目标路径
    if (!QFile::rename(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "cannot rename temp file to Polygon:" << filePath;
        QFile::remove(tempPath);
        return false;
    }

    ltInfo(LT_LOG_ANNOTATION()) << "Wrote" << annotations.size() << "Polygon annotations to" << filePath;
    return true;
}

QString YoloTxtWriter::formatPolygonLine(const Polygon &ann)
{
    ltTrace(LT_LOG_ANNOTATION()) << "classIndex=" << ann.classIndex << "points=" << ann.points.size();

    // 格式：class_id x1 y1 x2 y2 ... xn yn（6位小数）
    if (ann.points.size() < 3)
        return {};

    QString line = QString::number(ann.classIndex);
    for (const QPointF &pt : ann.points) {
        line += QStringLiteral(" %1 %2")
            .arg(static_cast<double>(pt.x()), 0, 'f', 6)
            .arg(static_cast<double>(pt.y()), 0, 'f', 6);
    }
    return line;
}
