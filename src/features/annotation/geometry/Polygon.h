#ifndef POLYGON_H
#define POLYGON_H

#include "Geometry.h"

#include <QString>
#include <QVector>
#include <QPointF>
#include <QRectF>

/**
 * @brief Arbitrary polygon annotation.
 *
 * Geometry: ordered list of vertices in normalized coordinates [0,1].
 * Supports bounding box computation, validity check, and point-in-polygon test.
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

    /**
     * @brief 判断多边形是否有效（至少3个点）
     */
    bool isValid() const { return points.size() >= 3; }

    /**
     * @brief 计算所有顶点的包围盒（归一化坐标）
     * @return 包围盒，空多边形返回空QRectF
     */
    QRectF boundingRect() const
    {
        if (points.isEmpty())
            return QRectF();

        float minX = 1.0f, minY = 1.0f, maxX = 0.0f, maxY = 0.0f;
        for (const QPointF &pt : points) {
            float x = static_cast<float>(pt.x());
            float y = static_cast<float>(pt.y());
            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
        return QRectF(minX, minY, maxX - minX, maxY - minY);
    }

    /**
     * @brief 判断点是否在多边形内部（射线法）
     * @param point 待检测点（归一化坐标）
     * @return true在多边形内部
     */
    bool contains(const QPointF &point) const
    {
        if (points.size() < 3)
            return false;

        // 射线法（Ray-casting algorithm）
        bool inside = false;
        const int n = points.size();
        qreal px = point.x();
        qreal py = point.y();

        for (int i = 0, j = n - 1; i < n; j = i++) {
            qreal xi = points[i].x(), yi = points[i].y();
            qreal xj = points[j].x(), yj = points[j].y();

            if (((yi > py) != (yj > py)) &&
                (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    }
};

#endif // POLYGON_H
