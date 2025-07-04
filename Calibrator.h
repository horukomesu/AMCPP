#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <QStringList>
#include <QVector>
#include <QMap>
#include <QPointF>
#include <QSize>
#include <QVector3D>
#include <Eigen/Dense>

struct CameraParameters
{
    Eigen::Matrix3d intrinsics;
    Eigen::Matrix3d rotation;
    Eigen::Vector3d translation;
};

class Calibrator
{
public:
    Calibrator();

    void loadImages(const QStringList &paths);
    void loadPointData(const QMap<int, QMap<int, QPointF>> &data);

    bool calibrate();

    QList<CameraParameters> getCameraParameters() const;
    QVector<Eigen::Vector3d> getPoints3D() const;
    QMap<QString, float> getReprojectionError() const;
    QMap<int, float> getReprojectionErrorsPerImage() const;

private:
    QStringList m_imagePaths;
    QVector<QSize> m_imageSizes;
    QMap<int, QMap<int, QPointF>> m_pointData;
    QMap<int, QMap<int, int>> m_keypointMaps;

    struct ResultData {
        QMap<int, Eigen::Matrix3d> intrinsics;
        QMap<int, Eigen::Matrix3d> rotations;
        QMap<int, Eigen::Vector3d> translations;
        QVector<Eigen::Vector3d> points3D;
        QVector<int> registeredIndices;
    } m_results;
};

#endif // CALIBRATOR_H
