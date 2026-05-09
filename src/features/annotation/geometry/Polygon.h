#ifndef POLYGON_H
#define POLYGON_H

#include "Geometry.h"

#include <QString>
#include <QVector>
#include <QPointF>

/**
 * @brief Arbitrary polygon annotation - stub for a later phase.
 *
 * Geometry: ordered list of vertices in normalized coordinates.
 */
struct Polygon {
    // --- Annotation metadata (mirrors Annotation) ---
    QString id;
    int     classIndex  = -1;
    QString className;
    float   confidence  = 0.0f;
    QString sourceType  = QStringLiteral("manual");
    bool    isConfirmed = true;
    bool    isSelected  = false;
    int     zIndex      = 0;

    // --- Geometry: normalized point list ---
    QVector<QPointF> points;
};

#endif // POLYGON_H
