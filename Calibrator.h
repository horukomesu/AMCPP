#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QMatrix3x3>
#include <QMatrix4x4>

struct CameraResult
{
    QMatrix3x3 K;
    QMatrix4x4 pose; // world to camera
};

class Calibrator : public QObject
{
    Q_OBJECT
public:
    explicit Calibrator(QObject* parent = nullptr);

    void setImages(const QStringList& paths);
    void setPointData(const QMap<int, QMap<int, QPointF>>& data);

    bool calibrate(const QString& colmapExePath);

    const QList<CameraResult>& results() const { return cameras; }
    QMap<int, double> reprojectionErrorPerImage() const;
    QMap<int, double> reprojectionErrorPerPoint() const;

private:
    QStringList imagePaths;
    QMap<int, QMap<int, QPointF>> pointData;
    QList<CameraResult> cameras;
};

