#ifndef YOLOTXTWRITER_H
#define YOLOTXTWRITER_H

#include "geometry/AxisAlignedBox.h"

#include <QString>
#include <QVector>

/**
 * @brief Writes AxisAlignedBox annotations to YOLO txt format label files.
 *
 * Uses an atomic write strategy (temp file + rename) to prevent data
 * corruption if the process is interrupted mid-write.
 *
 * Output format: one annotation per line as
 *   class_id cx cy w h
 * with 6 decimal places for each coordinate value.
 */
class YoloTxtWriter
{
public:
    YoloTxtWriter() = delete;   // static-only utility

    /**
     * @brief Write annotations to a YOLO txt file atomically.
     *
     * Writes all annotations to filePath + ".tmp", then renames the
     * temp file over the destination.  If the destination already
     * exists it is replaced; if not, it is created (parent directories
     * are created automatically).
     *
     * @param filePath     Destination file path.
     * @param annotations  Annotations to write.
     * @return  true on success, false on any I/O error.
     */
    static bool write(const QString &filePath, const QVector<AxisAlignedBox> &annotations);

    /**
     * @brief Format a single annotation as a YOLO txt line.
     * @param ann  The annotation to format.
     * @return  String in the form "class_id cx cy w h".
     */
    static QString formatLine(const AxisAlignedBox &ann);
};

#endif // YOLOTXTWRITER_H
