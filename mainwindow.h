#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "imageviewer.h"
#include "amutilities.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void importImages();
    void addLocator();
    void nextImage();
    void prevImage();
    void onLocatorAdded(float x, float y);

private:
    void showImage(int index, bool keepView = false);
    QString getNextLocatorName() const;

    Ui::MainWindow *ui;
    ImageViewer *viewer;
    QStringList imagePaths;
    QVector<QImage> images;
    QList<LocatorData> locators;
    QString selectedLocator;
    int currentIndex;
};
#endif // MAINWINDOW_H
