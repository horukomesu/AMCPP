#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QMenu>
#include <QList>
#include <QPointF>

struct ViewerMarker {
    float x;
    float y;
    QString name;
    float error = 0.0f;
};

class ImageViewer : public QGraphicsView
{
    Q_OBJECT
public:
    explicit ImageViewer(QWidget *parent = nullptr);

    void loadImage(const QImage &img, bool keepTransform = false);
    void setMarkers(const QList<ViewerMarker> &markers);
    void setAddingLocator(bool adding);

signals:
    void locatorAdded(float x, float y);
    void navigate(int step);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QGraphicsPixmapItem *m_pixmapItem;
    QList<QGraphicsItem *> m_markerItems;
    bool m_panning;
    QPoint m_panStart;
    bool m_addingLocator;
    qreal m_zoomStep;
};

#endif // IMAGEVIEWER_H
