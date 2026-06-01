#include "AnnotCanvasItem.h"
#include "CanvasController.h"
#include "../AnnotationModel.h"
#include "utils/Log.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QCursor>

static constexpr float HANDLE_SIZE = 6.0f;
static constexpr float HANDLE_HIT_RADIUS = 8.0f;
static constexpr float MIN_DRAW_SIZE = 5.0f;
static constexpr float MIN_ANNOTATION_SIZE = 0.005f;

static QColor classColor(int classIndex)
{
    static const QColor colors[] = {
        QColor("#FF4A70"), QColor("#8B5CF6"), QColor("#06B6D4"), QColor("#F59E0B"),
        QColor("#10B981"), QColor("#EF4444"), QColor("#3B82F6"), QColor("#EC4899"),
        QColor("#14B8A6"), QColor("#F97316")
    };
    return colors[classIndex % 10];
}

AnnotCanvasItem::AnnotCanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFocus(true);
    ltInfo(LT_LOG_ANNOTATION()) << "AnnotCanvasItem created";
}

void AnnotCanvasItem::setController(CanvasController* ctrl)
{
    if (m_controller == ctrl) return;
    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }
    m_controller = ctrl;
    if (m_controller) {
        connect(m_controller, &CanvasController::canvasUpdateRequested, this, [this]() { update(); });
        connect(m_controller, &CanvasController::currentImageChanged, this, [this]() {
            if (m_controller && !m_controller->currentImagePath().isEmpty()) {
                m_image.load(m_controller->currentImagePath());
                if (!m_image.isNull()) {
                    m_imageWidth = m_image.width();
                    m_imageHeight = m_image.height();
                }
            } else {
                m_image = QImage();
                m_imageWidth = 0;
                m_imageHeight = 0;
            }
            update();
        });
    }
    emit controllerChanged();
    update();
}

void AnnotCanvasItem::setAnnotationModel(AnnotationModel* model)
{
    if (m_model == model) return;
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &AnnotationModel::countChanged, this, [this]() { update(); });
    }
    emit annotationModelChanged();
    update();
}

void AnnotCanvasItem::setCurrentClassIndex(int idx)
{
    if (m_currentClassIndex == idx) return;
    m_currentClassIndex = idx;
    emit currentClassIndexChanged();
}

void AnnotCanvasItem::setCurrentClassName(const QString& name)
{
    if (m_currentClassName == name) return;
    m_currentClassName = name;
    emit currentClassNameChanged();
}

void AnnotCanvasItem::setShapeMode(int mode)
{
    if (m_shapeMode == mode) return;
    m_shapeMode = mode;
    emit shapeModeChanged();
}

void AnnotCanvasItem::setInteractionMode(const QString& mode)
{
    if (m_interactionMode == mode) return;
    m_interactionMode = mode;
    emit interactionModeChanged();
    update();
}

void AnnotCanvasItem::loadImage(const QString& imagePath, const QString& labelPath)
{
    ltInfo(LT_LOG_ANNOTATION()) << "Loading image:" << imagePath;

    m_image.load(imagePath);
    if (m_image.isNull()) {
        ltError(LT_LOG_ANNOTATION()) << "Failed to load image:" << imagePath;
        m_imageWidth = 0;
        m_imageHeight = 0;
    } else {
        m_imageWidth = m_image.width();
        m_imageHeight = m_image.height();
    }

    if (m_controller) {
        m_controller->loadImage(imagePath, labelPath);
    }

    m_undoStack.clear();
    m_undoIndex = -1;
    emit undoAvailabilityChanged();

    update();
}

void AnnotCanvasItem::fitToView()
{
    if (m_controller && m_imageWidth > 0 && m_imageHeight > 0) {
        m_controller->fitToView(width(), height());
    }
    update();
}

void AnnotCanvasItem::resetView()
{
    if (m_controller) {
        m_controller->resetView();
    }
    update();
}

void AnnotCanvasItem::paint(QPainter* painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter->fillRect(boundingRect(), QColor("#0D0E15"));

    drawImage(painter);
    drawAnnotations(painter);

    if (m_isDrawing) {
        drawDrawingRect(painter);
    }

    if (m_interactionMode == QStringLiteral("draw") && !m_isDrawing && m_imageWidth > 0) {
        drawCrosshair(painter);
    }
}

void AnnotCanvasItem::drawImage(QPainter* painter)
{
    if (m_image.isNull() || !m_controller) return;

    qreal z = m_controller->zoom();
    qreal px = m_controller->panX();
    qreal py = m_controller->panY();

    QRectF targetRect(px, py, m_imageWidth * z, m_imageHeight * z);
    painter->drawImage(targetRect, m_image, QRectF(0, 0, m_imageWidth, m_imageHeight));

    QPen borderPen(QColor("#3A3F55"), 1);
    painter->setPen(borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(targetRect);
}

void AnnotCanvasItem::drawAnnotations(QPainter* painter)
{
    if (!m_model) return;
    int count = m_model->rowCount();
    for (int i = 0; i < count; i++) {
        drawSingleAnnotation(painter, i);
    }
    for (int i = 0; i < count; i++) {
        QModelIndex idx = m_model->index(i, 0);
        bool selected = m_model->data(idx, AnnotationModel::IsSelectedRole).toBool();
        if (selected) {
            drawHandles(painter, i);
        }
    }
}

void AnnotCanvasItem::drawSingleAnnotation(QPainter* painter, int row)
{
    if (!m_model) return;
    QModelIndex idx = m_model->index(row, 0);

    float cx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
    float cy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
    float w = m_model->data(idx, AnnotationModel::WRole).toFloat();
    float h = m_model->data(idx, AnnotationModel::HRole).toFloat();
    float angle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();
    int classIdx = m_model->data(idx, AnnotationModel::ClassIndexRole).toInt();
    QString className = m_model->data(idx, AnnotationModel::ClassNameRole).toString();
    bool selected = m_model->data(idx, AnnotationModel::IsSelectedRole).toBool();

    if (cx < 0 || cy < 0 || w <= 0 || h <= 0) return;

    QColor color = classColor(classIdx);

    QPointF center = imageToCanvas(cx, cy);
    QPointF halfSize(
        (w * m_imageWidth * (m_controller ? m_controller->zoom() : 1.0)) / 2.0,
        (h * m_imageHeight * (m_controller ? m_controller->zoom() : 1.0)) / 2.0
    );

    QRectF rect(center.x() - halfSize.x(), center.y() - halfSize.y(),
                halfSize.x() * 2, halfSize.y() * 2);

    painter->save();

    if (m_shapeMode == 1 && angle != 0) {
        painter->translate(center);
        painter->rotate(angle);
        painter->translate(-center);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(color.red(), color.green(), color.blue(), 35));
    painter->drawRect(rect);

    QPen outlinePen(color, selected ? 2.5 : 1.5);
    painter->setPen(outlinePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);

    if (m_shapeMode == 1 && angle != 0) {
        painter->setPen(QPen(QColor(255, 255, 255, 100), 1, Qt::DashLine));
        painter->drawLine(center, QPointF(center.x() + halfSize.x(), center.y()));
    }

    painter->restore();

    QString label = className.isEmpty() ? QStringLiteral("class_%1").arg(classIdx) : className;
    QFont labelFont(QStringLiteral("Segoe UI"), 10, QFont::Bold);
    QFontMetrics fm(labelFont);
    qreal textW = fm.horizontalAdvance(label) + 8;
    qreal textH = fm.height() + 4;

    QPointF labelPos = center - halfSize;
    if (m_shapeMode == 1 && angle != 0) {
        qreal rad = angle * M_PI / 180.0;
        labelPos = center + QPointF(-halfSize.x() * cos(rad) + halfSize.y() * sin(rad),
                                    -halfSize.x() * sin(rad) - halfSize.y() * cos(rad));
    }
    labelPos.setY(labelPos.y() - textH);

    QRectF labelBg(labelPos.x(), labelPos.y(), textW, textH);
    painter->fillRect(labelBg, color);
    painter->setPen(QColor("#0D0E15"));
    painter->setFont(labelFont);
    painter->drawText(labelBg.adjusted(4, 2, -4, -2), Qt::AlignVCenter | Qt::AlignLeft, label);
}

void AnnotCanvasItem::drawHandles(QPainter* painter, int row)
{
    if (!m_model) return;
    QModelIndex idx = m_model->index(row, 0);

    float cx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
    float cy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
    float w = m_model->data(idx, AnnotationModel::WRole).toFloat();
    float h = m_model->data(idx, AnnotationModel::HRole).toFloat();
    float angle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();

    QRectF rect = annotationRect(row);
    QVector<QPointF> handles = computeHandlePositions(rect, angle);

    painter->setPen(QPen(QColor("#FFFFFF"), 1));
    painter->setBrush(QColor("#FF4A70"));
    for (const auto& hp : handles) {
        painter->drawRect(QRectF(hp.x() - HANDLE_SIZE / 2, hp.y() - HANDLE_SIZE / 2,
                                 HANDLE_SIZE, HANDLE_SIZE));
    }
}

void AnnotCanvasItem::drawDrawingRect(QPainter* painter)
{
    qreal x = qMin(m_drawStart.x(), m_drawCurrent.x());
    qreal y = qMin(m_drawStart.y(), m_drawCurrent.y());
    qreal w = qAbs(m_drawCurrent.x() - m_drawStart.x());
    qreal h = qAbs(m_drawCurrent.y() - m_drawStart.y());

    QColor color = classColor(m_currentClassIndex);
    painter->setPen(QPen(color, 1.5, Qt::DashLine));
    painter->setBrush(QColor(color.red(), color.green(), color.blue(), 25));
    painter->drawRect(QRectF(x, y, w, h));
}

void AnnotCanvasItem::drawCrosshair(QPainter* painter)
{
    QPointF cursor = mapFromGlobal(QCursor::pos());
    if (!boundingRect().contains(cursor)) return;

    QPen crossPen(QColor(255, 255, 255, 60), 1, Qt::DotLine);
    painter->setPen(crossPen);
    painter->drawLine(QPointF(0, cursor.y()), QPointF(width(), cursor.y()));
    painter->drawLine(QPointF(cursor.x(), 0), QPointF(cursor.x(), height()));
}

QPointF AnnotCanvasItem::imageToCanvas(float imgX, float imgY) const
{
    if (!m_controller) return QPointF(imgX, imgY);
    qreal z = m_controller->zoom();
    return QPointF(imgX * m_imageWidth * z + m_controller->panX(),
                   imgY * m_imageHeight * z + m_controller->panY());
}

QPointF AnnotCanvasItem::canvasToImage(qreal canvasX, qreal canvasY) const
{
    if (!m_controller || m_imageWidth <= 0 || m_imageHeight <= 0) return QPointF();
    qreal z = m_controller->zoom();
    if (z <= 0) return QPointF();
    return QPointF((canvasX - m_controller->panX()) / (m_imageWidth * z),
                   (canvasY - m_controller->panY()) / (m_imageHeight * z));
}

QRectF AnnotCanvasItem::annotationRect(int row) const
{
    if (!m_model) return QRectF();
    QModelIndex idx = m_model->index(row, 0);

    float cx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
    float cy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
    float w = m_model->data(idx, AnnotationModel::WRole).toFloat();
    float h = m_model->data(idx, AnnotationModel::HRole).toFloat();

    QPointF center = imageToCanvas(cx, cy);
    qreal z = m_controller ? m_controller->zoom() : 1.0;
    QPointF halfSize((w * m_imageWidth * z) / 2.0, (h * m_imageHeight * z) / 2.0);

    return QRectF(center.x() - halfSize.x(), center.y() - halfSize.y(),
                  halfSize.x() * 2, halfSize.y() * 2);
}

AnnotCanvasItem::HandlePosition AnnotCanvasItem::hitTestHandle(const QPointF& canvasPos, int row)
{
    if (!m_model) return NoHandle;
    QModelIndex idx = m_model->index(row, 0);
    if (!m_model->data(idx, AnnotationModel::IsSelectedRole).toBool()) return NoHandle;

    float angle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();
    QRectF rect = annotationRect(row);
    QVector<QPointF> handles = computeHandlePositions(rect, angle);

    QVector<HandlePosition> positions = {
        TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleRight,
        BottomLeft, BottomCenter, BottomRight
    };

    for (int i = 0; i < qMin(handles.size(), positions.size()); i++) {
        QPointF diff = canvasPos - handles[i];
        if (diff.x() * diff.x() + diff.y() * diff.y() <= HANDLE_HIT_RADIUS * HANDLE_HIT_RADIUS) {
            return positions[i];
        }
    }
    return NoHandle;
}

int AnnotCanvasItem::hitTestAnnotation(const QPointF& canvasPos)
{
    if (!m_model) return -1;
    QPointF imgPos = canvasToImage(canvasPos.x(), canvasPos.y());

    for (int i = m_model->rowCount() - 1; i >= 0; i--) {
        QModelIndex idx = m_model->index(i, 0);
        float cx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
        float cy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
        float w = m_model->data(idx, AnnotationModel::WRole).toFloat();
        float h = m_model->data(idx, AnnotationModel::HRole).toFloat();
        float angle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();

        if (m_shapeMode == 1 && angle != 0) {
            float dx = imgPos.x() - cx;
            float dy = imgPos.y() - cy;
            float rad = -angle * M_PI / 180.0f;
            float localX = dx * cos(rad) - dy * sin(rad);
            float localY = dx * sin(rad) + dy * cos(rad);
            if (qAbs(localX) <= w / 2 && qAbs(localY) <= h / 2) return i;
        } else {
            if (imgPos.x() >= cx - w / 2 && imgPos.x() <= cx + w / 2 &&
                imgPos.y() >= cy - h / 2 && imgPos.y() <= cy + h / 2) return i;
        }
    }
    return -1;
}

QVector<QPointF> AnnotCanvasItem::computeHandlePositions(const QRectF& rect, float angle) const
{
    QVector<QPointF> handles;
    QPointF center = rect.center();

    QVector<QPointF> corners = {
        rect.topLeft(), QPointF(rect.center().x(), rect.top()),
        rect.topRight(), QPointF(rect.left(), rect.center().y()),
        QPointF(rect.right(), rect.center().y()),
        rect.bottomLeft(), QPointF(rect.center().x(), rect.bottom()),
        rect.bottomRight()
    };

    if (m_shapeMode == 1 && angle != 0) {
        qreal rad = angle * M_PI / 180.0;
        for (auto& pt : corners) {
            QPointF d = pt - center;
            handles.append(center + QPointF(
                d.x() * cos(rad) - d.y() * sin(rad),
                d.x() * sin(rad) + d.y() * cos(rad)
            ));
        }
    } else {
        handles = corners;
    }
    return handles;
}

void AnnotCanvasItem::mousePressEvent(QMouseEvent* event)
{
    forceActiveFocus();

    if (m_spaceHeld || event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panStart = event->position();
        if (m_controller) {
            m_panStartX = m_controller->panX();
            m_panStartY = m_controller->panY();
        }
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->position();

        if (m_interactionMode == QStringLiteral("draw")) {
            m_isDrawing = true;
            m_drawStart = pos;
            m_drawCurrent = pos;
            event->accept();
            return;
        }

        for (int i = 0; i < (m_model ? m_model->rowCount() : 0); i++) {
            QModelIndex idx = m_model->index(i, 0);
            if (m_model->data(idx, AnnotationModel::IsSelectedRole).toBool()) {
                HandlePosition hp = hitTestHandle(pos, i);
                if (hp != NoHandle) {
                    pushUndo();
                    m_isDragging = true;
                    m_dragAnnotationRow = i;
                    m_dragHandle = hp;
                    m_dragStart = pos;
                    m_dragOrigCx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
                    m_dragOrigCy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
                    m_dragOrigW = m_model->data(idx, AnnotationModel::WRole).toFloat();
                    m_dragOrigH = m_model->data(idx, AnnotationModel::HRole).toFloat();
                    m_dragOrigAngle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();
                    event->accept();
                    return;
                }
            }
        }

        int hitRow = hitTestAnnotation(pos);
        if (hitRow >= 0) {
            for (int i = 0; i < (m_model ? m_model->rowCount() : 0); i++) {
                m_model->setSelected(i, false);
            }
            m_model->setSelected(hitRow, true);

            pushUndo();
            m_isDragging = true;
            m_dragAnnotationRow = hitRow;
            m_dragHandle = NoHandle;
            m_dragStart = pos;
            QModelIndex idx = m_model->index(hitRow, 0);
            m_dragOrigCx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
            m_dragOrigCy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
            m_dragOrigW = m_model->data(idx, AnnotationModel::WRole).toFloat();
            m_dragOrigH = m_model->data(idx, AnnotationModel::HRole).toFloat();
            m_dragOrigAngle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();
            event->accept();
            update();
            return;
        }

        for (int i = 0; i < (m_model ? m_model->rowCount() : 0); i++) {
            m_model->setSelected(i, false);
        }
        update();
        event->accept();
    }
}

void AnnotCanvasItem::mouseMoveEvent(QMouseEvent* event)
{
    QPointF pos = event->position();

    if (m_isPanning && m_controller) {
        QPointF delta = pos - m_panStart;
        m_controller->setPanX(m_panStartX + delta.x());
        m_controller->setPanY(m_panStartY + delta.y());
        update();
        event->accept();
        return;
    }

    if (m_isDrawing) {
        m_drawCurrent = pos;
        update();
        event->accept();
        return;
    }

    if (m_isDragging && m_model && m_controller) {
        QModelIndex idx = m_model->index(m_dragAnnotationRow, 0);
        qreal z = m_controller->zoom();

        QPointF imgDelta = canvasToImage(pos.x(), pos.y()) - canvasToImage(m_dragStart.x(), m_dragStart.y());

        if (m_dragHandle == NoHandle) {
            float newCx = m_dragOrigCx + imgDelta.x();
            float newCy = m_dragOrigCy + imgDelta.y();
            newCx = qBound(m_dragOrigW / 2.0f, newCx, 1.0f - m_dragOrigW / 2.0f);
            newCy = qBound(m_dragOrigH / 2.0f, newCy, 1.0f - m_dragOrigH / 2.0f);
            if (m_shapeMode == 1) {
                m_model->updateOBBGeometry(m_dragAnnotationRow, newCx, newCy, m_dragOrigW, m_dragOrigH, m_dragOrigAngle);
            } else {
                m_model->updateGeometry(m_dragAnnotationRow, newCx, newCy, m_dragOrigW, m_dragOrigH);
            }
        } else {
            float dCx = imgDelta.x();
            float dCy = imgDelta.y();
            float newCx = m_dragOrigCx;
            float newCy = m_dragOrigCy;
            float newW = m_dragOrigW;
            float newH = m_dragOrigH;

            switch (m_dragHandle) {
            case TopLeft:
                newCx = m_dragOrigCx + dCx / 2;
                newCy = m_dragOrigCy + dCy / 2;
                newW = m_dragOrigW - dCx;
                newH = m_dragOrigH - dCy;
                break;
            case TopCenter:
                newCy = m_dragOrigCy + dCy / 2;
                newH = m_dragOrigH - dCy;
                break;
            case TopRight:
                newCx = m_dragOrigCx + dCx / 2;
                newCy = m_dragOrigCy + dCy / 2;
                newW = m_dragOrigW + dCx;
                newH = m_dragOrigH - dCy;
                break;
            case MiddleLeft:
                newCx = m_dragOrigCx + dCx / 2;
                newW = m_dragOrigW - dCx;
                break;
            case MiddleRight:
                newCx = m_dragOrigCx + dCx / 2;
                newW = m_dragOrigW + dCx;
                break;
            case BottomLeft:
                newCx = m_dragOrigCx + dCx / 2;
                newCy = m_dragOrigCy + dCy / 2;
                newW = m_dragOrigW - dCx;
                newH = m_dragOrigH + dCy;
                break;
            case BottomCenter:
                newCy = m_dragOrigCy + dCy / 2;
                newH = m_dragOrigH + dCy;
                break;
            case BottomRight:
                newCx = m_dragOrigCx + dCx / 2;
                newCy = m_dragOrigCy + dCy / 2;
                newW = m_dragOrigW + dCx;
                newH = m_dragOrigH + dCy;
                break;
            default:
                break;
            }

            newW = qMax(MIN_ANNOTATION_SIZE, newW);
            newH = qMax(MIN_ANNOTATION_SIZE, newH);
            newCx = qBound(newW / 2.0f, newCx, 1.0f - newW / 2.0f);
            newCy = qBound(newH / 2.0f, newCy, 1.0f - newH / 2.0f);

            if (m_shapeMode == 1) {
                m_model->updateOBBGeometry(m_dragAnnotationRow, newCx, newCy, newW, newH, m_dragOrigAngle);
            } else {
                m_model->updateGeometry(m_dragAnnotationRow, newCx, newCy, newW, newH);
            }
        }

        if (m_controller) m_controller->markDirty();
        emit annotationModified();
        update();
        event->accept();
        return;
    }

    updateCursor(pos);
    event->accept();
}

void AnnotCanvasItem::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isPanning) {
        m_isPanning = false;
        setCursor(m_spaceHeld ? Qt::OpenHandCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (m_isDrawing && event->button() == Qt::LeftButton) {
        m_isDrawing = false;

        qreal dx = qAbs(m_drawCurrent.x() - m_drawStart.x());
        qreal dy = qAbs(m_drawCurrent.y() - m_drawStart.y());

        if (dx > MIN_DRAW_SIZE && dy > MIN_DRAW_SIZE && m_model) {
            QPointF imgStart = canvasToImage(qMin(m_drawStart.x(), m_drawCurrent.x()),
                                              qMin(m_drawStart.y(), m_drawCurrent.y()));
            QPointF imgEnd = canvasToImage(qMax(m_drawStart.x(), m_drawCurrent.x()),
                                            qMax(m_drawStart.y(), m_drawCurrent.y()));

            float cx = (imgStart.x() + imgEnd.x()) / 2.0f;
            float cy = (imgStart.y() + imgEnd.y()) / 2.0f;
            float w = imgEnd.x() - imgStart.x();
            float h = imgEnd.y() - imgStart.y();

            if (w > MIN_ANNOTATION_SIZE && h > MIN_ANNOTATION_SIZE &&
                cx >= 0 && cy >= 0 && cx <= 1 && cy <= 1) {
                pushUndo();
                if (m_shapeMode == 1) {
                    m_model->addOBBAnnotation(m_currentClassIndex, m_currentClassName, cx, cy, w, h, 0.0f);
                } else {
                    m_model->addAnnotation(m_currentClassIndex, m_currentClassName, cx, cy, w, h);
                }
                if (m_controller) m_controller->markDirty();
                emit annotationModified();
            }
        }
        update();
        event->accept();
        return;
    }

    if (m_isDragging) {
        m_isDragging = false;
        m_dragAnnotationRow = -1;
        m_dragHandle = NoHandle;
        event->accept();
        return;
    }

    event->accept();
}

void AnnotCanvasItem::wheelEvent(QWheelEvent* event)
{
    if (!m_controller) return;

    QPointF pos = event->position();
    qreal factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    qreal newZoom = m_controller->zoom() * factor;
    newZoom = qBound(0.05, newZoom, 50.0);

    qreal currentZoom = m_controller->zoom();
    if (currentZoom <= 0) currentZoom = 1.0;

    qreal newPanX = pos.x() - (pos.x() - m_controller->panX()) * (newZoom / currentZoom);
    qreal newPanY = pos.y() - (pos.y() - m_controller->panY()) * (newZoom / currentZoom);

    m_controller->setPanX(newPanX);
    m_controller->setPanY(newPanY);
    m_controller->setZoom(newZoom);

    event->accept();
}

void AnnotCanvasItem::hoverMoveEvent(QHoverEvent* event)
{
    updateCursor(event->position());
    event->accept();
}

void AnnotCanvasItem::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceHeld = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelected();
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Z) {
            if (event->modifiers() & Qt::ShiftModifier) {
                redo();
            } else {
                undo();
            }
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_A) {
            selectAll();
            event->accept();
            return;
        }
    }

    event->ignore();
}

void AnnotCanvasItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    update();
}

void AnnotCanvasItem::updateCursor(const QPointF& canvasPos)
{
    if (m_spaceHeld) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (m_interactionMode == QStringLiteral("draw")) {
        setCursor(Qt::CrossCursor);
        return;
    }

    for (int i = 0; i < (m_model ? m_model->rowCount() : 0); i++) {
        QModelIndex idx = m_model->index(i, 0);
        if (m_model->data(idx, AnnotationModel::IsSelectedRole).toBool()) {
            HandlePosition hp = hitTestHandle(canvasPos, i);
            if (hp != NoHandle) {
                switch (hp) {
                case TopLeft: case BottomRight:
                    setCursor(Qt::SizeFDiagCursor); return;
                case TopRight: case BottomLeft:
                    setCursor(Qt::SizeBDiagCursor); return;
                case TopCenter: case BottomCenter:
                    setCursor(Qt::SizeVerCursor); return;
                case MiddleLeft: case MiddleRight:
                    setCursor(Qt::SizeHorCursor); return;
                default:
                    setCursor(Qt::ArrowCursor); return;
                }
            }
        }
    }

    int hit = hitTestAnnotation(canvasPos);
    setCursor(hit >= 0 ? Qt::SizeAllCursor : Qt::ArrowCursor);
}

void AnnotCanvasItem::pushUndo()
{
    if (!m_model) return;

    UndoEntry entry;
    for (int i = 0; i < m_model->rowCount(); i++) {
        QModelIndex idx = m_model->index(i, 0);
        AnnotationSnapshot snap;
        snap.classIndex = m_model->data(idx, AnnotationModel::ClassIndexRole).toInt();
        snap.className = m_model->data(idx, AnnotationModel::ClassNameRole).toString();
        snap.cx = m_model->data(idx, AnnotationModel::CxRole).toFloat();
        snap.cy = m_model->data(idx, AnnotationModel::CyRole).toFloat();
        snap.w = m_model->data(idx, AnnotationModel::WRole).toFloat();
        snap.h = m_model->data(idx, AnnotationModel::HRole).toFloat();
        snap.angle = m_model->data(idx, AnnotationModel::AngleRole).toFloat();
        snap.isSelected = m_model->data(idx, AnnotationModel::IsSelectedRole).toBool();
        entry.state.append(snap);
    }

    while (m_undoIndex < m_undoStack.size() - 1) {
        m_undoStack.removeLast();
    }

    m_undoStack.append(entry);
    if (m_undoStack.size() > MAX_UNDO) {
        m_undoStack.removeFirst();
    }
    m_undoIndex = m_undoStack.size() - 1;

    emit undoAvailabilityChanged();
}

void AnnotCanvasItem::applyUndoState(const UndoEntry& entry)
{
    if (!m_model) return;

    while (m_model->rowCount() > 0) {
        m_model->removeAnnotation(0);
    }

    for (const auto& snap : entry.state) {
        if (m_shapeMode == 1 || snap.angle != 0) {
            m_model->addOBBAnnotation(snap.classIndex, snap.className,
                                       snap.cx, snap.cy, snap.w, snap.h, snap.angle);
        } else {
            m_model->addAnnotation(snap.classIndex, snap.className,
                                    snap.cx, snap.cy, snap.w, snap.h);
        }
    }

    if (m_controller) m_controller->markDirty();
    emit annotationModified();
    update();
}

void AnnotCanvasItem::undo()
{
    if (!canUndo()) return;
    if (m_undoIndex == m_undoStack.size() - 1) {
        pushUndo();
        m_undoIndex--;
    }
    m_undoIndex--;
    applyUndoState(m_undoStack[m_undoIndex]);
    emit undoAvailabilityChanged();
}

void AnnotCanvasItem::redo()
{
    if (!canRedo()) return;
    m_undoIndex++;
    applyUndoState(m_undoStack[m_undoIndex]);
    emit undoAvailabilityChanged();
}

bool AnnotCanvasItem::canUndo() const
{
    return m_undoIndex > 0;
}

bool AnnotCanvasItem::canRedo() const
{
    return m_undoIndex >= 0 && m_undoIndex < m_undoStack.size() - 1;
}

void AnnotCanvasItem::selectAll()
{
    if (!m_model) return;
    for (int i = 0; i < m_model->rowCount(); i++) {
        m_model->setSelected(i, true);
    }
    update();
}

void AnnotCanvasItem::deleteSelected()
{
    if (!m_model) return;
    pushUndo();
    for (int i = m_model->rowCount() - 1; i >= 0; i--) {
        QModelIndex idx = m_model->index(i, 0);
        if (m_model->data(idx, AnnotationModel::IsSelectedRole).toBool()) {
            m_model->removeAnnotation(i);
        }
    }
    if (m_controller) m_controller->markDirty();
    emit annotationModified();
    update();
}

void AnnotCanvasItem::commitUndoState()
{
    pushUndo();
}
