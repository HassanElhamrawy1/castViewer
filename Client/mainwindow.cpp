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

    qApp->installEventFilter(this);                  /* if we use this here we are telling the Qt any Event occurs in the MainWindow send it to me first
                                                        but we need to monitor everything in the app so we need o use qApp */

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



/* this function will be called everytime we event happened */
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    /* Handle only mouse-related events */
    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease)
    {
        /* Convert the generic event into a QMouseEvent */
        QMouseEvent *mouse = static_cast<QMouseEvent*>(event);

        /* Ensure socket is valid and connected */
        if (!socket || socket->state() != QAbstractSocket::ConnectedState)
            return false;

        /* Prepare a control packet to send mouse input to the client */
        QByteArray ctrlPacket;
        QDataStream out(&ctrlPacket, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::BigEndian);

        /* Packet type = 2 (mouse control packet) */
        out << static_cast<qint32>(2);

        /* Send mouse coordinates */
        out << static_cast<qint32>(mouse->position().x());
        out << static_cast<qint32>(mouse->position().y());

        /* Send mouse button */
        out << static_cast<qint32>(mouse->button());

        /* Send event type (press / release / move) */
        out << static_cast<qint32>(event->type());

        /* Send the packet to the client */
        if (socket && socket->state() == QAbstractSocket::ConnectedState)
        {
            socket->write(ctrlPacket);
        }




        /* Returning true blocks local mouse interaction on the server window */
        return true;
    }

    /* Default handling for other events */
    return QMainWindow::eventFilter(obj, event);
}

