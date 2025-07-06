#ifndef CAMERA_CALIBRATOR_H
#define CAMERA_CALIBRATOR_H

#include <QMap>
#include <QMatrix3x3>
#include <QPointF>
#include <QStringList>
#include <QVector3D>
#include <QVector>

class CameraCalibrator {
public:
  bool loadImages(const QStringList &imagePaths);
  bool loadPointData(const QMap<int, QMap<int, QPointF>> &pointData);
  bool calibrate();

  QVector<QMatrix3x3> getIntrinsics() const { return m_intrinsics; }
  QVector<QMatrix3x3> getRotations() const { return m_rotations; }
  QVector<QVector3D> getTranslations() const { return m_translations; }
  QMap<int, double> getReprojectionErrorPerImage() const;

private:
  bool populateDatabase(const QString &dbPath);

  QStringList m_imagePaths;
  QVector<QPair<int, int>> m_imageShapes;
  QMap<int, QMap<int, QPointF>> m_pointData;
  QVector<QMatrix3x3> m_intrinsics;
  QVector<QMatrix3x3> m_rotations;
  QVector<QVector3D> m_translations;
  QVector<int> m_registeredIndices;
  QMap<int, QMap<int, int>> m_keypointMaps;
};

#endif // CAMERA_CALIBRATOR_H
