#ifndef CELEXTCPSOCKET_H
#define CELEXTCPSOCKET_H

#include <stdint.h>

typedef struct TCP_MSG_PACK
{
	int cmd;
	int value1;
	int value2;
};

#define CMD_SET_FIXED_MODE    1
#define CMD_ENABLE_LOOP_MODE  3
#define CMD_SET_LOOP_MODE     5

#define CMD_RESET      10

#define CMD_SET_CLOCK         100
#define CMD_SET_BRIGHTNESS    101
#define CMD_SET_CONTRAST      102
#define CMD_SET_THRESHOLD     103
#define CMD_SET_ISO           104

#define CMD_SET_LOOP_EVENT_DURATION   105
#define CMD_SET_LOOP_FULL_PIC_NUM     106
#define CMD_SET_LOOP_S_FO_PIC_NUM     107
#define CMD_SET_LOOP_M_FO_PIC_NUM     108

class CeleXTcpSocket
{
public:
    CeleXTcpSocket();
	~CeleXTcpSocket();

	//SOCKET getSocket();
	int getSocket();
	int connectToZYNQServer();
	int recvData(char* buffer, int size);

	bool sendCmd(TCP_MSG_PACK msg);

private:
	int         m_uiServerSocket; //Server socket
	int         m_iConnectState;
};

#endif // CELEXTCPSOCKET_H