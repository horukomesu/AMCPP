#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "imageviewer.h"
#include "amutilities.h"
#include <QTreeWidgetItem>

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
    void newScene();
    void saveSceneTriggered();
    void saveSceneAsTriggered();
    void loadSceneTriggered();
    void calibrate();
    void defineWorldspace();
    void defineReferenceDistance();
    void addModelingLocator();
    void nextImage();
    void prevImage();
    void onLocatorAdded(float x, float y);
    void onTreeSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void deleteSelectedLocator();
    void exitLocatorMode();

private:
    void showImage(int index, bool keepView = false);
    QString getNextLocatorName() const;
    void updateTree();

    Ui::MainWindow *ui;
    ImageViewer *viewer;
    QStringList imagePaths;
    QVector<QImage> images;
    QList<LocatorData> locators;
    QMap<int, float> imageErrors;
    QString selectedLocator;
    QString sceneFilePath;
    bool locatorMode;
    int currentIndex;
};
#endif // MAINWINDOW_H
