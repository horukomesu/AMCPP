#include "Calibrator.h"
#include "DatabaseHelper.h"
#include "Runner.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QFile>
#include <QProcess>
#include <QDebug>
#include <QtMath>
#include <Eigen/Dense>
#include <fstream>

Calibrator::Calibrator()
{
}

void Calibrator::loadImages(const QStringList &paths)
{
    m_imagePaths = paths;
    m_imageSizes.clear();
    for (const QString &p : paths) {
        QImage img(p);
        m_imageSizes.append(img.size());
    }
}

void Calibrator::loadPointData(const QMap<int, QMap<int, QPointF>> &data)
{
    m_pointData = data;
}

bool Calibrator::calibrate()
{
    if (m_imagePaths.size() < 2 || m_pointData.size() < 3)
        return false;

    QTemporaryDir tmpDir;
    if (!tmpDir.isValid())
        return false;
    QString workDir = tmpDir.path();

    QString imagesDir = workDir + "/images";
    QDir().mkpath(imagesDir);
    for (const QString &p : m_imagePaths) {
        QFile::copy(p, imagesDir + "/" + QFileInfo(p).fileName());
    }

    QString dbPath = workDir + "/database.db";
    if (!DatabaseHelper::populateDatabase(dbPath, m_imagePaths, m_imageSizes, m_pointData, m_keypointMaps))
        return false;

    QString sparseDir = workDir + "/sparse";
    QDir().mkpath(sparseDir);
    if (!Runner::runMapper(dbPath, imagesDir, sparseDir))
        return false;

    QString modelDir = sparseDir + "/0";
    QString camerasTxt = modelDir + "/cameras.txt";
    QString imagesTxt = modelDir + "/images.txt";
    QString pointsTxt = modelDir + "/points3D.txt";
    if (!QFileInfo::exists(camerasTxt) || !QFileInfo::exists(imagesTxt))
        return false;

    m_results = ResultData();

    std::ifstream camFile(camerasTxt.toStdString());
    std::string line;
    while (std::getline(camFile, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::stringstream ss(line);
        int camId; std::string model; int w,h; double f,cx,cy;
        ss >> camId >> model >> w >> h >> f >> cx >> cy;
        Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
        K(0,0)=K(1,1)=f; K(0,2)=cx; K(1,2)=cy;
        m_results.intrinsics[camId] = K;
    }

    std::ifstream imgFile(imagesTxt.toStdString());
    while (std::getline(imgFile, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::stringstream ss(line);
        int imgId; double qw,qx,qy,qz,tx,ty,tz; int camId; std::string name;
        ss >> imgId >> qw>>qx>>qy>>qz>>tx>>ty>>tz>>camId >> name;
        Eigen::Quaterniond q(qw,qx,qy,qz);
        Eigen::Matrix3d R_w2c = q.toRotationMatrix();
        Eigen::Vector3d t_w2c(tx,ty,tz);
        Eigen::Matrix3d R_c2w = R_w2c.transpose();
        Eigen::Vector3d t_c2w = -R_c2w * t_w2c;
        int index = m_imagePaths.indexOf(QString::fromStdString(name));
        if (index >= 0) {
            m_results.rotations[index] = R_c2w;
            m_results.translations[index] = t_c2w;
            m_results.intrinsics[index] = m_results.intrinsics.value(camId);
            m_results.registeredIndices.append(index);
        }
    }

    std::ifstream ptsFile(pointsTxt.toStdString());
    while (std::getline(ptsFile, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::stringstream ss(line);
        long id; double x,y,z; double r,g,b; double err; int n;
        ss >> id >> x>>y>>z >> r>>g>>b >> err >> n;
        m_results.points3D.append(Eigen::Vector3d(x,y,z));
    }

    return !m_results.registeredIndices.isEmpty();
}

QList<CameraParameters> Calibrator::getCameraParameters() const
{
    QList<CameraParameters> res;
    for (int idx : m_results.registeredIndices) {
        if (m_results.intrinsics.contains(idx) && m_results.rotations.contains(idx)) {
            CameraParameters p;
            p.intrinsics = m_results.intrinsics[idx];
            p.rotation = m_results.rotations[idx];
            p.translation = m_results.translations[idx];
            res.append(p);
        }
    }
    return res;
}

QVector<Eigen::Vector3d> Calibrator::getPoints3D() const
{
    return m_results.points3D;
}

static Eigen::Vector3d triangulate(const QMap<int, QPointF> &obs,
                                   const QMap<int, Eigen::Matrix<double,3,4>> &proj)
{
    Eigen::MatrixXd A;
    A.resize(obs.size()*2,4);
    int row = 0;
    for(auto it=obs.begin(); it!=obs.end(); ++it) {
        if(!proj.contains(it.key())) continue;
        const auto &P = proj[it.key()];
        double x = it.value().x();
        double y = it.value().y();
        A.row(row++) = x*P.row(2)-P.row(0);
        A.row(row++) = y*P.row(2)-P.row(1);
    }
    if(row < 4) return Eigen::Vector3d::Zero();
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(A.topRows(row), Eigen::ComputeFullV);
    Eigen::Vector4d X = svd.matrixV().col(3);
    X /= X[3];
    return X.head<3>();
}

QMap<QString, float> Calibrator::getReprojectionError() const
{
    if (m_results.registeredIndices.isEmpty())
        return {};

    QMap<int, Eigen::Matrix<double,3,4>> proj;
    for(int idx: m_results.registeredIndices) {
        Eigen::Matrix3d K = m_results.intrinsics[idx];
        Eigen::Matrix3d R = m_results.rotations[idx].transpose();
        Eigen::Vector3d t = -R * m_results.translations[idx];
        Eigen::Matrix<double,3,4> P;
        P.block<3,3>(0,0)=R; P.col(3)=t;
        P = K * P;
        proj[idx]=P;
    }

    QMap<QString,float> errors;
    for(auto it = m_pointData.begin(); it!=m_pointData.end(); ++it) {
        Eigen::Vector3d X = triangulate(it.value(), proj);
        double total=0; int count=0;
        for(auto obs = it.value().begin(); obs!=it.value().end(); ++obs) {
            if(!proj.contains(obs.key())) continue;
            Eigen::Vector4d Xh; Xh << X,1.0;
            Eigen::Vector3d p = proj[obs.key()] * Xh;
            p /= p[2];
            double err = (Eigen::Vector2d(obs.value().x(), obs.value().y()) - p.head<2>()).norm();
            total += err; ++count;
        }
        if(count)
            errors[QString::number(it.key())] = static_cast<float>(total/count);
    }
    return errors;
}

QMap<int, float> Calibrator::getReprojectionErrorsPerImage() const
{
    if (m_results.registeredIndices.isEmpty())
        return {};

    QMap<int, Eigen::Matrix<double,3,4>> proj;
    for(int idx: m_results.registeredIndices) {
        Eigen::Matrix3d K = m_results.intrinsics[idx];
        Eigen::Matrix3d R = m_results.rotations[idx].transpose();
        Eigen::Vector3d t = -R * m_results.translations[idx];
        Eigen::Matrix<double,3,4> P;
        P.block<3,3>(0,0)=R; P.col(3)=t;
        P = K * P;
        proj[idx]=P;
    }

    QMap<int,double> totals;
    QMap<int,int> counts;

    for(const auto &obsSet : m_pointData) {
        Eigen::Vector3d X = triangulate(obsSet, proj);
        for(auto obs = obsSet.begin(); obs!=obsSet.end(); ++obs) {
            if(!proj.contains(obs.key())) continue;
            Eigen::Vector4d Xh; Xh << X,1.0;
            Eigen::Vector3d p = proj[obs.key()] * Xh;
            p /= p[2];
            double err = (Eigen::Vector2d(obs.value().x(), obs.value().y()) - p.head<2>()).norm();
            totals[obs.key()] += err;
            counts[obs.key()] += 1;
        }
    }

    QMap<int,float> result;
    for(int idx: proj.keys()) {
        if(counts.value(idx,0))
            result[idx] = static_cast<float>(totals[idx] / counts[idx]);
        else
            result[idx] = std::numeric_limits<float>::infinity();
    }
    return result;
}

