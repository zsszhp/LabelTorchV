#ifndef YOLOTXTREADER_H
#define YOLOTXTREADER_H

#include "geometry/AxisAlignedBox.h"
#include "geometry/RotatedBox.h"
#include "geometry/Geometry.h"

#include <QString>
#include <QVector>

/**
 * @brief Parses YOLO txt format label files into AxisAlignedBox or RotatedBox annotations.
 *
 * YOLO HBB format: one annotation per line as
 *   class_id cx cy w h
 * where class_id is a non-negative integer and cx/cy/w/h are floats in [0,1].
 *
 * YOLO OBB format (DOTA): one annotation per line as
 *   class_id x1 y1 x2 y2 x3 y3 x4 y4
 * where the 4 corner points are in normalized coordinates.
 *
 * Empty lines and lines starting with '#' are silently skipped.
 * On any file-level I/O error the returned vector is empty.
 */
class YoloTxtReader
{
public:
    YoloTxtReader() = delete;   // static-only utility

    // --- HBB methods ---

    /**
     * @brief Parse an entire YOLO txt file (HBB format).
     * @param filePath  Absolute path to the .txt label file.
     * @return  Vector of parsed AxisAlignedBox annotations.
     *          Empty vector on file-open failure.
     */
    static QVector<AxisAlignedBox> read(const QString &filePath);

    /**
     * @brief Parse a single YOLO txt line "class_id cx cy w h".
     * @param line  A trimmed line from a YOLO label file.
     * @return  Parsed AxisAlignedBox with a generated UUID.
     *          On parse failure classIndex is set to -1.
     */
    static AxisAlignedBox parseLine(const QString &line);

    // --- OBB methods ---

    /**
     * @brief Read file detecting format automatically (5 cols = HBB, 9 cols = OBB).
     * @param filePath  Absolute path to the .txt label file.
     * @return  Vector of parsed RotatedBox annotations.
     *          Empty vector on file-open failure.
     */
    static QVector<RotatedBox> readOBB(const QString &filePath);

    /**
     * @brief Parse a single OBB line (9 values).
     *
     * YOLO OBB format: class_id x1 y1 x2 y2 x3 y3 x4 y4
     * Converts 4 corners to (cx, cy, w, h, angle).
     *
     * @param line  A trimmed line from a YOLO OBB label file.
     * @return  Parsed RotatedBox with a generated UUID.
     *          On parse failure classIndex is set to -1.
     */
    static RotatedBox parseOBBLine(const QString &line);

    /**
     * @brief Detect format from file content.
     *
     * Reads the first non-empty, non-comment line and checks the number
     * of whitespace-separated tokens: 5 = HBB, 9 = OBB.
     *
     * @param filePath  Absolute path to the .txt label file.
     * @return  Geometry::ShapeType (HBB, OBB, or HBB as fallback).
     */
    static Geometry::ShapeType detectFormat(const QString &filePath);
};

#endif // YOLOTXTREADER_H
