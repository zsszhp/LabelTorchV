#include "AxisAlignedBox.h"

AxisAlignedBox::AxisAlignedBox() = default;

AxisAlignedBox::AxisAlignedBox(double x, double y, double w, double h)
    : m_x(x), m_y(y), m_w(w), m_h(h) {}

double AxisAlignedBox::x() const { return m_x; }
double AxisAlignedBox::y() const { return m_y; }
double AxisAlignedBox::w() const { return m_w; }
double AxisAlignedBox::h() const { return m_h; }

void AxisAlignedBox::setX(double x) { m_x = x; }
void AxisAlignedBox::setY(double y) { m_y = y; }
void AxisAlignedBox::setW(double w) { m_w = w; }
void AxisAlignedBox::setH(double h) { m_h = h; }
