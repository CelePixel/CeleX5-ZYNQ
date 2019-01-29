#ifndef TCPDATACLIENT_H
#define TCPDATACLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QPixmap>
#include <QTcpServer>

#include "socketcfg.h"


//enum TcpCmdType
//{
//    CMD_Unknown = 0,
//    CMD_GetModeState = 1,      //Get Sensor Fixed Mode
//    CMD_SetModeState = 2,      //Set Sensor Fixed Mode
//    CMD_SetBrightness = 3,     //Set Brightness
//    CMD_GetBrightness = 4,     //Get Brightness
//    CMD_SetThreshold = 5,      //Set Threshold
//    CMD_GetThreshold = 6,      //Get Threshold
//    CMD_SetContrast = 7,       //Set Contrast
//    CMD_GetContrast = 8,       //Get Contrast
//};

//enum TcpReplyType
//{
//    Reply_Unknown = 0,
//    Reply_GetModeState = 1,      //Get Sensor Fixed Mode
//    Reply_SetModeState = 2,      //Set Sensor Fixed Mode
//    Reply_SetBrightness = 3,     //Set Brightness
//    Reply_GetBrightness = 4,     //Get Brightness
//    Reply_SetThreshold = 5,      //Set Threshold
//    Reply_GetThreshold = 6,      //Get Threshold
//    Reply_SetContrast = 7,       //Set Contrast
//    Reply_GetContrast = 8,       //Get Contrast
//};

typedef struct MSG_PACKHEAD
{
    int cmd; //4 Byte, 命令
    int fin; //4 Byte, 是否是该命令的最后一包
    int len; //4 Byte, data里有效数据的字节数
    MSG_PACKHEAD()
    {
        cmd = 0;
        fin = 1;
        len = 0;
    }
}*pMSG_PACKHEAD, MSG_PACKHEAD;

Q_DECLARE_METATYPE(pMSG_PACKHEAD)
Q_DECLARE_METATYPE(MSG_PACKHEAD)

typedef struct Request_MSG_PACK
{
    int cmd; //4 Byte, 命令
    int fin; //4 Byte, 是否是该命令的最后一包
    int len; //4 Byte, data里有效数据的字节数
    char data[2000]; //数据区
}*pRequest_MSG_PACK, Request_MSG_PACK;

typedef struct SEND_MSG_PACK
{
    int cmd;
    int value;
    int value2;
}*PSEND_MSG_PACK, SEND_MSG_PACK;

class TCPDataClient: public QObject
{
    Q_OBJECT
public:
    TCPDataClient(QObject *parent = 0);
    ~TCPDataClient();

    void connectToZYNQServer();
    QTcpSocket *getSocket() { return m_pTcpSocket; }
    bool sendCmd(int cmd,QString stdData = "");
    bool sendCmd(SEND_MSG_PACK msg);
    bool getSocketStatue() { return m_bConnected;}

signals:
    void sglRcvPack(pMSG_PACKHEAD pack, QByteArray data);
    void pixmapReady(QByteArray);

public slots:
    bool sendData(pRequest_MSG_PACK pMsg);

private slots:
    void connectToServer(const std::string &hostName, uint16_t port);
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError();

    void onNewConnection();
    void readMessage();

private:
    QTcpSocket*    m_pTcpSocket;
    bool           m_bConnected;
    QByteArray     m_buffer;

    QByteArray     m_recvData;

    qint64         m_DataSize;  //The actual picture size
    qint64         m_checkSize; //The received picture size

    QTcpServer*    m_pTcpServer;
    QTcpSocket*    m_pConnectedSocket; //The client connnected to m_pTcpServer
};

#endif // TCPDATACLIENT_H
