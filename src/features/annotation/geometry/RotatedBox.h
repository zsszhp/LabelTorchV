#ifndef ROTATEDBOX_H
#define ROTATEDBOX_H

#include "Geometry.h"

#include <QString>

/**
 * @brief Oriented Bounding Box (OBB) annotation - stub for Phase 3.
 *
 * Geometry: YOLO OBB format (cx, cy, w, h, angle) with angle in degrees.
 * Spatial-query methods will be implemented in Phase 3.
 */
struct RotatedBox {
    // --- Annotation metadata (mirrors Annotation) ---
    QString id;
    int     classIndex  = -1;
    QString className;
    float   confidence  = 0.0f;
    QString sourceType  = QStringLiteral("manual");
    bool    isConfirmed = true;
    bool    isSelected  = false;
    int     zIndex      = 0;

    // --- Geometry: normalized coordinates [0,1] + rotation ---
    float cx    = 0.0f;
    float cy    = 0.0f;
    float w     = 0.0f;
    float h     = 0.0f;
    float angle = 0.0f;   // rotation in degrees
};

#endif // ROTATEDBOX_H
