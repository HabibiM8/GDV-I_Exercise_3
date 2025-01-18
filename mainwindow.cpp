// =========================================================================           //
// Authors: Daniel Rutz, Daniel Ströter, Roman Getto, Matthias Bein                    //
//                                                                                     //
// GRIS - Graphisch Interaktive Systeme                                                //
// Technische Universität Darmstadt                                                    //
// Fraunhoferstrasse 5                                                                 //
// D-64283 Darmstadt, Germany                                                          //
//                                                                                     //
// Content: Main Window class, mouse handling, init of UI signal-slot conns, SOLUTION  //
// =========================================================================           //

#include <functional>

#include <QFileDialog>
#include <QMouseEvent>

#include "mainwindow.h"
#include "./ui_mainwindow.h"

void MainWindow::refreshStatusBarMessage() const {
    statusBar()->showMessage(tr("FPS: %1, Triangles: %2").arg(fpsCount).arg(triangleCount));
}

void MainWindow::changeTriangleCount(unsigned int triangles)
{
    triangleCount = triangles;
    refreshStatusBarMessage();
}

void MainWindow::changeFpsCount(unsigned int fps)
{
    fpsCount = fps;
    refreshStatusBarMessage();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->exitButton, &QPushButton::clicked, qApp, &QApplication::exit);
    connect(ui->gridSizeSpinBox, &QSpinBox::valueChanged, ui->openGLWidget, &OpenGLView::setGridSize);
    connect(ui->lightMovementCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::triggerLightMovement);
    connect(ui->resetViewButton, &QPushButton::clicked, ui->openGLWidget, &OpenGLView::setDefaults);
    connect(ui->shaderComboBox, &QComboBox::currentIndexChanged, ui->openGLWidget, &OpenGLView::changeShader);
    connect(ui->coloringModeComboBox, &QComboBox::currentIndexChanged, this, &MainWindow::setColoringMode);
    connect(ui->loadNewShaderButton, &QPushButton::clicked, this, &MainWindow::openShaderLoadingDialog);
    connect(ui->diffuseEnableCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::toggleDiffuse);
    connect(ui->displacementEnableCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::toggleDisplacementMapping);
    connect(ui->bumpEnableCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::toggleNormalMapping);
    connect(ui->drawBBCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::toggleBoundingBox);
    connect(ui->drawNormalCheckBox, &QCheckBox::clicked, ui->openGLWidget, &OpenGLView::toggleNormals);
    connect(ui->genTerrainButton, &QPushButton::clicked, ui->openGLWidget, &OpenGLView::recreateTerrain);

    connect(ui->openGLWidget, &OpenGLView::triangleCountChanged, this, &MainWindow::changeTriangleCount);
    connect(ui->openGLWidget, &OpenGLView::fpsCountChanged, this, &MainWindow::changeFpsCount);
    connect(ui->openGLWidget, &OpenGLView::shaderCompiled, this, &MainWindow::addShaderToList);

    ui->openGLWidget->setGridSize(ui->gridSizeSpinBox->value());

    statusBar()->showMessage(tr("OpenGL-Fenster geöffnet."));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::mousePressEvent(QMouseEvent *ev)
{
    mousePos = ev->pos();
}

void MainWindow::mouseMoveEvent(QMouseEvent *ev)
{
    const auto& newPos = ev->pos();
    //rotate
    ui->openGLWidget->cameraRotates((newPos.x() - mousePos.x()) * mouseSensitivy, (newPos.y() - mousePos.y()) * mouseSensitivy);

    mousePos = ev->pos();
}

void MainWindow::keyPressEvent(QKeyEvent *ev)
{
    switch (ev->key()) {
    case Qt::Key_W:
        ui->openGLWidget->cameraMoves(0.f, 0.f, movementSpeed);
        break;
    case Qt::Key_S:
        ui->openGLWidget->cameraMoves(0.f, 0.f, -movementSpeed);
        break;
    case Qt::Key_A:
        ui->openGLWidget->cameraMoves(-movementSpeed, 0.0, 0.0);
        break;
    case Qt::Key_D:
        ui->openGLWidget->cameraMoves(movementSpeed, 0.0, 0.0);
        break;
    case Qt::Key_Plus:
        movementSpeed *= 2.f;
        break;
    case Qt::Key_Minus:
        movementSpeed /= 2.f;
    default:
        return QMainWindow::keyPressEvent(ev);
    }
}

void MainWindow::openShaderLoadingDialog() {
    const auto vertexShaderFileName = QFileDialog::getOpenFileName(this, QStringLiteral("Vertexshader auswählen"), QString(), QStringLiteral("Vertex Shader File (*.vert)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (vertexShaderFileName.isEmpty()) return;
    
    const auto fragmentShaderFileName = QFileDialog::getOpenFileName(this, QStringLiteral("Fragmentshader auswählen"), QString(), QStringLiteral("Fragment Shader File (*.frag)"), nullptr, QFileDialog::DontUseNativeDialog);
    if (fragmentShaderFileName.isEmpty()) return;

    ui->openGLWidget->compileShader(vertexShaderFileName, fragmentShaderFileName);
}

void MainWindow::addShaderToList(unsigned int index) {
    ui->shaderComboBox->addItem(QStringLiteral("Shader %1").arg(index));
}

void MainWindow::setColoringMode(unsigned int index)
{
    TriangleMesh::ColoringType type;
    switch (index) {
    default:
        qDebug() << "Error: Falscher Index. Setze Standardwert...\n";
        // [[fallthrough]];
    case 0:
        type = TriangleMesh::ColoringType::COLOR_ARRAY;
        break;
    case 1:
        type = TriangleMesh::ColoringType::TEXTURE;
        break;
    case 2:
        type = TriangleMesh::ColoringType::STATIC_COLOR;
        break;
    }

    ui->openGLWidget->changeColoringMode(type);
}

