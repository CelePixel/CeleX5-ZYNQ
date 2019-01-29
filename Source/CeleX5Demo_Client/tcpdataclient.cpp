#include "tcpdataclient.h"
#include <QFile>
#include <QMetaType>
#include <QDateTime>
#include <iostream>

using namespace std;

#define MINSIZE 12

TCPDataClient::TCPDataClient(QObject *parent)
    : QObject(parent)
    , m_bConnected(false)
    , m_checkSize(0)
{
    m_pTcpSocket = new QTcpSocket(this);
    connect(m_pTcpSocket, SIGNAL(connected()), this, SLOT(onConnected())); //connect
    connect(m_pTcpSocket, SIGNAL(disconnected()), this, SLOT(onDisconnected())); //disconnect
    connect(m_pTcpSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(m_pTcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketError()));
    //
    m_pTcpServer = new QTcpServer(this);
    m_pTcpServer->listen(QHostAddress::Any, 50600); //监听的端口号
    connect(m_pTcpServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
}

TCPDataClient::~TCPDataClient()
{
    if (m_pTcpSocket)
    {
        m_pTcpSocket->close();
        m_pTcpSocket->disconnect(this);
        delete m_pTcpSocket;
    }
}

void TCPDataClient::onConnected()
{
    cout << __FUNCTION__ << ": connect to server successfully!" << endl;
    m_bConnected = true;
}

void TCPDataClient::onDisconnected()
{
    cout << __FUNCTION__ << endl;
    m_pTcpSocket->close();
    if (!m_pTcpSocket->isOpen())
        m_bConnected = false;
}

#include <Windows.h>
void TCPDataClient::onReadyRead()
{
    //cout << __FUNCTION__ << endl;
    if (m_pTcpSocket->bytesAvailable() <= 0)
    {
        return;
    }
    QByteArray buffer = m_pTcpSocket->readAll();


    m_buffer.append(buffer);

    MSG_PACKHEAD head;
    int received_len = 0;
    while (1)
    {
        QDataStream packet(m_buffer);
        packet.setVersion(QDataStream::Qt_5_6);
        packet.setByteOrder(QDataStream::LittleEndian);

//        if (total_len < MINSIZE) //不够包头的数据直接就不处理。
//        {
//            break;
//        }
        packet >> head.cmd >> head.fin >> head.len;
        //cout << "size = " << m_buffer.size() << ", cmd = " << head.cmd << ", len = " << head.len << endl;
        received_len = m_buffer.size();
        if (received_len < 1024012)
        {
            break;
        }

        QByteArray tmpdata;
        tmpdata = m_buffer.mid(12, head.len);
        if (head.cmd == 10)
        {
            m_recvData.append(tmpdata);
            if (m_recvData.size() == 1024000)
            {
                emit pixmapReady(m_recvData);
                m_recvData.clear();
                m_buffer.clear();
                cout << "--------------- successfully! -------------" << endl;
                Sleep(100);
            }
        }
        else
        {
            emit sglRcvPack(&head, tmpdata);
        }
        //缓存多余的数据
        //buffer = m_buffer.right(total_len - msglen);
        //更新长度
        //total_len = buffer.size();
        //更新多余数据
        //m_buffer = buffer;
    }
}

void TCPDataClient::onSocketError()
{
    QString mess = m_pTcpSocket->errorString();
    cout << __FUNCDNAME__ << ": " << mess.toUtf8().constData() << endl;
    //qDebug("error: %s", mess.toUtf8().constData());
    connectToZYNQServer();
}

void TCPDataClient::connectToZYNQServer()
{
    string hostName = "192.168.1.11"; //server ip
    uint16_t port = 1234;             //server port
    connectToServer(hostName, port);
}

bool TCPDataClient::sendCmd(int cmd, QString strData)
{
    Request_MSG_PACK SendPack;
    SendPack.cmd = cmd;
    SendPack.fin = 1;
    SendPack.len = strData.length();
    memcpy(SendPack.data, strData.toStdString().c_str(), strData.length());
    return sendData(&SendPack);
}

bool TCPDataClient::sendCmd(SEND_MSG_PACK msg)
{
    int result = m_pTcpSocket->write((char*)&msg, sizeof(SEND_MSG_PACK));
    cout << __FUNCTION__ << ": write length: " << result << endl;
    return true;
}

bool TCPDataClient::sendData(pRequest_MSG_PACK pMsg)
{
    if (!m_pTcpSocket->isOpen())
    {
        return false;
    }
    int lenth = pMsg->len + 12;
    int result = m_pTcpSocket->write((char*)pMsg, lenth);
    cout <<"write length: " << result << endl;
    return true;
}

void TCPDataClient::connectToServer(const string &hostName, uint16_t port)
{
    printf("connectToServer: host = %s, port = %d\n", hostName.c_str(), port);
    if (m_pTcpSocket)
        m_pTcpSocket->close();
    m_pTcpSocket->connectToHost(QString::fromStdString(hostName), port);
}

void TCPDataClient::readMessage()
{
    QByteArray buffer;
    //读取缓冲区数据
    buffer = m_pConnectedSocket->readAll();
    if (buffer.isEmpty())
    {
//		qDebug() << "buffer Empty";
        return;
    }
    QDataStream packet(buffer);
    packet.setVersion(QDataStream::Qt_5_6);
    packet.setByteOrder(QDataStream::LittleEndian);
    int cmd = 0;
    int fin = 0;
    int len = 0;
    packet >> cmd >> fin >> len;
//    qDebug() << "cmd" <<cmd << "fin "<< fin <<"len "<<len;
    if(cmd == 201)
    {
        //QDataStream out(m_connectedSocket);
        QDateTime DateTime;
        QString StrdateTime = DateTime.currentDateTime().toString("yyyy.MM.dd-hh:mm:ss");
//		qDebug() << "StrdateTime" << StrdateTime;
        Request_MSG_PACK SendPack;
        SendPack.cmd = 202;
        SendPack.fin = 1;
        SendPack.len = StrdateTime.length();
        memcpy(SendPack.data, StrdateTime.toStdString().c_str(), StrdateTime.length());
        int lenth = StrdateTime.length() + 12;
        m_pConnectedSocket->write((char*)(&SendPack), lenth);
    }
}

void TCPDataClient::onNewConnection()
{
    m_pConnectedSocket = m_pTcpServer->nextPendingConnection();
    connect(m_pConnectedSocket, SIGNAL(readyRead()), this, SLOT(readMessage())); //服务器接收客户端的消息
    connect(m_pConnectedSocket, SIGNAL(disconnected()), m_pConnectedSocket, SLOT(deleteLater()));
}
