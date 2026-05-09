#ifndef CANVASCONTROLLER_H
#define CANVASCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVariantList>

class AnnotationModel;

class CanvasController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentImagePath READ currentImagePath NOTIFY currentImageChanged)
    Q_PROPERTY(QString currentLabelPath READ currentLabelPath NOTIFY currentImageChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY panChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY panChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(QString drawMode READ drawMode WRITE setDrawMode NOTIFY drawModeChanged)

public:
    explicit CanvasController(QObject *parent = nullptr);

    QString currentImagePath() const { return m_currentImagePath; }
    QString currentLabelPath() const { return m_currentLabelPath; }
    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    qreal panX() const { return m_panX; }
    void setPanX(qreal x);
    qreal panY() const { return m_panY; }
    void setPanY(qreal y);
    bool dirty() const { return m_dirty; }
    QString drawMode() const { return m_drawMode; }
    void setDrawMode(const QString &mode);

    Q_INVOKABLE void loadImage(const QString &imagePath, const QString &labelPath);
    Q_INVOKABLE void fitToView(qreal viewWidth, qreal viewHeight);
    Q_INVOKABLE void resetView();

    // Coordinate transforms
    Q_INVOKABLE qreal imageToCanvasX(qreal imgX) const;
    Q_INVOKABLE qreal imageToCanvasY(qreal imgY) const;
    Q_INVOKABLE qreal canvasToImageX(qreal canvasX) const;
    Q_INVOKABLE qreal canvasToImageY(qreal canvasY) const;

    Q_INVOKABLE void markDirty();
    Q_INVOKABLE void clearDirty();

signals:
    void currentImageChanged();
    void zoomChanged();
    void panChanged();
    void dirtyChanged();
    void drawModeChanged();
    void canvasUpdateRequested();

private:
    QString m_currentImagePath;
    QString m_currentLabelPath;
    qreal m_zoom = 1.0;
    qreal m_panX = 0;
    qreal m_panY = 0;
    bool m_dirty = false;
    QString m_drawMode = "select";  // "select" or "draw"
    qreal m_imageWidth = 0;
    qreal m_imageHeight = 0;
};

#endif
