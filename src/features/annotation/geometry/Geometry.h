#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <QString>

namespace Geometry {

enum ShapeType {
    HBB     = 0,   // Horizontal Bounding Box (AxisAlignedBox)
    OBB     = 1,   // Oriented Bounding Box (RotatedBox)  - Phase 3
    Polygon = 2    // Arbitrary polygon                   - later phase
};

} // namespace Geometry

/**
 * @brief Common annotation metadata shared by all geometry types.
 *
 * Every annotation placed on an image carries these fields regardless
 * of its shape (HBB, OBB, Polygon, ...).
 */
struct Annotation {
    QString id;                                          // UUID
    int     classIndex  = -1;                            // class_id in YOLO format
    QString className;                                   // class name (resolved from taxonomy)
    float   confidence  = 0.0f;                          // model confidence; 0 for manual
    QString sourceType  = QStringLiteral("manual");      // "manual" | "assisted_confirm" | "mapping_rewrite"
    bool    isConfirmed = true;                           // user has confirmed this annotation
    bool    isSelected  = false;                          // currently selected in UI
    int     zIndex      = 0;                             // render order
};

#endif // GEOMETRY_H
