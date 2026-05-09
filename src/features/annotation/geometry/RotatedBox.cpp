#include "RotatedBox.h"

RotatedBox::RotatedBox() = default;

RotatedBox::RotatedBox(double x, double y, double w, double h, double angle)
    : m_x(x), m_y(y), m_w(w), m_h(h), m_angle(angle) {}

double RotatedBox::x() const { return m_x; }
double RotatedBox::y() const { return m_y; }
double RotatedBox::w() const { return m_w; }
double RotatedBox::h() const { return m_h; }
double RotatedBox::angle() const { return m_angle; }

void RotatedBox::setX(double x) { m_x = x; }
void RotatedBox::setY(double y) { m_y = y; }
void RotatedBox::setW(double w) { m_w = w; }
void RotatedBox::setH(double h) { m_h = h; }
void RotatedBox::setAngle(double angle) { m_angle = angle; }
