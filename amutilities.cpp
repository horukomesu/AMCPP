#include "amutilities.h"
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "filesystem.h"

QColor errorToColor(float error, float minErr, float maxErr)
{
    float t = std::clamp((error - minErr) / (maxErr - minErr), 0.0f, 1.0f);
    int r = static_cast<int>(255 * t);
    int g = static_cast<int>(255 * (1.0f - t));
    return QColor(r, g, 0);
}

QVector<QImage> loadImages(const QStringList &paths)
{
    QVector<QImage> images;
    for (const QString &p : paths) {
        QImage img(p);
        if (!img.isNull())
            images.append(img);
    }
    return images;
}

QStringList verifyPaths(const QStringList &paths)
{
    QStringList valid;
    for (const QString &p : paths) {
        if (QFileInfo::exists(p))
            valid << p;
    }
    return valid;
}

bool saveScene(const QString &path, const QStringList &imagePaths, const QList<LocatorData> &locators)
{
    QJsonObject root;
    root["format_version"] = 1;
    QJsonArray imgs;
    for (const QString &p : imagePaths)
        imgs.append(p);
    root["images"] = imgs;

    QJsonArray locArr;
    for (const LocatorData &l : locators) {
        QJsonObject obj;
        obj["name"] = l.name;
        QJsonObject posObj;
        for (auto it = l.positions.begin(); it != l.positions.end(); ++it) {
            QJsonObject p;
            p["x"] = it.value().x();
            p["y"] = it.value().y();
            posObj[QString::number(it.key())] = p;
        }
        obj["positions"] = posObj;
        obj["error"] = l.error;
        locArr.append(obj);
    }
    root["locators"] = locArr;

    QJsonDocument doc(root);
    QByteArray jsonData = doc.toJson();
    return saveAms(path, jsonData, imagePaths);
}

bool loadSceneAms(const QString &path, QStringList &imagePaths, QList<LocatorData> &locators)
{
    QByteArray jsonData;
    QList<LoadedImage> loadedImages;
    if(!loadAms(path, jsonData, loadedImages))
        return false;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject())
        return false;
    QJsonObject root = doc.object();
    imagePaths.clear();
    for (const QJsonValue &v : root["images"].toArray())
        imagePaths << v.toString();
    locators.clear();
    for (const QJsonValue &lv : root["locators"].toArray()) {
        QJsonObject o = lv.toObject();
        LocatorData l;
        l.name = o["name"].toString();
        l.error = static_cast<float>(o["error"].toDouble());
        QJsonObject posObj = o["positions"].toObject();
        for (const QString &k : posObj.keys()) {
            QJsonObject p = posObj[k].toObject();
            l.positions.insert(k.toInt(), QPointF(p["x"].toDouble(), p["y"].toDouble()));
        }
        locators.append(l);
    }
    return true;
}


