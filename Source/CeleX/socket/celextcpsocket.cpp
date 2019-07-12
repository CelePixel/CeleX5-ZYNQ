
#include "celextcpsocket.h"
#include <iostream>

#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#endif

using namespace std;

//#define  SERVER_PORT    1234
//#define  SERVER_IP      "192.168.1.12" //192.168.1.11  192.168.8.37

CeleXTcpSocket::CeleXTcpSocket() 
	: m_uiServerSocket(0)
	, m_iConnectState(-1)
{
#ifdef _WIN32
	//Initialize socket dynamic lib
	WSADATA wsd; //WSADATA Variable  
	if (WSAStartup(MAKEWORD(2, 2), &wsd) == 0)
	{
		//Create socket  
		m_uiServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == m_uiServerSocket)
		{
			cout << "create socket failed!" << endl;
			WSACleanup();//Release socket resource  
		}
		cout << "create socket successfully!" << endl;
	}
	else
	{
		cout << "WSAStartup failed!" << endl;
	}
#else
	//create listen socket
	m_uiServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_uiServerSocket < 0)
	{
		cout << "create socket failed!" << endl;
	}
	cout << "create socket successfully!" << endl;
#endif
}

CeleXTcpSocket::~CeleXTcpSocket()
{
#ifdef _WIN32
	closesocket(m_uiServerSocket);
	WSACleanup();
#else
	close(m_uiServerSocket);
#endif
	m_uiServerSocket = -1;
	m_iConnectState = -1;
}

int CeleXTcpSocket::getSocket()
{
	return m_uiServerSocket;
}

int CeleXTcpSocket::connectToZYNQServer(std::string address, unsigned short port)
{
	if (m_iConnectState != -1)
	{
		return m_iConnectState;
	}
#ifdef _WIN32
	//Set server address 
	SOCKADDR_IN servAddr; //Server Address  
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr(address.data()); //server ip: 192.168.1.11
	servAddr.sin_port = htons(port); //server port

	//Connect Server
	m_iConnectState = connect(m_uiServerSocket, (LPSOCKADDR)&servAddr, sizeof(servAddr));
	while (-1 == m_iConnectState)
	{
		cout << "connect failed, try to connect again, please wait..." << endl;
		m_iConnectState = connect(m_uiServerSocket, (LPSOCKADDR)&servAddr, sizeof(servAddr));
		//closesocket(m_uiServerSocket); //close socket 
		//WSACleanup(); //Release socket resource   
		//return -1;
	}
#else
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(address.data());
	bzero(&(server_addr.sin_zero), sizeof(server_addr.sin_zero));

	struct timeval timeout = { 15, 0 };
	if (setsockopt(m_uiServerSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(struct timeval)) != 0)
	{
		printf("TCPSendDataClient set timeout failed\n");
	}
	m_iConnectState = connect(m_uiServerSocket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	while (-1 == m_iConnectState)
	{
		cout << "connect failed, try to connect again, please wait..." << endl;
		m_iConnectState = connect(m_uiServerSocket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	}
#endif
	cout << "connect to ZYNQServer successfully!" << endl;
	return m_iConnectState;
}

int CeleXTcpSocket::recvData(char* buffer, int size)
{
	if (m_uiServerSocket < 0)
		return -1;
	memset(buffer, 0, size);
	return recv(m_uiServerSocket, buffer, size, 0); // Accepting data from Server
}

bool CeleXTcpSocket::sendCmd(TCP_MSG_PACK msg)
{
	cout << "CeleXTcpSocket::sendCmd: cmd = " << msg.cmd << endl;
	//Sending data to the Server 
	int retVal = send(m_uiServerSocket, (char*)&msg, sizeof(TCP_MSG_PACK), 0);
	if (-1 == retVal)
	{
		cout << "send failed!" << endl;
		return false;
	}
	return true;
}
