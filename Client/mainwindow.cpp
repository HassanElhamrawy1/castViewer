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
    /* Append any new bytes to the receive buffer */
    recvBuffer.append(socket->readAll());

    /* Try to parse as many full packets as possible */
    int offset = 0;
    const int total = recvBuffer.size();
    const uchar *dataPtr = (const uchar*)recvBuffer.constData();

    while (true)
    {
        /* Need at least 4 bytes for packetType */
        if (total - offset < 4) break;

        qint32 packetType = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
        offset += 4;

        if (packetType == 1)
        {
            /* Image packet: need 4 bytes length + that many bytes */
            if (total - offset < 4) { offset -= 4; break; } // unread packetType
            qint32 imgSize = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset));
            offset += 4;

            if (total - offset < imgSize) { offset -= 8; break; } // not full yet

            QByteArray imgData = recvBuffer.mid(offset, imgSize);
            offset += imgSize;

            // Process the image (display)
            QPixmap pix;
            if (pix.loadFromData(imgData, "JPG"))
            {
                ui->labelInfo->setPixmap(pix.scaled(ui->labelInfo->size(),
                                                      Qt::KeepAspectRatio,
                                                      Qt::SmoothTransformation));
            } else {
                qDebug() << "[CLIENT] Failed to load image data";
            }
        }
        else if (packetType == 2)
        {
            /* Control packet: x,y,button,action -> each 4 bytes (qint32) */
            const int controlPayloadBytes = 4 * 4;
            if (total - offset < controlPayloadBytes) { offset -= 4; break; } // unread packetType

            qint32 x = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset)); offset += 4;
            qint32 y = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset)); offset += 4;
            qint32 button = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset)); offset += 4;
            qint32 action = qFromBigEndian<qint32>(*(const qint32*)(dataPtr + offset)); offset += 4;

            /* Debug: print received control */
            qDebug() << "[CLIENT] Mouse Event Received: pos=" << x << y << "button=" << button << "action=" << action;

            /* Map coordinates if needed (assume same resolution for now) */
            int localX = x;
            int localY = y;

            /* Move cursor */
            QCursor::setPos(localX, localY);

            /* Optionally simulate press/release inside Qt widgets (not system-wide) */
            QWidget *w = QApplication::widgetAt(QPoint(localX, localY));
            if (!w) w = this;

            QPoint posInWidget = w->mapFromGlobal(QPoint(localX, localY));
            Qt::MouseButton mb = Qt::LeftButton;
            if (button == 2) mb = Qt::RightButton;
            else if (button == 3) mb = Qt::MiddleButton;

            if (action == 1) { // press
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, QPoint(localX, localY), mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
            } else if (action == 2) { // release
                QMouseEvent relEv(QEvent::MouseButtonRelease, posInWidget, QPoint(localX, localY), mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &relEv);
            } else if (action == 3) { // click
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, QPoint(localX, localY), mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
                QMouseEvent relEv(QEvent::MouseButtonRelease, posInWidget, QPoint(localX, localY), mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &relEv);
            }
        }
        else {
            qDebug() << "[CLIENT] Unknown packetType:" << packetType;
            /* If unknown, we stop to avoid infinite loop */
            break;
        }
    } /* end while loop */

    // Remove processed bytes from recvBuffer */
    if (offset > 0) {
        recvBuffer.remove(0, offset);
    }
}



void MainWindow::processControlPacket( QByteArray &data)
{
    QDataStream in(&data, QIODevice::ReadOnly);
    in.setByteOrder(QDataStream::BigEndian);

    /* If the buffer might contain multiple packets, you should loop; for simplicity we assume single packet.*/
    /* Read packet type */
    qint32 packetType = 0;
    if (in.status() != QDataStream::Ok) return;
    in >> packetType;

    if (packetType == 2) // Mouse event
    {
        int x = 0, y = 0, button = 0, action = 0;
        in >> x >> y >> button >> action;

        /* DEBUG: print what we received */
        qDebug() << "[CLIENT] Mouse Event Received: pos=" << x << y << " button=" << button << " action=" << action;

        /* Map server coordinates to client screen coordinates if resolutions differ.
           Here we assume server sends screen coordinates relative to server screen size.
           If you know serverScreenSize (wServer,hServer), scale to local:
             int localX = x * (localWidth / (double)wServer);
             int localY = y * (localHeight / (double)hServer);
           For now, assume same resolution: */
        int localX = x;
        int localY = y;

        /* Move cursor */
        QCursor::setPos(localX, localY);

        /* Simulate mouse press/release depending on action:
                action==0 -> move only; action==1 -> press; action==2 -> release; action==3 -> click */
        if (action == 1 || action == 2 || action == 3)
        {
            /* find the widget under cursor */
            QPoint globalPos(localX, localY);
            QWidget *w = QApplication::widgetAt(globalPos);
            if (!w) w = this; /* fallback */

            /* Translate global position to widget-local coordinates */
            QPoint posInWidget = w->mapFromGlobal(globalPos);

            Qt::MouseButton mb = Qt::LeftButton;
            if (button == 2) mb = Qt::RightButton;
            else if (button == 3) mb = Qt::MiddleButton;

            if (action == 1) // press
            {
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
            }
            else if (action == 2) // release
            {
                QMouseEvent releaseEv(QEvent::MouseButtonRelease, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &releaseEv);
            }
            else if (action == 3) // click = press+release
            {
                QMouseEvent pressEv(QEvent::MouseButtonPress, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &pressEv);
                QMouseEvent releaseEv(QEvent::MouseButtonRelease, posInWidget, globalPos, mb, mb, Qt::NoModifier);
                QApplication::sendEvent(w, &releaseEv);
            }
        }
    }
    else
    {
        qDebug() << "[CLIENT] Unknown packetType:" << packetType;
        /* If packetType==1 (frame) or other, you should forward data to your frame parser. */
    }
}


