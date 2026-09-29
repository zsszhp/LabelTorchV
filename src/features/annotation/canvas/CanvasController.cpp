#include "CanvasController.h"
#include "utils/Log.h"
#include <QImage>
#include <QImageReader>

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

    // P1-24：只读图像头拿宽高，避免整图解码（4K 图解码耗时且占内存，
    // 像素数据由 AnnotCanvasItem 通过 ImageDecodeCache 共享缓存持有）
    QImageReader reader(imagePath);
    const QSize sz = reader.size();
    if (sz.isValid()) {
        m_imageWidth = sz.width();
        m_imageHeight = sz.height();
    } else {
        m_imageWidth = 0;
        m_imageHeight = 0;
        ltError(LT_LOG_ANNOTATION()) << "cannot read image size:" << imagePath
                                     << reader.errorString();
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

void CanvasController::setPolygonDrawing(bool drawing)
{
    ltTrace(LT_LOG_ANNOTATION()) << "drawing=" << drawing << "current=" << m_polygonDrawing;
    if (m_polygonDrawing == drawing) return;
    m_polygonDrawing = drawing;
    emit polygonDrawingChanged();
    ltInfo(LT_LOG_ANNOTATION()) << "Polygon drawing changed to" << drawing;
}

void CanvasController::cancelDrawing()
{
    ltTrace(LT_LOG_ANNOTATION()) << "cancelDrawing";
    if (m_polygonDrawing) {
        m_polygonDrawing = false;
        emit polygonDrawingChanged();
    }
    // 切换回选择模式
    if (m_drawMode != QStringLiteral("select")) {
        m_drawMode = QStringLiteral("select");
        emit drawModeChanged();
    }
    emit canvasUpdateRequested();
    ltInfo(LT_LOG_ANNOTATION()) << "Drawing cancelled, switched to select mode";
}
