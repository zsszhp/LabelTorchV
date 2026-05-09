#ifndef YOLOTXTWRITER_H
#define YOLOTXTWRITER_H

#include "geometry/AxisAlignedBox.h"
#include "geometry/RotatedBox.h"

#include <QString>
#include <QVector>

/**
 * @brief Writes AxisAlignedBox and RotatedBox annotations to YOLO txt format label files.
 *
 * Uses an atomic write strategy (temp file + rename) to prevent data
 * corruption if the process is interrupted mid-write.
 *
 * HBB output format: one annotation per line as
 *   class_id cx cy w h
 * with 6 decimal places for each coordinate value.
 *
 * OBB output format (DOTA): one annotation per line as
 *   class_id x1 y1 x2 y2 x3 y3 x4 y4
 * with 6 decimal places for each coordinate value.
 */
class YoloTxtWriter
{
public:
    YoloTxtWriter() = delete;   // static-only utility

    // --- HBB methods ---

    /**
     * @brief Write HBB annotations to a YOLO txt file atomically.
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
     * @brief Format a single HBB annotation as a YOLO txt line.
     * @param ann  The annotation to format.
     * @return  String in the form "class_id cx cy w h".
     */
    static QString formatLine(const AxisAlignedBox &ann);

    // --- OBB methods ---

    /**
     * @brief Write OBB annotations in DOTA format atomically.
     *
     * Same atomic write strategy as write().
     *
     * @param filePath     Destination file path.
     * @param annotations  RotatedBox annotations to write.
     * @return  true on success, false on any I/O error.
     */
    static bool writeOBB(const QString &filePath, const QVector<RotatedBox> &annotations);

    /**
     * @brief Format a single OBB annotation as a YOLO OBB line.
     *
     * Format: "class_id x1 y1 x2 y2 x3 y3 x4 y4"
     *
     * @param ann  The RotatedBox annotation to format.
     * @return  String in DOTA format.
     */
    static QString formatOBBLine(const RotatedBox &ann);
};

#endif // YOLOTXTWRITER_H
