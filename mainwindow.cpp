#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QGuiApplication>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)             /* constructor definition*/
    : QMainWindow(parent)                           /* calling the parent constructor QMainWindow */
    , ui(new Ui::MainWindow)                        /* create object to be used to control the user interface */
{
    ui->setupUi(this);                              /*binding the user elements like: the label or the bushbutton with the Mainwindow*/

    QTimer *timer = new QTimer(this);               /* create a new timer and nind it to the parent the MainWindow */
    connect(timer, &QTimer::timeout, this, &MainWindow::updateScreen);  /* every timeout for the timer call the function updatescreen*/
    timer->start(1000);                             /* update every second */
}

MainWindow::~MainWindow()                          /*destructor will be called after closing the window */
{
    delete ui;
}

void MainWindow::updateScreen()                   /* updatescreen to capture and show the screen */
{
    QScreen *screen = QGuiApplication::primaryScreen();      /* get the main window of the device*/
    if (!screen) return;                                     /* close the dunction if there is no screen to avoid issues (most probably will not happen )*/

    QPixmap pixmap = screen->grabWindow(0);                  /* capture the screen as Qpixmap    0 means the whole screen or we can use the window ID  to choose specific window */

    /* "ui->labelScreen->setPixmap" put the screen content of the user and put it into the QLabel */
    ui->labelScreen->setPixmap(pixmap.scaled(ui->labelScreen->size(),            /* "pixmap.scaled" to cgange the picture size to match the Qlable size */
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));         /* make the pícture smooth and without bluring */
}
