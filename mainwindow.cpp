#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>

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

    connect(ui->actionLoad, &QAction::triggered, this, &MainWindow::importImages);
    connect(ui->btnAddLoc, &QPushButton::clicked, this, &MainWindow::addLocator);

    connect(viewer, &ImageViewer::locatorAdded, this, &MainWindow::onLocatorAdded);
    connect(viewer, &ImageViewer::navigate, this, [this](int step){
        if(step > 0) nextImage(); else prevImage();
    });

    new QShortcut(QKeySequence(Qt::Key_Right), this, SLOT(nextImage()));
    new QShortcut(QKeySequence(Qt::Key_Left), this, SLOT(prevImage()));
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
    if (!images.isEmpty()) {
        showImage(0);
    }
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
            ViewerMarker m{static_cast<float>(p.x()), static_cast<float>(p.y()), l.name, l.error};
            markers.append(m);
        }
    }
    viewer->setMarkers(markers);
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
            ViewerMarker m{static_cast<float>(p.x()), static_cast<float>(p.y()), l.name, l.error};
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


