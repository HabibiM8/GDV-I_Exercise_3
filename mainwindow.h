// ========================================================================= //
// Authors: Daniel Rutz, Daniel Ströter, Roman Getto, Matthias Bein          //
//                                                                           //
// GRIS - Graphisch Interaktive Systeme                                      //
// Technische Universität Darmstadt                                          //
// Fraunhoferstrasse 5                                                       //
// D-64283 Darmstadt, Germany                                                //
//                                                                           //
// Content: Main Window class, mouse handling, init of UI signal-slot conns  //
// ========================================================================= //

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPoint>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

private slots:
    void openShaderLoadingDialog();
    void addShaderToList(unsigned int index);
    void setColoringMode(unsigned int index);

public slots:
    void changeTriangleCount(unsigned int triangles);
    void changeFpsCount(unsigned int fps);

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;

private:
    Ui::MainWindow *ui;
    unsigned int fpsCount = 0;
    unsigned int triangleCount = 0;
    void refreshStatusBarMessage() const;

    // mouse information
    QPoint mousePos;
    float mouseSensitivy = 1.0f;
    float movementSpeed = 1.0f;
};
#endif // MAINWINDOW_H
