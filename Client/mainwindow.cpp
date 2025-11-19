#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>                      /* to get a screenshot (APIs to capture the screen image) */
#include <QGuiApplication>              /* to get a screenshot (APIs to capture the screen image) */
#include <QBuffer>                      /* to store the image into buffer before sending it  */
#include <QDebug>                       /* to write debig messages on the screen */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , socket(new QTcpSocket(this))
    , timer(new QTimer(this))
{
    ui->setupUi(this);

    // default: connect to localhost; edit UI later to change IP
    socket->connectToHost("127.0.0.1", 45454);

    connect(timer, &QTimer::timeout, this, &MainWindow::sendScreen);  /* call sendScreen on every timeout of the timer */
    timer->start(1000); // every 1 second
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
    QByteArray bytes;
    QBuffer buffer(&bytes);                                    /*  used as temp memory to save the QBuffer into QByteArray*/
    buffer.open(QIODevice::WriteOnly);
    pix.save(&buffer, "JPG", 50);                              /* saving the image in bytes with quality 50% */

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);            /* make the packet ready to be sent */
    out.setByteOrder(QDataStream::BigEndian);                  /* set the byte order to be compatible with the server */


    out << static_cast<qint32>(bytes.size());                  /* send the image length first to garantee the image will be sent compeletly to the server */
    packet.append(bytes);                                      /* appent the message data after the length */
    if(socket->state() == QAbstractSocket::ConnectedState)
    {
        socket->write(packet);                                     /* send the message on the on the TCP if the socket is connected */
    }

}
