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
    /// 隐藏标注渲染开关（纯看图模式，对标 DLTools 标注页「隐藏标注」Toggle）
    Q_PROPERTY(bool annotationsVisible READ annotationsVisible WRITE setAnnotationsVisible NOTIFY annotationsVisibleChanged)

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

    bool annotationsVisible() const { return m_annotationsVisible; }
    void setAnnotationsVisible(bool visible);

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
    /// 取消未闭合的多边形（切换工具/样本前调用）
    Q_INVOKABLE void cancelCurrentPolygon();
    /// 是否正在绘制多边形（未闭合）
    Q_INVOKABLE bool isDrawingPolygon() const { return m_isDrawingPolygon; }
    /// 当前第一个选中行（无选中返回 -1）；供实例表格滚动联动
    Q_INVOKABLE int selectedRow() const;

signals:
    void controllerChanged();
    void annotationModelChanged();
    void currentClassIndexChanged();
    void currentClassNameChanged();
    void shapeModeChanged();
    void interactionModeChanged();
    void annotationsVisibleChanged();
    /// 画布选中集变化（点击命中/清空/多选切换时发出），实例表格据此联动
    void selectionChanged();
    void annotationModified();
    void undoAvailabilityChanged();
    void navigatePrevious();
    void navigateNext();
    /// P1-6 跳到下一张未标注（Ctrl+Shift+D）
    void navigateNextUnlabeledRequested();
    /// P1-5 沿用上一帧标注（Ctrl+P，X-AnyLabeling keep_prev 对标）
    void keepPrevRequested();
    void saveRequested();
    void editLabelRequested(int annotationIndex);
    void changeClassRequested(int direction);
    /// 数字键 1-9 切换类别（信号驱动：QML 侧同步 selectedClassId）
    void classSelectRequested(int classIndex);
    /// C++ 侧切换工具（快捷键 W/O/P/Esc）后通知 QML 同步工具栏与 shapeType
    void drawToolChanged(int shapeMode, const QString &interactionMode);

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
    /// 顶点编辑：命中选中多边形的顶点索引（未命中返回 -1）
    int hitTestPolygonVertex(const QPointF& canvasPos, int row) const;
    QVector<QPointF> computeHandlePositions(const QRectF& rect, float angle = 0) const;

    void pushUndo();
    void applyUndoState(const UndoEntry& entry);

    void updateCursor(const QPointF& canvasPos);

    /// P1-24：经 ImageDecodeCache 加载并填充 m_image/m_loadedPath
    void loadSharedImage(const QString& imagePath);

    CanvasController* m_controller = nullptr;
    AnnotationModel* m_model = nullptr;
    QImage m_image;
    /// 当前已加载图片的路径，用于避免同一图重复解码
    QString m_loadedPath;
    qreal m_imageWidth = 0;
    qreal m_imageHeight = 0;

    int m_currentClassIndex = 0;
    QString m_currentClassName = QStringLiteral("class_0");
    int m_shapeMode = 0;
    QString m_interactionMode = QStringLiteral("select");
    bool m_annotationsVisible = true;

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

    // 顶点编辑（多边形）：>=0 时拖拽的是该行标注的第 m_dragVertexIndex 个顶点
    int m_dragVertexIndex = -1;
    // 顶点编辑（多边形）：当前选中的顶点（release 后保留，供 Backspace 删点）
    int m_selectedVertexRow = -1;
    int m_selectedVertexIndex = -1;

    bool m_isPanning = false;
    QPointF m_panStart;
    qreal m_panStartX = 0, m_panStartY = 0;

    int m_hoveredRow = -1;
    HandlePosition m_hoveredHandle = NoHandle;

    bool m_spaceHeld = false;

    // 用户是否手动缩放/平移过（true 后画布尺寸变化不再自动重新适配）
    bool m_userAdjustedView = false;

    QVector<AnnotationSnapshot> m_clipboard;

    QVector<UndoEntry> m_undoStack;
    int m_undoIndex = -1;
    static constexpr int MAX_UNDO = 50;
};

#endif // ANNOTCANVASITEM_H
