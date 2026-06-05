#ifndef ANNOTCANVASITEM_H
#define ANNOTCANVASITEM_H

#include <QQuickPaintedItem>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QColor>
#include <QVector>
#include "CanvasController.h"
#include "../AnnotationModel.h"
/**
 * @brief High-performance annotation canvas using QQuickPaintedItem + QPainter.
 *
 * Renders the current image and all annotations (HBB/OBB) with class-colored
 * outlines, fill, and labels. Handles mouse interaction for drawing new
 * annotations, selecting, moving, and resizing existing ones.
 *
 * Coordinate system:
 *   - Image coordinates: normalized [0,1] range (used by AnnotationModel)
 *   - Canvas coordinates: pixel coordinates of this QQuickItem
 *   - Transform: canvasX = imgX * imageWidth * zoom + panX
 */
class AnnotCanvasItem : public QQuickPaintedItem
{
    Q_OBJECT

    Q_PROPERTY(CanvasController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(AnnotationModel* annotationModel READ annotationModel WRITE setAnnotationModel NOTIFY annotationModelChanged)
    Q_PROPERTY(int currentClassIndex READ currentClassIndex WRITE setCurrentClassIndex NOTIFY currentClassIndexChanged)
    Q_PROPERTY(QString currentClassName READ currentClassName WRITE setCurrentClassName NOTIFY currentClassNameChanged)
    Q_PROPERTY(int shapeMode READ shapeMode WRITE setShapeMode NOTIFY shapeModeChanged)
    Q_PROPERTY(QString interactionMode READ interactionMode WRITE setInteractionMode NOTIFY interactionModeChanged)

public:
    enum HandlePosition {
        NoHandle = 0,
        TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleRight,
        BottomLeft, BottomCenter, BottomRight,
        RotationHandle
    };
    Q_ENUM(HandlePosition)

    explicit AnnotCanvasItem(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    CanvasController* controller() const { return m_controller; }
    void setController(CanvasController* ctrl);

    AnnotationModel* annotationModel() const { return m_model; }
    void setAnnotationModel(AnnotationModel* model);

    int currentClassIndex() const { return m_currentClassIndex; }
    void setCurrentClassIndex(int idx);

    QString currentClassName() const { return m_currentClassName; }
    void setCurrentClassName(const QString& name);

    int shapeMode() const { return m_shapeMode; }
    void setShapeMode(int mode);

    QString interactionMode() const { return m_interactionMode; }
    void setInteractionMode(const QString& mode);

    Q_INVOKABLE void loadImage(const QString& imagePath, const QString& labelPath);
    Q_INVOKABLE void fitToView();
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE bool canUndo() const;
    Q_INVOKABLE bool canRedo() const;
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void commitUndoState();
    Q_INVOKABLE void rotateSelected(float deltaAngle);
    Q_INVOKABLE void copySelected();
    Q_INVOKABLE void pasteClipboard();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void finishDrawing();
    Q_INVOKABLE void nudgeSelected(int dxPixels, int dyPixels);

signals:
    void controllerChanged();
    void annotationModelChanged();
    void currentClassIndexChanged();
    void currentClassNameChanged();
    void shapeModeChanged();
    void interactionModeChanged();
    void annotationModified();
    void undoAvailabilityChanged();
    void navigatePrevious();
    void navigateNext();
    void saveRequested();
    void editLabelRequested(int annotationIndex);
    void changeClassRequested(int direction);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    struct AnnotationSnapshot {
        int classIndex;
        QString className;
        float cx, cy, w, h, angle;
        bool isSelected;
        int shapeType = 0;
        QVector<QPointF> polygonPoints;
    };

    struct UndoEntry {
        QVector<AnnotationSnapshot> state;
    };

    void drawImage(QPainter* painter);
    void drawAnnotations(QPainter* painter);
    void drawSingleAnnotation(QPainter* painter, int row);
    void drawHandles(QPainter* painter, int row);
    void drawDrawingRect(QPainter* painter);
    void drawDrawingPolygon(QPainter* painter);
    void drawCrosshair(QPainter* painter);

    QPointF imageToCanvas(float imgX, float imgY) const;
    QPointF canvasToImage(qreal canvasX, qreal canvasY) const;
    QRectF annotationRect(int row) const;

    HandlePosition hitTestHandle(const QPointF& canvasPos, int row);
    int hitTestAnnotation(const QPointF& canvasPos);
    QVector<QPointF> computeHandlePositions(const QRectF& rect, float angle = 0) const;

    void pushUndo();
    void applyUndoState(const UndoEntry& entry);

    void updateCursor(const QPointF& canvasPos);

    CanvasController* m_controller = nullptr;
    AnnotationModel* m_model = nullptr;
    QImage m_image;
    qreal m_imageWidth = 0;
    qreal m_imageHeight = 0;

    int m_currentClassIndex = 0;
    QString m_currentClassName = QStringLiteral("class_0");
    int m_shapeMode = 0;
    QString m_interactionMode = QStringLiteral("select");

    bool m_isDrawing = false;
    QPointF m_drawStart;
    QPointF m_drawCurrent;

    bool m_isDrawingPolygon = false;
    QVector<QPointF> m_polygonPoints;
    QPointF m_polygonHoverPoint;
    bool m_nearStartPoint = false;
    static constexpr float SNAP_THRESHOLD = 10.0f;

    bool m_isDragging = false;
    int m_dragAnnotationRow = -1;
    HandlePosition m_dragHandle = NoHandle;
    QPointF m_dragStart;
    float m_dragOrigCx = 0, m_dragOrigCy = 0, m_dragOrigW = 0, m_dragOrigH = 0, m_dragOrigAngle = 0;

    bool m_isPanning = false;
    QPointF m_panStart;
    qreal m_panStartX = 0, m_panStartY = 0;

    int m_hoveredRow = -1;
    HandlePosition m_hoveredHandle = NoHandle;

    bool m_spaceHeld = false;

    QVector<AnnotationSnapshot> m_clipboard;

    QVector<UndoEntry> m_undoStack;
    int m_undoIndex = -1;
    static constexpr int MAX_UNDO = 50;
};

#endif // ANNOTCANVASITEM_H
