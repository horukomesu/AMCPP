#include "Runner.h"
#include <QProcess>
#include <QDebug>

bool Runner::runMapper(const QString &databasePath,
                       const QString &imagePath,
                       const QString &outputPath)
{
    QStringList args;
    args << "mapper"
         << "--database_path" << databasePath
         << "--image_path" << imagePath
         << "--output_path" << outputPath
         << "--Mapper.init_min_tri_angle" << "1.0"
         << "--Mapper.abs_pose_max_error" << "24";

    QProcess proc;
    proc.start("colmap", args);
    if (!proc.waitForFinished(-1)) {
        qWarning() << "colmap mapper failed" << proc.errorString();
        return false;
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

