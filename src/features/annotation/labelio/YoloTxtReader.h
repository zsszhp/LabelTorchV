#ifndef YOLOTXTREADER_H
#define YOLOTXTREADER_H

#include "geometry/AxisAlignedBox.h"

#include <QString>
#include <QVector>

/**
 * @brief Parses YOLO txt format label files into AxisAlignedBox annotations.
 *
 * YOLO format: one annotation per line as
 *   class_id cx cy w h
 * where class_id is a non-negative integer and cx/cy/w/h are floats in [0,1].
 *
 * Empty lines and lines starting with '#' are silently skipped.
 * On any file-level I/O error the returned vector is empty.
 */
class YoloTxtReader
{
public:
    YoloTxtReader() = delete;   // static-only utility

    /**
     * @brief Parse an entire YOLO txt file.
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
};

#endif // YOLOTXTREADER_H
