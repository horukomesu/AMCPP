#include "camera_calibrator.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QtMath>

#include <Eigen/Core>
#include <Eigen/SVD>

#include <colmap/controllers/incremental_pipeline.h>
#include <colmap/scene/camera.h>
#include <colmap/scene/database.h>
#include <colmap/scene/image.h>
#include <colmap/scene/reconstruction_manager.h>

using namespace colmap;

bool CameraCalibrator::loadImages(const QStringList &paths) {
  m_imagePaths = paths;
  m_imageShapes.clear();
  for (const QString &p : paths) {
    QImage img(p);
    m_imageShapes.append(qMakePair(img.width(), img.height()));
  }
  return !m_imagePaths.isEmpty();
}

bool CameraCalibrator::loadPointData(
    const QMap<int, QMap<int, QPointF>> &data) {
  m_pointData = data;
  return !m_pointData.isEmpty();
}

bool CameraCalibrator::populateDatabase(const QString &dbPath) {
  try {
    if (QFileInfo::exists(dbPath))
      QFile::remove(dbPath);
    Database db(dbPath.toStdString());
    DatabaseTransaction transaction(&db);

    const CameraModelId modelId = CameraModelId::kSimplePinhole;
    QMap<int, camera_t> camIds;
    QMap<int, image_t> imgIds;

    QList<int> setIds = m_pointData.keys();
    std::sort(setIds.begin(), setIds.end());

    m_keypointMaps.clear();

    for (int i = 0; i < m_imagePaths.size(); ++i) {
      const auto &shape = m_imageShapes[i];
      size_t width = static_cast<size_t>(shape.first);
      size_t height = static_cast<size_t>(shape.second);
      double f = 1.2 * qMax(width, height);
      double cx = width / 2.0;
      double cy = height / 2.0;
      Camera cam;
      cam.camera_id = kInvalidCameraId;
      cam.model_id = modelId;
      cam.width = width;
      cam.height = height;
      cam.params = {f, cx, cy};
      camera_t camId = db.WriteCamera(cam);
      camIds[i] = camId;

      Image img;
      img.SetName(QFileInfo(m_imagePaths[i]).fileName().toStdString());
      img.SetCameraId(camId);
      image_t imgId = db.WriteImage(img);
      imgIds[i] = imgId;

      FeatureKeypoints keypoints;
      QMap<int, int> kpMap;
      int kpIdx = 0;
      for (int setId : setIds) {
        if (m_pointData[setId].contains(i)) {
          QPointF p = m_pointData[setId][i];
          keypoints.emplace_back(p.x(), p.y(), 1.0f, 0.0f);
          kpMap[setId] = kpIdx++;
        }
      }
      if (!keypoints.empty()) {
        db.WriteKeypoints(imgId, keypoints);
        FeatureDescriptors desc(keypoints.size(), 128);
        desc.setZero();
        db.WriteDescriptors(imgId, desc);
      }
      m_keypointMaps[i] = kpMap;
    }

    for (int i = 0; i < m_imagePaths.size(); ++i) {
      for (int j = i + 1; j < m_imagePaths.size(); ++j) {
            QSet<int> common = QSet<int>(m_keypointMaps[i].keys().begin(), m_keypointMaps[i].keys().end()) &
                               QSet<int>(m_keypointMaps[j].keys().begin(), m_keypointMaps[j].keys().end());
        if (common.isEmpty())
          continue;
        FeatureMatches matches;
        for (int setId : common) {
          matches.emplace_back(m_keypointMaps[i][setId],
                               m_keypointMaps[j][setId]);
        }
        db.WriteMatches(imgIds[i], imgIds[j], matches);
        TwoViewGeometry geom;
        geom.config = TwoViewGeometry::CALIBRATED;
        geom.inlier_matches = matches;
        db.WriteTwoViewGeometry(imgIds[i], imgIds[j], geom);
      }
    }
  } catch (...) {
    return false;
  }
  return true;
}

bool CameraCalibrator::calibrate() {
  if (m_imagePaths.size() < 2 || m_pointData.size() < 3)
    return false;

  QString workDir = QDir::temp().filePath("colmap_cpp_work");
  QDir().rmdir(workDir); // remove old
  QDir().mkpath(workDir);
  QString dbPath = QDir(workDir).filePath("database.db");
  QString imgDir = QDir(workDir).filePath("images");
  QDir().mkpath(imgDir);
  for (const QString &p : m_imagePaths)
    QFile::copy(p, QDir(imgDir).filePath(QFileInfo(p).fileName()));

  if (!populateDatabase(dbPath))
    return false;

  QString sparseDir = QDir(workDir).filePath("sparse");
  QDir().mkpath(sparseDir);

  IncrementalPipelineOptions options;
  options.min_num_matches = 3;
  options.mapper.init_min_num_inliers = 3;
  options.mapper.init_min_tri_angle = 1.0;
  options.mapper.abs_pose_min_num_inliers = 3;
  options.mapper.abs_pose_max_error = 24.0;
  options.mapper.filter_min_tri_angle = 0.0;

  auto optPtr = std::make_shared<IncrementalPipelineOptions>(options);
  auto recManager = std::make_shared<ReconstructionManager>();
  IncrementalPipeline pipeline(optPtr, imgDir.toStdString(),
                               dbPath.toStdString(), recManager);
  pipeline.Run();

  if (recManager->Size() == 0)
    return false;
  std::shared_ptr<Reconstruction> best = recManager->Get(0);
  for (size_t i = 1; i < recManager->Size(); ++i) {
    if (recManager->Get(i)->NumRegImages() > best->NumRegImages())
      best = recManager->Get(i);
  }
  if (best->NumRegImages() < 2)
    return false;

  m_intrinsics.clear();
  m_rotations.clear();
  m_translations.clear();
  m_registeredIndices.clear();

  Database db(dbPath.toStdString());
  std::unordered_map<std::string, image_t> nameToId;
  for (const auto &img : db.ReadAllImages()) {
    nameToId[img.Name()] = img.ImageId();
  }

  for (const auto &pair : best->Images()) {
    image_t imgId = pair.first;
    const Image &img = pair.second;
    QString name = QString::fromStdString(img.Name());
    int idx = m_imagePaths.indexOf(QDir(m_imagePaths[0]).filePath(name));
    if (idx < 0)
      idx = m_imagePaths.indexOf(name);
    if (idx < 0)
      continue;
    const Camera &cam = best->Camera(img.CameraId());
    if (cam.model_id == CameraModelId::kSimplePinhole &&
        cam.params.size() == 3) {
      QMatrix3x3 K;
      K(0, 0) = cam.params[0];
      K(0, 1) = 0;
      K(0, 2) = cam.params[1];
      K(1, 0) = 0;
      K(1, 1) = cam.params[0];
      K(1, 2) = cam.params[2];
      K(2, 0) = 0;
      K(2, 1) = 0;
      K(2, 2) = 1;
      if (m_intrinsics.size() <= idx)
        m_intrinsics.resize(idx + 1);
      m_intrinsics[idx] = K;
    }
    Rigid3d pose = img.CamFromWorld();
    Eigen::Matrix3d Rcw = pose.rotation.toRotationMatrix();
    Eigen::Vector3d tcw = pose.translation;
    Eigen::Matrix3d Rwc = Rcw.transpose();
    Eigen::Vector3d twc = -Rwc * tcw;
    QMatrix3x3 Rmat;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        Rmat(r, c) = Rwc(r, c);
    QVector3D tvec(twc(0), twc(1), twc(2));
    if (m_rotations.size() <= idx)
      m_rotations.resize(idx + 1);
    if (m_translations.size() <= idx)
      m_translations.resize(idx + 1);
    m_rotations[idx] = Rmat;
    m_translations[idx] = tvec;
    m_registeredIndices.append(idx);
  }

  return true;
}

static Eigen::Vector3d
triangulatePoint(const QMap<int, QPointF> &obs,
                 const QMap<int, Eigen::Matrix<double, 3, 4>> &P) {
  std::vector<Eigen::Vector4d> rows;
  for (auto it = obs.begin(); it != obs.end(); ++it) {
    int idx = it.key();
    if (!P.contains(idx))
      continue;
    const auto &mat = P[idx];
    double x = it.value().x();
    double y = it.value().y();
    rows.emplace_back(x * mat.row(2) - mat.row(0));
    rows.emplace_back(y * mat.row(2) - mat.row(1));
  }
  if (rows.size() < 4)
    return Eigen::Vector3d::Zero();
  Eigen::MatrixXd A(rows.size(), 4);
  for (int i = 0; i < rows.size(); ++i)
    A.row(i) = rows[i];
  Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullV);
  Eigen::Vector4d X = svd.matrixV().col(3);
  X /= X(3);
  return X.head<3>();
}

QMap<int, double> CameraCalibrator::getReprojectionErrorPerImage() const {
  QMap<int, double> errors;
  if (m_registeredIndices.isEmpty())
    return errors;

  QMap<int, Eigen::Matrix<double, 3, 4>> Pmats;
  for (int idx : m_registeredIndices) {
    if (idx >= m_intrinsics.size())
      continue;
    const QMatrix3x3 &Kq = m_intrinsics[idx];
    const QMatrix3x3 &Rq = m_rotations[idx];
    QVector3D tq = m_translations[idx];
    Eigen::Matrix3d K, Rwc;
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        K(r, c) = Kq(r, c);
        Rwc(r, c) = Rq(r, c);
      }
    }
    Eigen::Vector3d twc(tq.x(), tq.y(), tq.z());
    Eigen::Matrix3d Rcw = Rwc.transpose();
    Eigen::Vector3d tcw = -Rcw * twc;
    Eigen::Matrix<double, 3, 4> P;
    P.leftCols<3>() = Rcw;
    P.col(3) = tcw;
    P = K * P;
    Pmats[idx] = P;
  }

  QMap<int, double> totals;
  QMap<int, int> counts;
  for (const auto &obs : m_pointData) {
    Eigen::Vector3d X = triangulatePoint(obs, Pmats);
    Eigen::Vector4d Xh;
    Xh << X, 1.0;
    for (auto it = obs.begin(); it != obs.end(); ++it) {
      int idx = it.key();
      if (!Pmats.contains(idx))
        continue;
      const auto &P = Pmats[idx];
      Eigen::Vector3d proj = P * Xh;
      proj /= proj(2);
      QPointF pt = it.value();
      double err = std::hypot(pt.x() - proj(0), pt.y() - proj(1));
      totals[idx] += err;
      counts[idx] += 1;
    }
  }
  for (int idx : Pmats.keys()) {
    if (counts[idx])
      errors[idx] = totals[idx] / counts[idx];
    else
      errors[idx] = std::numeric_limits<double>::infinity();
  }
  return errors;
}
