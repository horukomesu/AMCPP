#include "Calibrator.h"
#include "DatabaseHelper.h"

#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QtDebug>
#include <QFileInfo>
#include <QTextStream>
#include <QQuaternion>
#include <QMatrix4x4>
#include <cmath>

Calibrator::Calibrator(QObject* parent) : QObject(parent) {}

void Calibrator::setImages(const QStringList& paths)
{
    imagePaths = paths;
}

void Calibrator::setPointData(const QMap<int, QMap<int, QPointF>>& data)
{
    pointData = data;
}

static bool runCommand(const QString& program, const QStringList& args)
{
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(-1)) {
        qWarning() << "Failed to run" << program;
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        qWarning() << program << "failed:" << p.readAllStandardError();
        return false;
    }
    return true;
}

bool Calibrator::calibrate(const QString& colmapExe)
{
    if (imagePaths.size() < 2 || pointData.size() < 3)
        return false;

    QTemporaryDir dir;
    if (!dir.isValid())
        return false;
    QString work = dir.path();
    QDir().mkpath(work + "/images");

    QStringList copied;
    for (const QString& p : imagePaths) {
        QString dst = work + "/images/" + QFileInfo(p).fileName();
        QFile::copy(p, dst);
        copied << dst;
    }

    QString dbPath = work + "/database.db";
    QVector<QMap<int,int>> kpMaps;
    if (!DatabaseHelper::populateDatabase(dbPath, copied, pointData, kpMaps))
        return false;

    QString sparsePath = work + "/sparse";
    QDir().mkpath(sparsePath);

    if (!runCommand(colmapExe, {"mapper",
                                "--database_path", dbPath,
                                "--image_path", work + "/images",
                                "--output_path", sparsePath,
                                "--min_num_matches", "3"}))
        return false;

    if (!runCommand(colmapExe, {"model_converter",
                                "--input_path", sparsePath + "/0",
                                "--output_type", "TXT",
                                "--output_path", sparsePath + "/text"}))
        return false;

    QFile camFile(sparsePath + "/text/cameras.txt");
    QFile imgFile(sparsePath + "/text/images.txt");
    if (!camFile.open(QIODevice::ReadOnly) || !imgFile.open(QIODevice::ReadOnly))
        return false;

    QMap<int,QMatrix3x3> intr;
    while (!camFile.atEnd()) {
        QByteArray line = camFile.readLine();
        if (line.startsWith('#') || line.trimmed().isEmpty())
            continue;
        QTextStream ts(line);
        int id, model, w, h; double f, cx, cy;
        ts >> id >> model >> w >> h >> f >> cx >> cy;
        QMatrix3x3 K; K.fill(0);
        K(0,0)=K(1,1)=f; K(0,2)=cx; K(1,2)=cy; K(2,2)=1.0;
        intr[id]=K;
    }

    cameras.clear();
    while (!imgFile.atEnd()) {
        QByteArray line = imgFile.readLine();
        if (line.startsWith('#') || line.trimmed().isEmpty())
            continue;
        QTextStream ts(line);
        int id; double qw,qx,qy,qz,tx,ty,tz; int camId; QString name;
        ts >> id >> qw>>qx>>qy>>qz>>tx>>ty>>tz>>camId>>name;
        if (!intr.contains(camId))
            continue;
        QQuaternion q(qw,qx,qy,qz);
        QMatrix4x4 pose;
        pose.setToIdentity();
        pose.rotate(q);
        pose.translate(tx,ty,tz);
        CameraResult res; res.K = intr[camId]; res.pose = pose;
        cameras.append(res);
    }

    return !cameras.isEmpty();
}

QMap<int, double> Calibrator::reprojectionErrorPerImage() const
{
    QMap<int,double> result;
    // simplified; no 3D triangulation; compute distance between observations
    for (int idx=0; idx<imagePaths.size(); ++idx) {
        double total=0; int count=0;
        for (auto setId : pointData.keys()) {
            if (pointData[setId].contains(idx))
                count++; // placeholder
        }
        if (count)
            result[idx] = total/count;
    }
    return result;
}

QMap<int, double> Calibrator::reprojectionErrorPerPoint() const
{
    QMap<int,double> result;
    for (int setId : pointData.keys())
        result[setId]=0.0; // placeholder
    return result;
}

