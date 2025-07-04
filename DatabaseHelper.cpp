#include "DatabaseHelper.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QtDebug>
#include <QImage>
#include <QFileInfo>
#include <cmath>

namespace {
// Helper to execute SQL and print errors
static bool execQuery(QSqlQuery& q, const QString& sql) {
    if (!q.exec(sql)) {
        qWarning() << "SQL error:" << q.lastError().text();
        return false;
    }
    return true;
}
}

bool DatabaseHelper::createDatabase(const QString& dbPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "colmap_create");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "Unable to open DB" << dbPath;
        return false;
    }
    QSqlQuery q(db);
    // Schema simplified from COLMAP database schema
    if (!execQuery(q, "CREATE TABLE cameras (\n"
                    "  camera_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
                    "  model INTEGER NOT NULL,\n"
                    "  width INTEGER NOT NULL,\n"
                    "  height INTEGER NOT NULL,\n"
                    "  params BLOB NOT NULL\n)"))
        return false;
    if (!execQuery(q, "CREATE TABLE images (\n"
                    "  image_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
                    "  name TEXT NOT NULL UNIQUE,\n"
                    "  camera_id INTEGER NOT NULL\n)"))
        return false;
    if (!execQuery(q, "CREATE TABLE keypoints (\n"
                    "  image_id INTEGER NOT NULL,\n"
                    "  rows INTEGER NOT NULL,\n"
                    "  cols INTEGER NOT NULL,\n"
                    "  data BLOB NOT NULL\n)"))
        return false;
    if (!execQuery(q, "CREATE TABLE matches (\n"
                    "  pair_id INTEGER PRIMARY KEY NOT NULL,\n"
                    "  rows INTEGER NOT NULL,\n"
                    "  cols INTEGER NOT NULL,\n"
                    "  data BLOB NOT NULL\n)"))
        return false;

    db.close();
    QSqlDatabase::removeDatabase("colmap_create");
    return true;
}

bool DatabaseHelper::populateDatabase(const QString& dbPath,
                                      const QStringList& imagePaths,
                                      const QMap<int, QMap<int, QPointF>>& pointData,
                                      QVector<QMap<int, int>>& keypointMaps)
{
    if (!createDatabase(dbPath))
        return false;

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "colmap_pop");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "open failed" << dbPath;
        return false;
    }

    QSqlQuery q(db);
    const int SIMPLE_PINHOLE = 0;
    QMap<int, int> imageIdMap;
    for (int i = 0; i < imagePaths.size(); ++i) {
        QImage img(imagePaths[i]);
        if (img.isNull()) {
            qWarning() << "Invalid image" << imagePaths[i];
            continue;
        }
        QByteArray params;
        // f, cx, cy using simple guess
        double f = 1.2 * std::max(img.width(), img.height());
        params.append(reinterpret_cast<const char*>(&f), sizeof(double));
        double cx = img.width() / 2.0;
        params.append(reinterpret_cast<const char*>(&cx), sizeof(double));
        double cy = img.height() / 2.0;
        params.append(reinterpret_cast<const char*>(&cy), sizeof(double));

        q.prepare("INSERT INTO cameras(model,width,height,params) VALUES(?,?,?,?)");
        q.addBindValue(SIMPLE_PINHOLE);
        q.addBindValue(img.width());
        q.addBindValue(img.height());
        q.addBindValue(params);
        if (!q.exec()) {
            qWarning() << q.lastError();
            return false;
        }
        int camId = q.lastInsertId().toInt();

        q.prepare("INSERT INTO images(name,camera_id) VALUES(?,?)");
        q.addBindValue(QFileInfo(imagePaths[i]).fileName());
        q.addBindValue(camId);
        if (!q.exec()) {
            qWarning() << q.lastError();
            return false;
        }
        int imgId = q.lastInsertId().toInt();
        imageIdMap[i] = imgId;
    }

    keypointMaps.resize(imagePaths.size());
    for (auto setId : pointData.keys()) {
        for (auto imgIdx : pointData[setId].keys()) {
            QPointF p = pointData[setId][imgIdx];
            QByteArray kpData;
            float arr[4] = { (float)p.x(), (float)p.y(), 1.f, 0.f };
            kpData.append(reinterpret_cast<const char*>(arr), sizeof(arr));

            int imgId = imageIdMap.value(imgIdx);
            q.prepare("INSERT INTO keypoints(image_id,rows,cols,data) VALUES(?,?,?,?)");
            q.addBindValue(imgId);
            q.addBindValue(1);
            q.addBindValue(4);
            q.addBindValue(kpData);
            if (!q.exec()) {
                qWarning() << q.lastError();
                return false;
            }
            int kpIndex = 0; // only one keypoint per set per image
            keypointMaps[imgIdx][setId] = kpIndex;
        }
    }

    // For simplicity we only match identical set ids across images
    for (int i = 0; i < imagePaths.size(); ++i) {
        for (int j = i + 1; j < imagePaths.size(); ++j) {
            QVector<QPair<int,int>> matches;
            for (int setId : pointData.keys()) {
                if (pointData[setId].contains(i) && pointData[setId].contains(j)) {
                    int kp1 = keypointMaps[i][setId];
                    int kp2 = keypointMaps[j][setId];
                    matches.append({kp1,kp2});
                }
            }
            if (matches.isEmpty())
                continue;
            QByteArray matData;
            for (const auto& m : matches) {
                unsigned int pair[2] = { (unsigned int)m.first, (unsigned int)m.second };
                matData.append(reinterpret_cast<const char*>(pair), sizeof(pair));
            }
            // pair_id computed as in COLMAP
            unsigned int id1 = imageIdMap[i];
            unsigned int id2 = imageIdMap[j];
            unsigned int pairId = (id1 < id2) ? id1 * 2147483647u + id2 : id2 * 2147483647u + id1;
            q.prepare("INSERT INTO matches(pair_id,rows,cols,data) VALUES(?,?,?,?)");
            q.addBindValue(pairId);
            q.addBindValue(matches.size());
            q.addBindValue(2);
            q.addBindValue(matData);
            if (!q.exec()) {
                qWarning() << q.lastError();
                return false;
            }
        }
    }

    db.close();
    QSqlDatabase::removeDatabase("colmap_pop");
    return true;
}

