#include <vector>
#include <string>
#include <stdexcept>
#include <errno.h>
#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <signal.h>
#include <iostream>

using namespace std;

#define DUMMYBUF_SIZE_BYTE 4096*4
#define MESSAGE_LENGTH_CHAR 1024
#define DUAL_ETHERNET 1


class UDPClient
{
	public:
		UDPClient(int _serverCommandPort,int _serverStreamPort, string _serverIP,int _rxBufferSizeByte,int _txBufferSizeByte);
		~UDPClient();
		static UDPClient* findServer(int _serverCommandPort,int _serverStreamPort, string _serverIP,int _rxBufferSizeByte,int _txBufferSizeByte);
		string sendCommand(string cmd);
		int sendStreamBuffer(char* pBuffer);
		int receiveStreamBuffer(char* pBuffer);
		void setTXBufferSizeByte(int sizeByte);
		void setRXBufferSizeByte(int sizeByte);
		int getTXBufferSizeByte() { return txBufferSizeByte; }
		int getRXBufferSizeByte() { return rxBufferSizeByte; }
		int getServerCommandPort() { return serverCommandPort; }
		int getServerStreamPort() { return serverStreamPort; }
		int getSentCount() { return send_count; }
		int getRecvCount() { return recv_count; }
		void initProcedure();
	private:
		struct sockaddr_in servCMDAddr;
		struct sockaddr_in servSTRAddr;
		struct sockaddr_in servSTRAddr_tx;
		int serverCommandPort;
		int serverStreamPort;
		int streamSocket;
		int streamSocket_tx;
		int commandSocket;
		int rxBufferSizeByte;
		int txBufferSizeByte;
		string serverIP;
		string serverIP_tx;
		struct timeval tv;
		int send_count;
		int recv_count;
};

