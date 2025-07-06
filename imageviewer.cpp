#include "imageviewer.h"
#include "tools.h"
#include "amutilities.h"
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>

ImageViewer::ImageViewer(QWidget *parent)
    : QGraphicsView(parent),
      m_pixmapItem(new QGraphicsPixmapItem()),
      m_panning(false),
      m_addingLocator(false),
      m_zoomStep(1.2),
      m_toolController(nullptr)
{
    setScene(new QGraphicsScene(this));
    scene()->addItem(m_pixmapItem);

    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::NoAnchor);
}

void ImageViewer::setToolController(ToolController *controller)
{
    m_toolController = controller;
}

void ImageViewer::loadImage(const QImage &img, bool keepTransform)
{
    QTransform current = transform();
    int h = horizontalScrollBar()->value();
    int v = verticalScrollBar()->value();

    scene()->setSceneRect(0, 0, img.width(), img.height());
    m_pixmapItem->setPixmap(QPixmap::fromImage(img));

    if (keepTransform) {
        setTransform(current);
        horizontalScrollBar()->setValue(h);
        verticalScrollBar()->setValue(v);
    } else {
        resetTransform();
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    }
}

void ImageViewer::setMarkers(const QList<ViewerMarker> &markers)
{
    for (QGraphicsItem *it : m_markerItems)
        scene()->removeItem(it);
    m_markerItems.clear();

    for (const ViewerMarker &m : markers) {
        QPointF pos(m.x * sceneRect().width(), m.y * sceneRect().height());
        QPen pen(errorToColor(m.error));
        pen.setWidth(m.highlight ? 2 : 1);
        QGraphicsLineItem *l1 = scene()->addLine(-5, 0, 5, 0, pen);
        QGraphicsLineItem *l2 = scene()->addLine(0, -5, 0, 5, pen);
        l1->setPos(pos);
        l2->setPos(pos);
        l1->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        l2->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        m_markerItems << l1 << l2;

        QGraphicsSimpleTextItem *txt = scene()->addSimpleText(m.name);
        txt->setPos(pos + QPointF(6, -6));
        txt->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        m_markerItems << txt;
    }
}

void ImageViewer::setAddingLocator(bool adding)
{
    m_addingLocator = adding;
    setCursor(adding ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ImageViewer::mousePressEvent(QMouseEvent *event)
{
    if (m_toolController) {
        m_toolController->mousePressEvent(event);
        if (event->isAccepted())
            return;
    }

    if (m_addingLocator && event->button() == Qt::LeftButton) {
        QPointF pos = mapToScene(event->pos());
        float x = pos.x() / sceneRect().width();
        float y = pos.y() / sceneRect().height();
        emit locatorAdded(x, y);
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_panStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
    if (event->button() == Qt::RightButton) {
        QMenu menu(this);

        QAction *action1 = menu.addAction(tr("Действие 1"));
        QAction *action2 = menu.addAction(tr("Действие 2"));
        QAction *selectedAction = menu.exec(event->globalPos());

        if (selectedAction == action1) {
            qDebug() << "Выбрано: Действие 1";
        } else if (selectedAction == action2) {
            qDebug() << "Выбрано: Действие 2";
        }

        return;
    }
}

void ImageViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_toolController) {
        m_toolController->mouseMoveEvent(event);
        if (event->isAccepted())
            return;
    }
    if (m_panning) {
        QPoint delta = event->pos() - m_panStart;
        m_panStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    } else {
        QGraphicsView::mouseMoveEvent(event);
    }
}

void ImageViewer::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_toolController) {
        m_toolController->mouseReleaseEvent(event);
        if (event->isAccepted())
            return;
    }
    if (event->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        setCursor(m_addingLocator ? Qt::CrossCursor : Qt::ArrowCursor);
    } else {
        QGraphicsView::mouseReleaseEvent(event);
    }
}

void ImageViewer::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int step = event->angleDelta().y() > 0 ? 1 : -1;
        emit navigate(step);
        return;
    }
    QPointF oldPos = mapToScene(event->position().toPoint());
    bool zoomOut = event->angleDelta().y() < 0;
    qreal factor = zoomOut ? 1.0 / m_zoomStep : m_zoomStep;
    scale(factor, factor);
    QPointF newPos = mapToScene(event->position().toPoint());
    QPointF delta = newPos - oldPos;
    translate(delta.x(), delta.y());
}

void ImageViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_pixmapItem->pixmap().isNull())
        fitInView(sceneRect(), Qt::KeepAspectRatio);
    QGraphicsView::mouseDoubleClickEvent(event);
}


