#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "Calibrator.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QMessageBox>
#include <QFileInfo>
#include <qdebug.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      viewer(nullptr),
      currentIndex(-1)
{
    ui->setupUi(this);

    QVBoxLayout *layout = new QVBoxLayout(ui->MainFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    viewer = new ImageViewer(ui->MainFrame);
    layout->addWidget(viewer);

    locatorMode = false;
    sceneFilePath.clear();
    imageErrors.clear();


    connect(ui->MainTree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeSelectionChanged);

    connect(ui->actionLoad, &QAction::triggered, this, &MainWindow::importImages);
    connect(ui->actionNEW, &QAction::triggered, this, &MainWindow::newScene);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::loadSceneTriggered);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveSceneTriggered);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::saveSceneAsTriggered);
    connect(ui->btnAddLoc, &QPushButton::clicked, this, &MainWindow::addLocator);
    connect(ui->btnCalibrate, &QPushButton::clicked, this, &MainWindow::calibrate);
    connect(ui->btnDFWS, &QPushButton::clicked, this, &MainWindow::defineWorldspace);
    connect(ui->btnDFMM, &QPushButton::clicked, this, &MainWindow::defineReferenceDistance);
    connect(ui->btnLocMod, &QPushButton::clicked, this, &MainWindow::addModelingLocator);

    connect(viewer, &ImageViewer::locatorAdded, this, &MainWindow::onLocatorAdded);
    connect(viewer, &ImageViewer::navigate, this, [this](int step){
        if(step > 0) nextImage(); else prevImage();
    });

    new QShortcut(QKeySequence(Qt::Key_Right), this, SLOT(nextImage()));
    new QShortcut(QKeySequence(Qt::Key_Left), this, SLOT(prevImage()));
    QShortcut *delShort = new QShortcut(QKeySequence(Qt::Key_Delete), ui->MainTree);
    connect(delShort, &QShortcut::activated, this, &MainWindow::deleteSelectedLocator);
    QShortcut *escShort = new QShortcut(QKeySequence(Qt::Key_Escape), viewer);
    connect(escShort, &QShortcut::activated, this, &MainWindow::exitLocatorMode);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::importImages()
{
    QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select Images"), QString(), tr("Images (*.png *.jpg *.jpeg *.tif)"));
    paths = verifyPaths(paths);
    imagePaths = paths;
    images = loadImages(paths);
    locators.clear();
    selectedLocator.clear();
    imageErrors.clear();
    if (!images.isEmpty()) {
        showImage(0);
    } else {
        viewer->loadImage(QImage());
    }
    updateTree();
}

QString MainWindow::getNextLocatorName() const
{
    QSet<QString> names;
    for (const LocatorData &l : locators) names.insert(l.name);
    int idx = 1;
    while (names.contains(QStringLiteral("loc%1").arg(idx)))
        ++idx;
    return QStringLiteral("loc%1").arg(idx);
}

void MainWindow::addLocator()
{
    if (images.isEmpty())
        return;
    selectedLocator = getNextLocatorName();
    LocatorData loc;
    loc.name = selectedLocator;
    locators.append(loc);
    viewer->setAddingLocator(true);
    updateTree();
}

void MainWindow::onLocatorAdded(float x, float y)
{
    if (currentIndex < 0 || currentIndex >= images.size())
        return;
    for (LocatorData &l : locators) {
        if (l.name == selectedLocator) {
            l.positions.insert(currentIndex, QPointF(x, y));
            break;
        }
    }
    viewer->setAddingLocator(false);
    QList<ViewerMarker> markers;
    for (const LocatorData &l : locators) {
        if (l.positions.contains(currentIndex)) {
            QPointF p = l.positions.value(currentIndex);
            ViewerMarker m{static_cast<float>(p.x()), static_cast<float>(p.y()), l.name, l.error, l.name == selectedLocator};
            markers.append(m);
        }
    }
    viewer->setMarkers(markers);
    updateTree();
}

void MainWindow::showImage(int index, bool keepView)
{
    if (index < 0 || index >= images.size())
        return;
    currentIndex = index;
    viewer->loadImage(images[index], keepView);

    QList<ViewerMarker> markers;
    for (const LocatorData &l : locators) {
        if (l.positions.contains(index)) {
            QPointF p = l.positions.value(index);
            ViewerMarker m{static_cast<float>(p.x()), static_cast<float>(p.y()), l.name, l.error, l.name == selectedLocator};
            markers.append(m);
        }
    }
    viewer->setMarkers(markers);
}

void MainWindow::nextImage()
{
    if (images.isEmpty()) return;
    int idx = (currentIndex + 1) % images.size();
    showImage(idx, true);
}

void MainWindow::prevImage()
{
    if (images.isEmpty()) return;
    int idx = (currentIndex - 1 + images.size()) % images.size();
    showImage(idx, true);
}

void MainWindow::newScene()
{
    exitLocatorMode();
    imagePaths.clear();
    images.clear();
    locators.clear();
    selectedLocator.clear();
    imageErrors.clear();
    sceneFilePath.clear();
    currentIndex = -1;
    viewer->loadImage(QImage());
    viewer->setMarkers({});
    updateTree();
}

void MainWindow::saveSceneTriggered()
{
    if (sceneFilePath.isEmpty()) {
        saveSceneAsTriggered();
        return;
    }
    if (!saveScene(sceneFilePath, imagePaths, locators)) {
        QMessageBox::critical(this, tr("Save Failed"), tr("Could not save scene."));
    }
}

void MainWindow::saveSceneAsTriggered()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Scene As"), sceneFilePath, tr("AMS Scene (*.ams)"));
    if (path.isEmpty())
        return;
    if (!path.toLower().endsWith(".ams"))
        path += ".ams";
    sceneFilePath = path;
    saveSceneTriggered();
}

void MainWindow::loadSceneTriggered()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Load Scene"), QString(), tr("AMS Scene (*.ams)"));
    if (path.isEmpty())
        return;
    QStringList imgs;
    QList<LocatorData> locs;
    if (!loadSceneAms(path, imgs, locs)) {
        QMessageBox::critical(this, tr("Load Failed"), tr("Could not load scene."));
        return;
    }
    sceneFilePath = path;
    imagePaths = imgs;
    images = loadImages(imgs);
    locators = locs;
    selectedLocator.clear();
    imageErrors.clear();
    if (!images.isEmpty())
        showImage(0);
    updateTree();
}

void MainWindow::calibrate()
{

    if(imagePaths.isEmpty()) {
        QMessageBox::warning(this, tr("Calibrate"), tr("No images loaded."));
        return;
    }

    Calibrator calib;
    calib.loadImages(imagePaths);

    QMap<int, QMap<int, QPointF>> pointData;
    for(int setId=0; setId<locators.size(); ++setId) {
        for(auto it = locators[setId].positions.begin(); it != locators[setId].positions.end(); ++it) {
            pointData[setId][it.key()] =  QPointF(it.value().x() * images[it.key()].width(),it.value().y() * images[it.key()].height());
        }
    }
    calib.loadPointData(pointData);

    if(!calib.calibrate()) {
        QMessageBox::critical(this, tr("Calibrate"), tr("Calibration failed."));
        return;
    }

    QMap<QString,float> locErr = calib.getReprojectionError();
    for(int i=0;i<locators.size();++i)
        locators[i].error = locErr.value(QString::number(i), 0.0f);
    imageErrors = calib.getReprojectionErrorsPerImage();

    QMessageBox::information(this, tr("Calibrate"), tr("Calibration completed."));
    updateTree();
}

void MainWindow::defineWorldspace()
{
    QMessageBox::information(this, tr("Worldspace"), tr("Define worldspace not implemented."));
}

void MainWindow::defineReferenceDistance()
{
    QMessageBox::information(this, tr("Reference Distance"), tr("Define reference distance not implemented."));
}

void MainWindow::addModelingLocator()
{
    QMessageBox::information(this, tr("Modeling Locator"), tr("Add modeling locator not implemented."));
}

void MainWindow::updateTree()
{
    ui->MainTree->clear();
    QTreeWidgetItem *imgRoot = new QTreeWidgetItem(ui->MainTree, QStringList(tr("Images")));
    for (int i = 0; i < imagePaths.size(); ++i) {
        QTreeWidgetItem *it = new QTreeWidgetItem(imgRoot, QStringList(QFileInfo(imagePaths[i]).fileName()));
        float err = 0.0f;
        if(imageErrors.contains(i)) err = imageErrors.value(i);
        QPixmap pix(16,16); pix.fill(errorToColor(err));
        it->setIcon(0,QIcon(pix));
    }
    QTreeWidgetItem *locRoot = new QTreeWidgetItem(ui->MainTree, QStringList(tr("Locators")));
    for (const LocatorData &l : locators) {
        QTreeWidgetItem *it = new QTreeWidgetItem(locRoot, QStringList(l.name));
        QPixmap pix(16,16); pix.fill(errorToColor(l.error));
        it->setIcon(0,QIcon(pix));
    }
    imgRoot->setExpanded(true);
    locRoot->setExpanded(true);
}

void MainWindow::exitLocatorMode()
{
    locatorMode = false;
    viewer->setAddingLocator(false);
}

void MainWindow::onTreeSelectionChanged(QTreeWidgetItem *current, QTreeWidgetItem *)
{
    if (!current)
        return;
    QTreeWidgetItem *parent = current->parent();
    if (!parent)
        return;
    QString pText = parent->text(0);
    if (pText == tr("Images")) {
        exitLocatorMode();
        int idx = parent->indexOfChild(current);
        showImage(idx);
    } else if (pText == tr("Locators")) {
        selectedLocator = current->text(0);
        locatorMode = true;
        viewer->setAddingLocator(true);
    }
}

void MainWindow::deleteSelectedLocator()
{
    QTreeWidgetItem *item = ui->MainTree->currentItem();
    if (!item || !item->parent())
        return;
    if (item->parent()->text(0) != tr("Locators"))
        return;
    QString name = item->text(0);
    for (int i = 0; i < locators.size(); ++i) {
        if (locators[i].name == name) {
            locators.removeAt(i);
            break;
        }
    }
    if (selectedLocator == name)
        selectedLocator.clear();
    updateTree();
    if (currentIndex >= 0)
        showImage(currentIndex, true);
}


