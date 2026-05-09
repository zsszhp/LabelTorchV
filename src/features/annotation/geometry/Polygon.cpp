#include "Polygon.h"

Polygon::Polygon() = default;

Polygon::Polygon(const QVector<QPointF> &points) : m_points(points) {}

QVector<QPointF> Polygon::points() const { return m_points; }

void Polygon::setPoints(const QVector<QPointF> &points) { m_points = points; }

void Polygon::appendPoint(const QPointF &point) { m_points.append(point); }
