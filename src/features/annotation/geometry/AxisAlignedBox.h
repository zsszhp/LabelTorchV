#ifndef AXISALIGNEDBOX_H
#define AXISALIGNEDBOX_H

#include "Geometry.h"

#include <algorithm>
#include <QString>

/**
 * @brief Horizontal Bounding Box (HBB) annotation in YOLO normalized coordinates.
 *
 * Geometry uses YOLO format: (cx, cy, w, h) where all values lie in [0,1],
 * representing center-x, center-y, width, and height relative to the image
 * dimensions.
 */
struct AxisAlignedBox {
    // --- Annotation metadata (mirrors Annotation) ---
    QString id;
    int     classIndex  = -1;
    QString className;
    float   confidence  = 0.0f;
    QString sourceType  = QStringLiteral("manual");
    bool    isConfirmed = true;
    bool    isSelected  = false;
    int     zIndex      = 0;

    // --- Geometry: normalized coordinates [0,1] ---
    float cx = 0.0f;   // center x
    float cy = 0.0f;   // center y
    float w  = 0.0f;   // width
    float h  = 0.0f;   // height

    // --- Derived boundary accessors ---
    float left()   const { return cx - w / 2.0f; }
    float top()    const { return cy - h / 2.0f; }
    float right()  const { return cx + w / 2.0f; }
    float bottom() const { return cy + h / 2.0f; }

    // --- Spatial queries ---

    /**
     * @brief Test whether a normalized point (px, py) lies inside this box.
     */
    bool hitTest(float px, float py) const {
        return px >= left()  && px <= right()
            && py >= top()   && py <= bottom();
    }

    /**
     * @brief Test whether this box intersects another AxisAlignedBox.
     *
     * Two AABBs intersect when they overlap on both axes.
     */
    bool intersects(const AxisAlignedBox &other) const {
        return left()   < other.right()
            && right()  > other.left()
            && top()    < other.bottom()
            && bottom() > other.top();
    }

    /**
     * @brief Compute Intersection-over-Union (IoU) with another box.
     */
    float iou(const AxisAlignedBox &other) const {
        float interLeft   = std::max(left(),   other.left());
        float interTop    = std::max(top(),    other.top());
        float interRight  = std::min(right(),  other.right());
        float interBottom = std::min(bottom(), other.bottom());

        float interW    = std::max(0.0f, interRight - interLeft);
        float interH    = std::max(0.0f, interBottom - interTop);
        float interArea = interW * interH;

        float areaThis  = w * h;
        float areaOther = other.w * other.h;
        float unionArea = areaThis + areaOther - interArea;

        if (unionArea <= 0.0f)
            return 0.0f;

        return interArea / unionArea;
    }

    /**
     * @brief Check whether the geometry values are valid.
     *
     * A valid box has positive area and all coordinates in [0,1].
     */
    bool isValid() const {
        return w > 0.0f && h > 0.0f
            && cx >= 0.0f && cx <= 1.0f
            && cy >= 0.0f && cy <= 1.0f
            && w  >= 0.0f && w  <= 1.0f
            && h  >= 0.0f && h  <= 1.0f;
    }
};

#endif // AXISALIGNEDBOX_H
