#ifndef UDPSERVER_H
#define UDPSERVER_H

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
#include <stdio.h>
#include <string.h>

#include "Controller.h"


#define MESSAGE_LENGTH_CHAR 512


struct UDPServer
{
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
        pthread_t command_thread;
        bool command_thread_active;
	bool clientInitiated;
};
                
int UDPServerInit(int _commandPort,int _streamPort,int _rxBufferSizeByte,int _txBufferSizeByte);
void UDPServerDelete();
static void* runCommands(void* server);
int sendStreamBuffer(char* pBuffer);
int receiveStreamBuffer(char* pBuffer);
int initClient(struct sockaddr_in addr);

#endif
