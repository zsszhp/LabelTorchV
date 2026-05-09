#ifndef AXISALIGNEDBOX_H
#define AXISALIGNEDBOX_H

#include <QPointF>

class AxisAlignedBox
{
public:
    AxisAlignedBox();
    AxisAlignedBox(double x, double y, double w, double h);

    double x() const;
    double y() const;
    double w() const;
    double h() const;

    void setX(double x);
    void setY(double y);
    void setW(double w);
    void setH(double h);

private:
    double m_x = 0.0;
    double m_y = 0.0;
    double m_w = 0.0;
    double m_h = 0.0;
};

#endif
