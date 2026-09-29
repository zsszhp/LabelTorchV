#include "YoloTxtWriter.h"
#include "utils/Log.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <cstdio>

#ifdef Q_OS_WIN
// NOGDI：排除 wingdi.h，避免其 Polygon 函数与标注结构体 Polygon 命名冲突
#define NOGDI
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
// P0-4: 原子替换工具
// ---------------------------------------------------------------------------

/**
 * @brief 将临时文件原子替换到目标路径（P0-4）。
 *
 * Windows 使用 MoveFileEx(MOVEFILE_REPLACE_EXISTING)：NTFS 上替换操作是原子的，
 * 失败时原文件保持完好——消除先 remove 再 rename 的断电丢文件窗口。
 * 非 Windows 平台使用 POSIX rename()，同样是原子替换语义。
 *
 * @param fromPath 临时文件路径（已写入完毕并关闭）。
 * @param toPath   目标文件路径。
 * @return 成功返回 true；失败返回 false（原文件不受影响）。
 */
static bool atomicReplaceFile(const QString &fromPath, const QString &toPath)
{
#ifdef Q_OS_WIN
    // MoveFileExW 带 MOVEFILE_REPLACE_EXISTING：若目标存在则原子替换，
    // MOVEFILE_WRITE_THROUGH 确保元数据写入磁盘后再返回
    std::wstring fromW = fromPath.toStdWString();
    std::wstring toW = toPath.toStdWString();
    if (!MoveFileExW(fromW.c_str(), toW.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ltError(LT_LOG_ANNOTATION()) << "MoveFileExW failed from" << fromPath
                                     << "to" << toPath
                                     << "GetLastError=" << GetLastError();
        return false;
    }
    return true;
#else
    // POSIX rename() 在同一文件系统内是原子替换
    if (std::rename(fromPath.toLocal8Bit().constData(),
                    toPath.toLocal8Bit().constData()) != 0) {
        ltError(LT_LOG_ANNOTATION()) << "rename failed from" << fromPath << "to" << toPath;
        return false;
    }
    return true;
#endif
}

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

    // --- Atomic write: temp file + atomic replace ---
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

    // 原子替换：MoveFileEx 替换已有文件，失败时原文件完好（P0-4）
    if (!atomicReplaceFile(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "atomic replace failed for:" << filePath;
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

    // --- Atomic write: temp file + atomic replace ---
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

    // 原子替换：MoveFileEx 替换已有文件，失败时原文件完好（P0-4）
    if (!atomicReplaceFile(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "atomic replace failed for OBB:" << filePath;
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

    // --- 原子写入：临时文件 + 原子替换 ---
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

    // 原子替换：MoveFileEx 替换已有文件，失败时原文件完好（P0-4）
    if (!atomicReplaceFile(tempPath, filePath)) {
        ltError(LT_LOG_ANNOTATION()) << "atomic replace failed for Polygon:" << filePath;
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
