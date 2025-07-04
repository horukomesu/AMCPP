#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QMap>
#include <QList>

struct LoadedImage {
    QString name;
    QByteArray data;
};

bool saveAms(const QString &filepath, const QByteArray &jsonData, const QStringList &imagePaths);
bool loadAms(const QString &filepath, QByteArray &jsonData, QList<LoadedImage> &images);

#endif // FILESYSTEM_H
