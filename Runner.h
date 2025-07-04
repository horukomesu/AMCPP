#pragma once
#include <QObject>
class Calibrator;

class Runner : public QObject
{
    Q_OBJECT
public:
    explicit Runner(QObject* parent = nullptr);
    void runExample(const QString& colmapExe);
private:
    Calibrator* calibrator;
};

