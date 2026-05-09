#include "CanvasController.h"
#include <QDebug>
#include <QImage>

CanvasController::CanvasController(QObject *parent) : QObject(parent) {}

void CanvasController::setZoom(qreal z)
{
    if (qFuzzyCompare(m_zoom, z)) return;
    m_zoom = z;
    emit zoomChanged();
    emit canvasUpdateRequested();
}

void CanvasController::setPanX(qreal x)
{
    if (qFuzzyCompare(m_panX, x)) return;
    m_panX = x;
    emit panChanged();
    emit canvasUpdateRequested();
}

void CanvasController::setPanY(qreal y)
{
    if (qFuzzyCompare(m_panY, y)) return;
    m_panY = y;
    emit panChanged();
    emit canvasUpdateRequested();
}

void CanvasController::setDrawMode(const QString &mode)
{
    if (m_drawMode == mode) return;
    m_drawMode = mode;
    emit drawModeChanged();
}

void CanvasController::loadImage(const QString &imagePath, const QString &labelPath)
{
    m_currentImagePath = imagePath;
    m_currentLabelPath = labelPath;

    // Get image dimensions
    QImage img(imagePath);
    if (!img.isNull()) {
        m_imageWidth = img.width();
        m_imageHeight = img.height();
    } else {
        m_imageWidth = 0;
        m_imageHeight = 0;
        qWarning() << "CanvasController: cannot load image:" << imagePath;
    }

    m_dirty = false;
    emit dirtyChanged();
    emit currentImageChanged();
    emit canvasUpdateRequested();
}

void CanvasController::fitToView(qreal viewWidth, qreal viewHeight)
{
    if (m_imageWidth <= 0 || m_imageHeight <= 0) return;

    qreal scaleX = viewWidth / m_imageWidth;
    qreal scaleY = viewHeight / m_imageHeight;
    m_zoom = qMin(scaleX, scaleY) * 0.9;  // 90% to add padding

    // Center the image
    m_panX = (viewWidth - m_imageWidth * m_zoom) / 2.0;
    m_panY = (viewHeight - m_imageHeight * m_zoom) / 2.0;

    emit zoomChanged();
    emit panChanged();
    emit canvasUpdateRequested();
}

void CanvasController::resetView()
{
    m_zoom = 1.0;
    m_panX = 0;
    m_panY = 0;
    emit zoomChanged();
    emit panChanged();
    emit canvasUpdateRequested();
}

qreal CanvasController::imageToCanvasX(qreal imgX) const
{
    return imgX * m_imageWidth * m_zoom + m_panX;
}

qreal CanvasController::imageToCanvasY(qreal imgY) const
{
    return imgY * m_imageHeight * m_zoom + m_panY;
}

qreal CanvasController::canvasToImageX(qreal canvasX) const
{
    if (m_imageWidth <= 0 || m_zoom <= 0) return 0;
    return (canvasX - m_panX) / (m_imageWidth * m_zoom);
}

qreal CanvasController::canvasToImageY(qreal canvasY) const
{
    if (m_imageHeight <= 0 || m_zoom <= 0) return 0;
    return (canvasY - m_panY) / (m_imageHeight * m_zoom);
}

void CanvasController::markDirty()
{
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged();
    }
}

void CanvasController::clearDirty()
{
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged();
    }
}
