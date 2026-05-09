#ifndef POLYGON_H
#define POLYGON_H

#include <QVector>
#include <QPointF>

class Polygon
{
public:
    Polygon();
    explicit Polygon(const QVector<QPointF> &points);

    QVector<QPointF> points() const;
    void setPoints(const QVector<QPointF> &points);
    void appendPoint(const QPointF &point);

private:
    QVector<QPointF> m_points;
};

#endif
