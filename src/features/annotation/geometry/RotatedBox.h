#ifndef ROTATEDBOX_H
#define ROTATEDBOX_H

#include "AxisAlignedBox.h"

class RotatedBox
{
public:
    RotatedBox();
    RotatedBox(double x, double y, double w, double h, double angle);

    double x() const;
    double y() const;
    double w() const;
    double h() const;
    double angle() const;

    void setX(double x);
    void setY(double y);
    void setW(double w);
    void setH(double h);
    void setAngle(double angle);

private:
    double m_x = 0.0;
    double m_y = 0.0;
    double m_w = 0.0;
    double m_h = 0.0;
    double m_angle = 0.0;
};

#endif
