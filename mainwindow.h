#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "imageviewer.h"
#include "tools.h"
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

    void dragEnterEvent(QDragEnterEvent * evt) override;
    void dropEvent(QDropEvent * evt) override;

private slots:
    void importImages();
    void addLocator();
    void newScene();
    void saveSceneTriggered();
    void saveSceneAsTriggered();
    void loadSceneTriggered(QString filename);
//void calibrate();
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
    ToolController *m_toolController;
    AddLocatorTool *m_addLocatorTool;
};
#endif // MAINWINDOW_H
