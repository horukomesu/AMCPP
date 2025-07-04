#include "DatabaseHelper.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QProcess>
#include <QDebug>

namespace {
constexpr quint64 kMaxNumImages = 2147483647;

quint64 PairId(int id1, int id2)
{
    if (id1 > id2)
        std::swap(id1, id2);
    return static_cast<quint64>(id1) * kMaxNumImages + id2;
}

QByteArray floatArrayToBlob(const QVector<float> &vals)
{
    QByteArray blob;
    blob.resize(static_cast<int>(vals.size() * sizeof(float)));
    memcpy(blob.data(), vals.constData(), blob.size());
    return blob;
}

QByteArray uintArrayToBlob(const QVector<quint32> &vals)
{
    QByteArray blob;
    blob.resize(static_cast<int>(vals.size() * sizeof(quint32)));
    memcpy(blob.data(), vals.constData(), blob.size());
    return blob;
}

} // namespace

bool DatabaseHelper::populateDatabase(const QString &dbPath,
                                      const QStringList &imagePaths,
                                      const QVector<QSize> &imageSizes,
                                      const QMap<int, QMap<int, QPointF>> &pointData,
                                      QMap<int, QMap<int, int>> &keypointMaps)
{
    if (QFile::exists(dbPath))
        QFile::remove(dbPath);

    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    // create empty DB via colmap CLI so schema matches
    QProcess::execute("colmap", {"database_creator", "--database_path", dbPath});

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "colmap_db");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "Failed to open DB" << dbPath << db.lastError().text();
        return false;
    }

    keypointMaps.clear();
    QList<int> setIds = pointData.keys();
    std::sort(setIds.begin(), setIds.end());

    // insert cameras and images
    QVector<int> camIds(imagePaths.size());
    QVector<int> imgIds(imagePaths.size());

    for (int i = 0; i < imagePaths.size(); ++i) {
        QSize sz = imageSizes[i];
        double f = 1.2 * std::max(sz.width(), sz.height());
        double cx = sz.width() / 2.0;
        double cy = sz.height() / 2.0;
        QVector<float> params{static_cast<float>(f), static_cast<float>(cx), static_cast<float>(cy)};
        QByteArray paramBlob = floatArrayToBlob(params);

        QSqlQuery q(db);
        q.prepare("INSERT INTO cameras(model, width, height, params, prior_focal_length) VALUES(?, ?, ?, ?, 0)");
        q.addBindValue(1); // SIMPLE_PINHOLE
        q.addBindValue(sz.width());
        q.addBindValue(sz.height());
        q.addBindValue(paramBlob);
        if (!q.exec()) {
            qWarning() << "Insert camera failed" << q.lastError().text();
            return false;
        }
        camIds[i] = q.lastInsertId().toInt();

        q.prepare("INSERT INTO images(name, camera_id) VALUES(?, ?)");
        q.addBindValue(QFileInfo(imagePaths[i]).fileName());
        q.addBindValue(camIds[i]);
        if (!q.exec()) {
            qWarning() << "Insert image failed" << q.lastError().text();
            return false;
        }
        imgIds[i] = q.lastInsertId().toInt();
    }

    // prepare keypoints per image
    QMap<int, QVector<QVector<float>>> kps; // [imgIdx][kpIdx][4]
    for (int imgIdx = 0; imgIdx < imagePaths.size(); ++imgIdx) {
        keypointMaps[imgIdx] = {};
        QVector<QVector<float>> arr;
        int cur = 0;
        for (int setId : setIds) {
            if (pointData.contains(setId) && pointData[setId].contains(imgIdx)) {
                QPointF p = pointData[setId][imgIdx];
                arr.append({static_cast<float>(p.x()), static_cast<float>(p.y()), 1.0f, 0.0f});
                keypointMaps[imgIdx][setId] = cur++;
            }
        }
        kps[imgIdx] = arr;
    }

    // write keypoints and descriptors
    for (int i = 0; i < imagePaths.size(); ++i) {
        QVector<float> kpVals;
        for (const QVector<float> &v : kps[i])
            kpVals << v;
        QByteArray kpBlob = floatArrayToBlob(kpVals);

        QSqlQuery q(db);
        q.prepare("INSERT INTO keypoints(image_id, rows, cols, data) VALUES(?, ?, 4, ?)");
        q.addBindValue(imgIds[i]);
        q.addBindValue(kps[i].size());
        q.addBindValue(kpBlob);
        if (!q.exec()) {
            qWarning() << "Insert keypoints failed" << q.lastError().text();
            return false;
        }

        QByteArray descBlob(kps[i].size() * 128, 0);
        q.prepare("INSERT INTO descriptors(image_id, rows, cols, data) VALUES(?, ?, 128, ?)");
        q.addBindValue(imgIds[i]);
        q.addBindValue(kps[i].size());
        q.addBindValue(descBlob);
        if (!q.exec()) {
            qWarning() << "Insert descriptors failed" << q.lastError().text();
            return false;
        }
    }

    // write matches
    for (int i = 0; i < imagePaths.size(); ++i) {
        for (int j = i + 1; j < imagePaths.size(); ++j) {
            QVector<quint32> matches;
            for (int setId : setIds) {
                if (pointData[setId].contains(i) && pointData[setId].contains(j)) {
                    matches << static_cast<quint32>(keypointMaps[i][setId]);
                    matches << static_cast<quint32>(keypointMaps[j][setId]);
                }
            }
            if (matches.isEmpty())
                continue;
            QByteArray blob = uintArrayToBlob(matches);
            QSqlQuery q(db);
            q.prepare("INSERT INTO matches(pair_id, data) VALUES(?, ?)");
            q.addBindValue(static_cast<qulonglong>(PairId(imgIds[i], imgIds[j])));
            q.addBindValue(blob);
            if (!q.exec()) {
                qWarning() << "Insert matches failed" << q.lastError().text();
                return false;
            }
        }
    }

    db.close();
    QSqlDatabase::removeDatabase("colmap_db");

    return true;
}

