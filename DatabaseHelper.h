#pragma once

#include <QString>
#include <QStringList>
#include <QPointF>
#include <QMap>
#include <QVector>

class DatabaseHelper
{
public:
    // Create an empty COLMAP SQLite database
    static bool createDatabase(const QString& dbPath);

    // Populate database with cameras, images and manual keypoints
    // pointData: set_id -> image_idx -> QPointF
    // keypointMaps: per image map set_id -> keypoint index
    static bool populateDatabase(const QString& dbPath,
                                 const QStringList& imagePaths,
                                 const QMap<int, QMap<int, QPointF>>& pointData,
                                 QVector<QMap<int, int>>& keypointMaps);
};

