#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>                      /* to get a screenshot (APIs to capture the screen image) */
#include <QGuiApplication>              /* to get a screenshot (APIs to capture the screen image) */
#include <QBuffer>                      /* to store the image into buffer before sending it  */
#include <QDebug>                       /* to write debig messages on the screen */
#include "FramesSender.h"
#include <QThread>
#include <QDataStream>                  /* to read the packets */
#include <QCursor>
#include <QApplication>
#include <QWindow>
#include <QWidget>
#include <QMouseEvent>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , timer(new QTimer(this))
{
    ui->setupUi(this);

    /* default: connect to localhost; edit UI later to change IP */
    socket->connectToHost("127.0.0.1", 45454);


    /* Create a separate thread for sending frames */
    senderThread = new QThread(this);
    frameSender = new FramesSender(nullptr);
    frameSender->setSocket(socket);

    /* Move worker to thread */
    frameSender->moveToThread(senderThread);

    /* Start thread */
    senderThread->start();

    /* Connect signal to worker */
    connect(this, &MainWindow::frameReady, frameSender, &FramesSender::encodeFrame);

    connect(frameSender, &FramesSender::frameEncoded, this, [this](const QByteArray& packet){ socket->write(packet);});

    /* when we receive readyRead signal on the network the onReadyRead will be called */
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::onReadyRead);

    connect(timer, &QTimer::timeout, this, &MainWindow::sendScreen);  /* call sendScreen on every timeout of the timer */
    timer->start(100); /* 10 frames every 1 second */
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::sendScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();         /* capter the mainscreen */
    if (!screen || socket->state() != QAbstractSocket::ConnectedState) return;   /* return if the socket is not connected to the server  or if there is no screen */

    QPixmap pix = screen->grabWindow(0);                       /* 0 means we capturing the whole screen */
    /* scaling down the image before compressing */
    pix = pix.scaled(pix.width()/2, pix.height()/2, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    emit frameReady(pix);                             /* send the frame worker to the thread */

}

void MainWindow::onReadyRead()
{
    /* Read all available bytes from the socket */
    QByteArray data = socket->readAll();

    /* Process control packets (mouse/keyboard) and screen frames */
    processControlPacket(data);

    // If you already have code processing screen frames,
    // keep that code here too.

    // NOTE:
    // If image frames are sent on the same socket without a packetType prefix,
    // you'll need to merge parsers: read length prefix and handle image frames too.
}

void MainWindow::processControlPacket(QByteArray data)
{
    QDataStream in(&data, QIODevice::ReadOnly);
    in.setByteOrder(QDataStream::BigEndian);

    qint32 packetType;
    in >> packetType;

    if (packetType == 2)  /* Mouse event */
    {
        int x, y, button, action;
        in >> x >> y >> button >> action;

        /* Debugging */
        qDebug() << "[CLIENT] Mouse Event Received:";
        qDebug() << "   Position:" << x << y;
        qDebug() << "   Button:" << button << "Action:" << action;

        /* TO_DO the real mouse movements */
    }
}



