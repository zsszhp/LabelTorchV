#include "RotatedBox.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

QVector<QPointF> RotatedBox::corners() const
{
    QVector<QPointF> pts;
    pts.reserve(4);

    const float hw = w / 2.0f;
    const float hh = h / 2.0f;
    const float rad = static_cast<float>(angle * M_PI / 180.0);
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);

    // Unrotated corners relative to center: (±hw, ±hh)
    // Order: top-left, top-right, bottom-right, bottom-left
    const float localX[4] = { -hw,  hw,  hw, -hw };
    const float localY[4] = { -hh, -hh,  hh,  hh };

    for (int i = 0; i < 4; ++i) {
        float rx = localX[i] * cosA - localY[i] * sinA;
        float ry = localX[i] * sinA + localY[i] * cosA;
        pts.append(QPointF(static_cast<qreal>(rx + cx),
                           static_cast<qreal>(ry + cy)));
    }

    return pts;
}

bool RotatedBox::hitTest(float px, float py) const
{
    // Transform the point into the box's local coordinate system
    // by translating to center and reversing the rotation
    const float dx = px - cx;
    const float dy = py - cy;
    const float rad = static_cast<float>(-angle * M_PI / 180.0);
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);

    const float localX = dx * cosA - dy * sinA;
    const float localY = dx * sinA + dy * cosA;

    const float hw = w / 2.0f;
    const float hh = h / 2.0f;

    return localX >= -hw && localX <= hw
        && localY >= -hh && localY <= hh;
}

bool RotatedBox::isValid() const
{
    return w > 0.0f && h > 0.0f
        && cx >= 0.0f && cx <= 1.0f
        && cy >= 0.0f && cy <= 1.0f
        && w  >= 0.0f && w  <= 1.0f
        && h  >= 0.0f && h  <= 1.0f
        && angle >= -360.0f && angle <= 360.0f;
}

QRectF RotatedBox::boundingRect() const
{
    QVector<QPointF> c = corners();
    if (c.size() != 4)
        return QRectF();

    double minX = c[0].x(), maxX = c[0].x();
    double minY = c[0].y(), maxY = c[0].y();

    for (int i = 1; i < 4; ++i) {
        if (c[i].x() < minX) minX = c[i].x();
        if (c[i].x() > maxX) maxX = c[i].x();
        if (c[i].y() < minY) minY = c[i].y();
        if (c[i].y() > maxY) maxY = c[i].y();
    }

    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

QString RotatedBox::toYoloOBB() const
{
    QVector<QPointF> c = corners();
    if (c.size() != 4)
        return {};

    return QStringLiteral("%1 %2 %3 %4 %5 %6 %7 %8")
        .arg(static_cast<double>(c[0].x()), 0, 'f', 6)
        .arg(static_cast<double>(c[0].y()), 0, 'f', 6)
        .arg(static_cast<double>(c[1].x()), 0, 'f', 6)
        .arg(static_cast<double>(c[1].y()), 0, 'f', 6)
        .arg(static_cast<double>(c[2].x()), 0, 'f', 6)
        .arg(static_cast<double>(c[2].y()), 0, 'f', 6)
        .arg(static_cast<double>(c[3].x()), 0, 'f', 6)
        .arg(static_cast<double>(c[3].y()), 0, 'f', 6);
}
