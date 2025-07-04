#include "Runner.h"
#include "Calibrator.h"
#include <QDebug>

Runner::Runner(QObject* parent) : QObject(parent)
{
    calibrator = new Calibrator(this);
}

void Runner::runExample(const QString& colmap)
{
    QStringList images; // fill with image paths
    calibrator->setImages(images);
    QMap<int,QMap<int,QPointF>> points; // fill with manual observations
    calibrator->setPointData(points);
    if (calibrator->calibrate(colmap)) {
        qDebug() << "Calibration done";
    } else {
        qDebug() << "Calibration failed";
    }
}

