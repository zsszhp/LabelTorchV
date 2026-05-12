#include "CanvasController.h"
#include "utils/Log.h"
#include <QImage>

CanvasController::CanvasController(QObject *parent) : QObject(parent)
{
    ltTrace(LT_LOG_ANNOTATION()) << "parent=" << parent;
}

void CanvasController::setZoom(qreal z)
{
    ltTrace(LT_LOG_ANNOTATION()) << "z=" << z << "current=" << m_zoom;
    if (qFuzzyCompare(m_zoom, z)) return;
    m_zoom = z;
    emit zoomChanged();
    emit canvasUpdateRequested();
    ltInfo(LT_LOG_ANNOTATION()) << "Zoom changed to" << z;
}

void CanvasController::setPanX(qreal x)
{
    ltTrace(LT_LOG_ANNOTATION()) << "x=" << x << "current=" << m_panX;
    if (qFuzzyCompare(m_panX, x)) return;
    m_panX = x;
    emit panChanged();
    emit canvasUpdateRequested();
}

void CanvasController::setPanY(qreal y)
{
    ltTrace(LT_LOG_ANNOTATION()) << "y=" << y << "current=" << m_panY;
    if (qFuzzyCompare(m_panY, y)) return;
    m_panY = y;
    emit panChanged();
    emit canvasUpdateRequested();
}

void CanvasController::setDrawMode(const QString &mode)
{
    ltTrace(LT_LOG_ANNOTATION()) << "mode=" << mode << "current=" << m_drawMode;
    if (m_drawMode == mode) return;
    m_drawMode = mode;
    emit drawModeChanged();
    ltInfo(LT_LOG_ANNOTATION()) << "Draw mode changed to" << mode;
}

void CanvasController::loadImage(const QString &imagePath, const QString &labelPath)
{
    ltTrace(LT_LOG_ANNOTATION()) << "imagePath=" << imagePath << "labelPath=" << labelPath;

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
        ltError(LT_LOG_ANNOTATION()) << "cannot load image:" << imagePath;
    }

    m_dirty = false;
    emit dirtyChanged();
    emit currentImageChanged();
    emit canvasUpdateRequested();

    ltInfo(LT_LOG_ANNOTATION()) << "Image loaded:" << imagePath
                                << m_imageWidth << "x" << m_imageHeight;
}

void CanvasController::fitToView(qreal viewWidth, qreal viewHeight)
{
    ltTrace(LT_LOG_ANNOTATION()) << "viewWidth=" << viewWidth << "viewHeight=" << viewHeight;

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

    ltDebug(LT_LOG_ANNOTATION()) << "Fit to view: zoom=" << m_zoom
                                 << "pan=(" << m_panX << "," << m_panY << ")";
}

void CanvasController::resetView()
{
    ltTrace(LT_LOG_ANNOTATION()) << "resetting view";

    m_zoom = 1.0;
    m_panX = 0;
    m_panY = 0;
    emit zoomChanged();
    emit panChanged();
    emit canvasUpdateRequested();

    ltInfo(LT_LOG_ANNOTATION()) << "View reset to defaults";
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
    ltTrace(LT_LOG_ANNOTATION()) << "dirty=" << m_dirty;
    if (!m_dirty) {
        m_dirty = true;
        emit dirtyChanged();
        ltInfo(LT_LOG_ANNOTATION()) << "Canvas marked dirty";
    }
}

void CanvasController::clearDirty()
{
    ltTrace(LT_LOG_ANNOTATION()) << "dirty=" << m_dirty;
    if (m_dirty) {
        m_dirty = false;
        emit dirtyChanged();
        ltInfo(LT_LOG_ANNOTATION()) << "Canvas dirty flag cleared";
    }
}
