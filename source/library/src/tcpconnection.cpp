#include "tcpconnection.h"
#include "tcpmessage_keys.h"

#include <QtNetwork>
#include <iostream>
#include <QDataStream>
#include <QThread>
#if defined(Q_OS_UNIX)
#include <unistd.h>
#include <sys/syscall.h>
#endif
/*
std::vector<TcpConnection*> TcpConnection::globalConnectionList;
uint32_t TcpConnection::globalNrBytesRead;
uint32_t TcpConnection::globalNrBytesWritten;
*/

TcpConnection::TcpConnection(QString newHostName, quint16 newPort, int newVerbose, int newTimeout,
    int newPingInterval, QObject *parent)
    : QObject(parent)
{
    verbose = newVerbose;
    hostName = newHostName;
    port = newPort;
    timeout = newTimeout;
    pingInterval = newPingInterval;
    //qRegisterMetaType<QVector<QVariant> >("QVector<QVariant>");
}

TcpConnection::TcpConnection(int socketDescriptor, int newVerbose, int newTimeout, int newPingInterval, QObject *parent)
    : QObject(parent)
    , m_socketDescriptor(socketDescriptor)//, text(data)
{
    pingInterval = newPingInterval;
    timeout = newTimeout;
    verbose = newVerbose;
    //qRegisterMetaType<QVector<float> >("QVector<float>");
}

TcpConnection::~TcpConnection()
{
    if (in!=nullptr){ delete in; in = nullptr; }
    if (!t.isNull()){
        t.clear();
    }
/*
    auto it = std::find(globalConnectionList.begin(), globalConnectionList.end(), this);
    if (it != globalConnectionList.end()) globalConnectionList.erase(it);
*/
}

void TcpConnection::makeConnection()
// this function gets called with a signal from client-thread
// (TcpConnection runs in a separate thread only communicating with main thread through messages)
{
#if defined(Q_OS_UNIX)
    if (verbose > 4) {
        qInfo() << this->thread()->objectName() << " thread id (pid): " << syscall(SYS_gettid);
    }
#endif
    tcpSocket = new QTcpSocket(this);
    in = new QDataStream();
    in->setVersion(QDataStream::Qt_4_0);
    in->setDevice(tcpSocket);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    //connect(tcpSocket, &QTcpSocket::disconnected, this, &TcpConnection::disconnected);
    tcpSocket->connectToHost(hostName, port);
    firstConnection = time(NULL);
    lastConnection = firstConnection;
    if (!tcpSocket->waitForConnected(timeout)) {
        emit error(tcpSocket->error(), tcpSocket->errorString());
        this->thread()->quit();
        return;
    }
    //globalConnectionList.push_back(this);
    emit connected();
    peerAddress = tcpSocket->peerAddress().toString();
    emit toConsole(QString("makeConnection: peer1 ")+peerAddress);
    if(peerAddress!=""){
        peerAddress = peerAddress.split(':').last();
    }
    emit toConsole(QString("makeConnection: peer2 ")+peerAddress);
    localAddress = tcpSocket->localAddress().toString();
    if(localAddress!=""){
        localAddress = localAddress.split(':').last();
    }
    peerPort = tcpSocket->peerPort();
    localPort = tcpSocket->localPort();
    bytesRead=bytesWritten=0;
    //startTimePulser();
}

void TcpConnection::receiveConnection()
{   // setting up tcpSocket.
    // only done once
#if defined(Q_OS_UNIX)
    if (verbose > 4) {
        qInfo() << this->thread()->objectName() << " thread id (pid): " << syscall(SYS_gettid);
    }
#endif
    tcpSocket = new QTcpSocket(this);
    if (!tcpSocket->setSocketDescriptor(m_socketDescriptor)) {
        emit error(tcpSocket->error(), tcpSocket->errorString());
        this->thread()->quit();
        return;
    }
    peerAddress = tcpSocket->peerAddress().toString();
    //emit toConsole(QString("receiveConnection: peer1 ")+peerAddress);
    if(peerAddress!=""){
        peerAddress = peerAddress.split(':').last();
    }
    //emit toConsole(QString("receiveConnection: peer2 ")+peerAddress);

    localAddress = tcpSocket->localAddress().toString();
    if(localAddress!=""){
        localAddress = localAddress.split(':').last();
    }
    peerPort = tcpSocket->peerPort();
    localPort = tcpSocket->localPort();
    lastConnection = time(NULL);
    //connect(tcpSocket, &QTcpSocket::disconnected, this, &TcpConnection::onSocketDisconnected);
    in = new QDataStream();
    in->setVersion(QDataStream::Qt_4_0);
    in->setDevice(tcpSocket);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    firstConnection = time(NULL);
    lastConnection = firstConnection;
    emit madeConnection(peerAddress, peerPort, localAddress, localPort);
    bytesRead=bytesWritten=0;
    //startTimePulser();
}

void TcpConnection::closeConnection(QString closedAddress) {
    if (peerAddress!=""){
        if (peerAddress==closedAddress){
            closeThisConnection();
        }
    }
}

void TcpConnection::closeThisConnection(){
    TcpMessage quitMessage(TCP_MSG_KEY::MSG_QUIT_CONNECTION);
    *(quitMessage.dStream) << localAddress;
    sendTcpMessage(quitMessage);
    emit finished();
    return;
}

void TcpConnection::onReadyRead() {
    // this function gets called when tcpSocket emits readyRead signal
    if (!in) {
        emit toConsole("input stream not yet initialized");
        return;
    }
    while(tcpSocket->bytesAvailable()!=0){
        if (blockSize == 0) {
            if (tcpSocket->bytesAvailable() < (int)(sizeof(quint16))) {
                return;
            }
            *in >> blockSize;
        }
        if (tcpSocket->bytesAvailable() < blockSize) {
            return;
        }
//        if (tcpSocket->bytesAvailable() > blockSize) {
//            emit toConsole(QString(QString::number(tcpSocket->bytesAvailable() - blockSize)
//                + " more Bytes available than expected by blockSize"));
//        }
        //qDebug() << "blockSize: " << blockSize;
        QByteArray block;
        char* data;
        data = (char *) malloc (blockSize);
        if (data==NULL){
            qDebug() << "critical error: memory allocation of " << blockSize << " bytes failed. Memory full?";
            exit(1);
        }
        in->readRawData(data,blockSize);
        QDataStream str(&block,QIODevice::ReadWrite);
        str << blockSize;
        str.writeRawData(data,blockSize);
        bytesRead+=blockSize;
        //globalNrBytesRead+=blockSize;
        //block.setRawData(data,blockSize); // not sure if it works correctly
        blockSize = 0;
        //*in >> block;
        //qDebug() << block.size()-2 << "Bytes read: "; // -2 because "str << blockSize" makes
                                                      // uint16_t blockSize itself part of block
        if (verbose > 4){
            qDebug() << block;
        }

        TcpMessage tcpMessage(block);
        emit receivedTcpMessage(tcpMessage);
        // emit toConsole("something went wrong with the transmission code");
    }
}

bool TcpConnection::sendTcpMessage(TcpMessage tcpMessage) {
    QByteArray block;
    block = tcpMessage.getData();
    QDataStream stream(&block, QIODevice::ReadWrite);
    stream.device()->seek(0);
    stream << (quint16)(block.size() - (int)sizeof(quint16)); // size of payload
    if (verbose > 4){
        qDebug() << block;
    }
    return writeBlock(block);
}

bool TcpConnection::writeBlock(const QByteArray &block) {
    if (!tcpSocket) {
        emit toConsole("in client => tcpConnection:\ntcpSocket not instantiated");
        return false;
    }
    tcpSocket->write(block);
    bytesWritten+=block.size();
    //globalNrBytesWritten+=block.size();
    for (int i = 0; i < 3; i++) {
        if (tcpSocket->state() != QTcpSocket::UnconnectedState) {
            if (!tcpSocket->waitForBytesWritten(timeout)) {
                //emit toConsole("wait for bytes written timeout");
                quint32 connectionDuration = (quint32)(time(NULL)-firstConnection);
                quint32 timeoutTime = (quint32)time(NULL);
                emit connectionTimeout(peerAddress,peerPort,localAddress,localPort,timeoutTime,connectionDuration);
                this->deleteLater();
                return false;
            }
            return true;
        }
        else {
            //emit toConsole("client unconnected");
            delay(100);
        }
    }
    if (verbose > 1){
        emit toConsole("tcp unconnected state before wait for bytes written, closing connection");
    }
    this->thread()->quit();
    return false;
}

/*
void TcpConnection::onTimePulse(){
    if (fabs(time(NULL)-lastConnection)>timeout/1000){
        // hier einfügen, was passiert, wenn host nicht auf ping antwortet
        quint32 connectionDuration = (quint32)(time(NULL)-firstConnection);
        quint32 timeoutTime = (quint32)time(NULL);
        sendCode(timeoutSig);
        emit connectionTimeout(peerAddress->toString(),peerPort,localAddress->toString(),localPort,timeoutTime,connectionDuration);
        this->deleteLater();
        return;
    }
    if (tcpSocket){
        if (verbose>3){
            emit toConsole("sending ping");
        }
        sendCode(ping);
    }
}

void TcpConnection::startTimePulser()
{
    t = new QTimer(this);
    t->setInterval(pingInterval);
    connect(this, &TcpConnection::stopTimePulser, t, &QTimer::stop);
    connect(t, &QTimer::timeout, this, &TcpConnection::onTimePulse);
    t->start();
}
*/
void TcpConnection::delay(int millisecondsWait)
{
    QEventLoop loop;
    QTimer delay;
    //connect(this, &TcpConnection::posixTerminate, &loop, &QEventLoop::quit);
    //connect(tcpSocket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    delay.connect(&delay, &QTimer::timeout, &loop, &QEventLoop::quit);
    delay.start(millisecondsWait);
    loop.exec();
}
