#ifndef DATABASEHELPER_H
#define DATABASEHELPER_H

#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QSize>
#include <QSqlDatabase>

class DatabaseHelper
{
public:
    // Populate COLMAP database with manually provided keypoints and matches
    static bool populateDatabase(const QString &dbPath,
                                 const QStringList &imagePaths,
                                 const QVector<QSize> &imageSizes,
                                 const QMap<int, QMap<int, QPointF>> &pointData,
                                 QMap<int, QMap<int, int>> &keypointMaps);
};

#endif // DATABASEHELPER_H
