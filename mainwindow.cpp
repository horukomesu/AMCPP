#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <QMessageBox>
#include <QFileInfo>
#include <qdebug.h>
#include "tools.h"
//#include <event.h>
#include <QMimeData>
#include <optional>
#include <limits>
#include <cmath>
#include <Eigen/Core>
#include <Eigen/SVD>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      viewer(nullptr),
      currentIndex(-1),
      m_toolController(nullptr),
      m_addLocatorTool(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(true);

    QVBoxLayout *layout = new QVBoxLayout(ui->MainFrame);
    layout->setContentsMargins(0, 0, 0, 0);
    viewer = new ImageViewer(ui->MainFrame);
    layout->addWidget(viewer);
    m_toolController = new ToolController(viewer, this);
    m_addLocatorTool = new AddLocatorTool(viewer, m_toolController);
    connect(m_addLocatorTool, &AddLocatorTool::locatorCreated,
            this, &MainWindow::onLocatorAdded);
    m_toolController->registerTool(ToolType::AddLocator, m_addLocatorTool);
    m_toolController->registerTool(ToolType::DefineMeasurements,
                                   new DefineMeasurementsTool(viewer, m_toolController));
    m_toolController->registerTool(ToolType::DefineWorldspace,
                                   new DefineWorldspaceTool(viewer, m_toolController));
    viewer->setToolController(m_toolController);

    locatorMode = false;
    sceneFilePath.clear();
    imageErrors.clear();


    connect(ui->MainTree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onTreeSelectionChanged);

    connect(ui->actionLoad, &QAction::triggered, this, &MainWindow::importImages);
    connect(ui->actionNEW, &QAction::triggered, this, &MainWindow::newScene);

    connect(ui->actionOpen, &QAction::triggered, this, [this](){
        this->loadSceneTriggered(QFileDialog::getOpenFileName(this, tr("Load Scene"), QString(), tr("AMS Scene (*.ams)")));
    });

    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveSceneTriggered);
    connect(ui->actionSave_As, &QAction::triggered, this, &MainWindow::saveSceneAsTriggered);
    connect(ui->btnAddLoc, &QPushButton::clicked, this, &MainWindow::addLocator);
    connect(ui->btnCalibrate, &QPushButton::clicked, this, &MainWindow::calibrate);
    connect(ui->btnDFWS, &QPushButton::clicked, this, &MainWindow::defineWorldspace);
    connect(ui->btnDFMM, &QPushButton::clicked, this, &MainWindow::defineReferenceDistance);
    connect(ui->btnLocMod, &QPushButton::clicked, this, &MainWindow::addModelingLocator);

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
    m_toolController->setActiveTool(ToolType::AddLocator);
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

void MainWindow::loadSceneTriggered(QString filename)
{
    QString path = filename;
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
    if (imagePaths.isEmpty()) {
        QMessageBox::warning(this, tr("Calibrate"),
                             tr("No images loaded for calibration."));
        return;
    }

    CameraCalibrator calibrator;
    calibrator.loadImages(imagePaths);

    QMap<int, QMap<int, QPointF>> pointData;
    for (int setId = 0; setId < locators.size(); ++setId) {
        QMap<int, QPointF> map;
        for (auto it = locators[setId].positions.begin();
             it != locators[setId].positions.end(); ++it) {
            int idx = it.key();
            if (idx < 0 || idx >= images.size())
                continue;
            QPointF p = it.value();
            QPointF pix(p.x() * images[idx].width(),
                        p.y() * images[idx].height());
            map.insert(idx, pix);
        }
        if (!map.isEmpty())
            pointData.insert(setId, map);
    }

    calibrator.loadPointData(pointData);
    if (!calibrator.calibrate()) {
        QMessageBox::critical(this, tr("Calibrate"),
                              tr("Calibration failed. Check your points."));
        return;
    }

    imageErrors = calibrator.getReprojectionErrorPerImage();

    // Compute per-locator errors
    QMap<int, Eigen::Matrix<double, 3, 4>> Pmats;
    QVector<int> regIdx = calibrator.getRegisteredIndices();
    auto intr = calibrator.getIntrinsics();
    auto rot = calibrator.getRotations();
    auto trans = calibrator.getTranslations();
    for (int idx : regIdx) {
        if (idx >= intr.size() || idx >= rot.size() || idx >= trans.size())
            continue;
        Eigen::Matrix3d K, Rwc;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                K(r, c) = intr[idx](r, c);
                Rwc(r, c) = rot[idx](r, c);
            }
        }
        Eigen::Vector3d twc(trans[idx].x(), trans[idx].y(), trans[idx].z());
        Eigen::Matrix3d Rcw = Rwc.transpose();
        Eigen::Vector3d tcw = -Rcw * twc;
        Eigen::Matrix<double, 3, 4> P;
        P.leftCols<3>() = Rcw;
        P.col(3) = tcw;
        P = K * P;
        Pmats[idx] = P;
    }

    auto triangulatePointFn = [](const QMap<int, QPointF> &obs,
                                 const QMap<int, Eigen::Matrix<double, 3, 4>> &P) {
        std::vector<Eigen::Vector4d> rows;
        for (auto it = obs.begin(); it != obs.end(); ++it) {
            int i = it.key();
            if (!P.contains(i))
                continue;
            const auto &mat = P[i];
            double x = it.value().x();
            double y = it.value().y();
            rows.emplace_back(x * mat.row(2) - mat.row(0));
            rows.emplace_back(y * mat.row(2) - mat.row(1));
        }
        if (rows.size() < 4)
            return std::optional<Eigen::Vector3d>();
        Eigen::MatrixXd A(rows.size(), 4);
        for (int i = 0; i < rows.size(); ++i)
            A.row(i) = rows[i];
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullV);
        Eigen::Vector4d X = svd.matrixV().col(3);
        X /= X(3);
        return std::optional<Eigen::Vector3d>(X.head<3>());
    };

    for (int setId : pointData.keys()) {
        const auto &obs = pointData[setId];
        auto optX = triangulatePointFn(obs, Pmats);
        if (!optX) {
            locators[setId].error = std::numeric_limits<float>::infinity();
            continue;
        }
        Eigen::Vector4d Xh;
        Xh << *optX, 1.0;
        double total = 0.0;
        int count = 0;
        for (auto it = obs.begin(); it != obs.end(); ++it) {
            int i = it.key();
            if (!Pmats.contains(i))
                continue;
            const auto &P = Pmats[i];
            Eigen::Vector3d proj = P * Xh;
            proj /= proj(2);
            QPointF pt = it.value();
            double err = std::hypot(pt.x() - proj(0), pt.y() - proj(1));
            total += err;
            count += 1;
        }
        if (count)
            locators[setId].error = static_cast<float>(total / count);
        else
            locators[setId].error = std::numeric_limits<float>::infinity();
    }

    updateTree();
    if (currentIndex >= 0)
        showImage(currentIndex, true);

    QMessageBox::information(this, tr("Calibration Completed"),
                             tr("Calibration finished successfully."));
}


void MainWindow::dragEnterEvent(QDragEnterEvent * evt)
{
    if(evt->mimeData()->hasUrls())
        evt->accept();
}
void MainWindow::dropEvent(QDropEvent * evt)
{

    const QMimeData *mimeData = evt->mimeData();
    if(mimeData->hasUrls())
    {
        auto urls = mimeData->urls();
        foreach(auto url, urls) {
            QString str = url.toString();

            if(str.startsWith("file:///home/")) // linux moment
                str.replace("file:///home/","/home/");

            if(str.startsWith("file:///"))
                str.replace("file:///","");

            loadSceneTriggered(str);

        }
    }
}


void MainWindow::defineWorldspace()
{
    m_toolController->setActiveTool(ToolType::DefineWorldspace);
}

void MainWindow::defineReferenceDistance()
{
    m_toolController->setActiveTool(ToolType::DefineMeasurements);
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
    m_toolController->setActiveTool(ToolType::None);
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
        m_toolController->setActiveTool(ToolType::AddLocator);
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


