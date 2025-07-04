#include "filesystem.h"
#include "miniz.h"
#include "json.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QDir>

using json = nlohmann::json;

static QString md5File(const QString &path)
{
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Md5);
    while(!f.atEnd())
        hash.addData(f.read(4096));
    return hash.result().toHex();
}

static QString md5String(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex();
}

bool saveAms(const QString &filepath, const QByteArray &jsonData, const QStringList &imagePaths)
{
    QMap<QString, QString> oldHashes;
    if(QFileInfo::exists(filepath)) {
        mz_zip_archive zip{}; memset(&zip, 0, sizeof(zip));
        if(mz_zip_reader_init_file(&zip, filepath.toUtf8().constData(), 0)) {
            int idx = mz_zip_reader_locate_file(&zip, "meta/hashmap.json", nullptr, 0);
            if(idx >= 0) {
                size_t sz;
                char* buf = (char*)mz_zip_reader_extract_to_heap(&zip, idx, &sz, 0);
                if(buf) {
                    auto j = json::parse(std::string(buf, sz));
                    for(auto &it : j.items())
                        oldHashes[QString::fromStdString(it.key())] = QString::fromStdString(it.value().get<std::string>());
                    mz_free(buf);
                }
            }
            mz_zip_reader_end(&zip);
        }
    }

    QMap<QString, QString> newHashes;
    newHashes["scene.json"] = md5String(jsonData);
    for(const QString &img : imagePaths)
        newHashes["images/" + QFileInfo(img).fileName()] = md5File(img);

    mz_zip_archive zipw{}; memset(&zipw, 0, sizeof(zipw));
    if(!mz_zip_writer_init_file(&zipw, filepath.toUtf8().constData(), 0))
        return false;

    if(!mz_zip_writer_add_mem(&zipw, "scene.json", jsonData.constData(), jsonData.size(), MZ_BEST_COMPRESSION))
        return false;

    for(const QString &img : imagePaths) {
        QByteArray pathUtf8 = img.toUtf8();
        std::string name = std::string("images/") + QFileInfo(img).fileName().toStdString();
        if(!mz_zip_writer_add_file(&zipw, name.c_str(), pathUtf8.constData(), nullptr, 0, MZ_BEST_COMPRESSION))
            return false;
    }

    json j;
    for(auto it = oldHashes.begin(); it != oldHashes.end(); ++it)
        j[it.key().toStdString()] = it.value().toStdString();
    for(auto it = newHashes.begin(); it != newHashes.end(); ++it)
        j[it.key().toStdString()] = it.value().toStdString();
    std::string jstr = j.dump();
    mz_zip_writer_add_mem(&zipw, "meta/hashmap.json", jstr.data(), jstr.size(), MZ_BEST_COMPRESSION);

    mz_zip_writer_finalize_archive(&zipw);
    mz_zip_writer_end(&zipw);
    return true;
}

bool loadAms(const QString &filepath, QByteArray &jsonData, QList<LoadedImage> &images)
{
    mz_zip_archive zip{}; memset(&zip, 0, sizeof(zip));
    if(!mz_zip_reader_init_file(&zip, filepath.toUtf8().constData(), 0))
        return false;

    int fileCount = (int)mz_zip_reader_get_num_files(&zip);
    jsonData.clear();
    images.clear();
    for(int i=0;i<fileCount;i++) {
        mz_zip_archive_file_stat st;
        mz_zip_reader_file_stat(&zip, i, &st);
        std::string name = st.m_filename;
        if(name == "scene.json") {
            size_t sz; char* buf = (char*)mz_zip_reader_extract_to_heap(&zip, i, &sz, 0);
            if(!buf) { mz_zip_reader_end(&zip); return false; }
            jsonData = QByteArray(buf, (int)sz);
            mz_free(buf);
        } else if(name.rfind("images/",0)==0) {
            size_t sz; unsigned char* buf=(unsigned char*)mz_zip_reader_extract_to_heap(&zip,i,&sz,0);
            if(!buf) { mz_zip_reader_end(&zip); return false; }
            LoadedImage li; li.name=QString::fromStdString(name.substr(7)); li.data=QByteArray((char*)buf,(int)sz);
            images.append(li);
            mz_free(buf);
        }
    }
    mz_zip_reader_end(&zip);
    return !jsonData.isEmpty();
}

