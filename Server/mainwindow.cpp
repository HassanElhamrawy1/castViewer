#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>                    /* to show the image on the Qlabel */
#include <QDebug>                     /* to write debugging message on the screen */
#include <Qpainter>                   /* if we need to draw picture of fill spaces */

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)                                        /* create new UI object */
    , server(new QTcpServer(this))                                  /* create new tcp server object and bind it to the parent */
    , clientSocket(nullptr)                                         /* initially there is no clients */
{
    ui->setupUi(this);                                              /* bind all the UI with the actual window */

    connect(server, &QTcpServer::newConnection,
            this, &MainWindow::onNewConnection);                    /* call onNewConnection when a client request new connection */

    if (!server->listen(QHostAddress::Any, 45454))       /* make sure that you listen to the port 45454 */
    {
        qDebug() << "Server failed to start!";
    } else
    {
        qDebug() << "Server listening on port 45454";
    }
}

MainWindow::~MainWindow()             /* responsible for cleaning the memory after closing the application */
{
    delete ui;
}                                     /* note we do not need to delete the server or the clienr socket because we select them with this and Qt manage the memory automatically */

void MainWindow::onNewConnection()
{
    clientSocket = server->nextPendingConnection();            /* returning QTcpSocket of the new client "QTcpSocket" represent the client connection */
    connect(clientSocket, &QTcpSocket::readyRead,
            this, &MainWindow::onReadyRead);                   /* call "onReadyRead" directly after receiving data from the client */

    qDebug() << "Client connected!";                           /* write debug message to tell the user that the client is connected */
}

void MainWindow::onReadyRead()
{
    static QByteArray buffer;                               /* the data can be received partially so we need a buffer to assemble the parts in it*/
    buffer.append(clientSocket->readAll());                 /* append the upcoming data on the client socket in it*/

    while (buffer.size() >= 4)                              /* the first 4 bytes represent the photo length so we need to read it first */
    {
        QDataStream stream(buffer);
        stream.setByteOrder(QDataStream::BigEndian);        /* to read the length prefix in a correct way here it is BigEndian*/
        qint32 imgSize;                                     /* variable to dave the image size */
        stream >> imgSize;                                  /* storing the image size in the variable */

        if (buffer.size() < imgSize + 4) break;             /* if the image is not reveived completely  break the loop and wait for the new data */

        QByteArray imgData = buffer.mid(4, imgSize);        /* extract the image data after the first 4 bytes */
        buffer.remove(0, 4 + imgSize);                      /* remove the data from the buffer after processing it */

        QPixmap pix;
        pix.loadFromData(imgData, "JPG");                   /* transform the data into a form can be presented by Qt */

        /* show the image in the Qlabel */
        ui->labelScreen->setPixmap(pix.scaled(
            ui->labelScreen->size(),
            Qt::KeepAspectRatio,                /* to keep the wide and hite of the image */
            Qt::SmoothTransformation            /* make the image smooth when we change the size */
            ));
    }
}
