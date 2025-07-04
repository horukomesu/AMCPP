#ifndef RUNNER_H
#define RUNNER_H

#include <QString>

class Runner
{
public:
    static bool runMapper(const QString &databasePath,
                          const QString &imagePath,
                          const QString &outputPath);
};

#endif // RUNNER_H
