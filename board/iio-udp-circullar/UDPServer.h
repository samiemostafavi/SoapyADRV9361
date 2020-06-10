#ifndef UDPSERVER_H
#define UDPSERVER_H

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
#include <unistd.h>

#include "Controller.h"

using namespace std;

#define MESSAGE_LENGTH_CHAR 1024

class Controller;

class UDPServer
{
        public:
                UDPServer(int _commandPort,int _streamPort,int _rxBufferSizeByte,int _txBufferSizeByte,Controller* _controller);
                ~UDPServer();
                static void* runCommands(void* server);
		void runCommand();
                int sendStreamBuffer(char* pBuffer);
                int receiveStreamBuffer(char* pBuffer);
                void setTXBufferSizeByte(int size);
                void setRXBufferSizeByte(int size);
                int getTXBufferSizeByte() { return txBufferSizeByte; }
                int getRXBufferSizeByte() { return rxBufferSizeByte; }
                int getSentCount() { return send_count; }
                int getRecvCount() { return recv_count; }
                struct sockaddr_in getClientSTRAddr() { return clntSTRAddr; }
                struct sockaddr_in getClientCMDAddr() { return clntCMDAddr; }
                static void* receiveStreamBufferCon(void* server);
        private:
		void initClient(struct sockaddr_in addr);
		Controller* controller;
                struct sockaddr_in servCMDAddr;
                struct sockaddr_in servSTRAddr;
                struct sockaddr_in clntCMDAddr;
                struct sockaddr_in clntSTRAddr;
                int commandPort;
                int streamPort;
                int streamSocket;
                int commandSocket;
                int rxBufferSizeByte;
                int txBufferSizeByte;
                int send_count;
                int recv_count;
                int send_fr_count;
                int recv_fr_count;
		int sendout_fr_count;
                pthread_t command_thread;
                bool command_thread_active;
		bool clientInitiated;
		int rxCBufferSize;
		vector<char> rxCBuffer;
		bool rxThreadRunning;
		pthread_t rxThread;
};

#endif
