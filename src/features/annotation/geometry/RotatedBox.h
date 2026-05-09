#ifndef ROTATEDBOX_H
#define ROTATEDBOX_H

#include "Geometry.h"

#include <QString>
#include <QVector>
#include <QPointF>
#include <QRectF>

/**
 * @brief Oriented Bounding Box (OBB) annotation.
 *
 * Geometry: YOLO OBB format (cx, cy, w, h, angle) with angle in degrees.
 * Supports rotation interaction, hit testing, and DOTA format conversion.
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

    // --- Spatial queries ---

    /**
     * @brief Compute 4 corner points from (cx, cy, w, h, angle).
     *
     * Uses rotation matrix: for each unrotated corner (±hw, ±hh) relative
     * to center, rotate by angle and translate back.
     */
    QVector<QPointF> corners() const;

    /**
     * @brief Point-in-rotated-box test.
     *
     * Transforms the point into the box's local coordinate system
     * (by reversing the rotation) and checks against the axis-aligned extents.
     */
    bool hitTest(float px, float py) const;

    /**
     * @brief Check if geometry is valid (non-zero dimensions, valid angle).
     */
    bool isValid() const;

    /**
     * @brief Get the axis-aligned bounding box that encloses this rotated box.
     */
    QRectF boundingRect() const;

    /**
     * @brief Convert to 4-corner DOTA format for YOLO OBB.
     *
     * Returns "x1 y1 x2 y2 x3 y3 x4 y4" string with 6 decimal places.
     */
    QString toYoloOBB() const;
};

#endif // ROTATEDBOX_H
