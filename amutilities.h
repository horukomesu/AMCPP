#ifndef AMUTILITIES_H
#define AMUTILITIES_H

#include <QColor>
#include <QImage>
#include <QStringList>
#include <QMap>
#include <QPointF>
#include <QList>

struct LocatorData {
    QString name;
    QMap<int, QPointF> positions;
    float error = 0.0f;
};

QColor errorToColor(float error, float minErr = 0.0f, float maxErr = 10.0f);
QVector<QImage> loadImages(const QStringList &paths);
QStringList verifyPaths(const QStringList &paths);
bool saveScene(const QString &path, const QStringList &imagePaths, const QList<LocatorData> &locators);
bool loadSceneAms(const QString &path, QStringList &imagePaths, QList<LocatorData> &locators);

#endif // AMUTILITIES_H
