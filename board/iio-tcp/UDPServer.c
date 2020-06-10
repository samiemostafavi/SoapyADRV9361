#include "UDPServer.h"


struct UDPServer* us = NULL;

int UDPServerInit(int _commandPort,int _streamPort,int _rxBufferSizeByte,int _txBufferSizeByte)
{
	us = (struct UDPServer*) malloc(sizeof(struct UDPServer));

	us->send_count = 0;
	us->recv_count = 0;
	us->clientInitiated = false;
	
	us->commandPort = _commandPort;
	us->streamPort = _streamPort;
	us->rxBufferSizeByte = _rxBufferSizeByte;
	us->txBufferSizeByte = _txBufferSizeByte;

	us->command_thread_active = true;

        memset(&us->servCMDAddr, 0, sizeof(us->servCMDAddr));
        us->servCMDAddr.sin_family = AF_INET;                  // Internet address family
        us->servCMDAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        us->servCMDAddr.sin_port = htons(us->commandPort);           // Local port

        // Create socket for receiving datagrams
        if ((us->commandSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
                printf("Unable to create the command socket\n");
		return -1;
	}

        // Set the socket as reusable
        int true_v = 1;
        if (setsockopt(us->commandSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
	{
                printf("Unable to make the command socket reusable\n");
		return -1;
	}

        // Bind to the local address
        if (bind(us->commandSocket, (struct sockaddr *) &us->servCMDAddr, sizeof(us->servCMDAddr)) < 0)
	{
                printf("Unable to bind the socket\n");
		return -1;
	}

       	// Streaming  
	memset(&us->servSTRAddr, 0, sizeof(us->servSTRAddr));
        us->servSTRAddr.sin_family = AF_INET;                  // Internet address family
        us->servSTRAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        us->servSTRAddr.sin_port = htons(us->streamPort);           // Local port
        
	// Create socket for receiving datagrams
        if ((us->commandSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
	{
                printf("Unable to create the command socket\n");
		return -1;
	}

        // Set the socket as reusable
        true_v = 1;
        if (setsockopt(us->commandSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
	{
                printf("Unable to make the command socket reusable\n");
		return -1;
	}

        // Bind to the local address
        if (bind(us->commandSocket, (struct sockaddr *) &us->servSTRAddr, sizeof(us->servSTRAddr)) < 0)
	{
                printf("Unable to bind the socket\n");
		return -1;
	}

	// Init controller server
	setServer(us);
	
	// A clean controller stop
	stop(RX);
	stop(TX);
	
	// Start the command runner thread (waits for the client "init" command)
        if(pthread_create(&us->command_thread, NULL, &runCommands,us) != 0)
	{
                printf("Unable to start command thread\n");
		return -1;
	}

	return 0;
}

void UDPServerDelete()
{
	us->command_thread_active = false;
	pthread_join(us->command_thread, NULL);
	close(us->commandSocket);
	close(us->streamSocket);

	free(us);
	us = NULL;
}

int initClient(struct sockaddr_in cliAddr)
{
	// stop controller
	stop(RX);
	stop(TX);

	// set the new address
	us->clntCMDAddr = cliAddr;

	// Send the response
	char res[4]  = "ack";
        int sendMsgSize = sendto(us->commandSocket, res, 5, 0,(struct sockaddr*) &(us->clntCMDAddr), sizeof(us->clntCMDAddr));
        if (sendMsgSize<0)
	{
        	printf("Unable to respond to commands\n");
		return -1;
	}

        // Blocking receive of the first stream buffer (dummy)
        char dummy[us->rxBufferSizeByte];
        if (receiveStreamBuffer(dummy) < 0)
	{
                printf("Failed to receive the dummy buffer\n");
		return -1;
	}
	
	// Printout the result
	char ip[INET_ADDRSTRLEN];
	uint16_t cmdport;
	uint16_t strport;

	inet_ntop(AF_INET, &(us->clntCMDAddr.sin_addr), ip, sizeof(ip));
	cmdport = htons(us->clntCMDAddr.sin_port);
	strport = htons(us->clntSTRAddr.sin_port);

	us->clientInitiated = true;

	printf("UDPServer is connected to %s, cmd port: %d, str port: %d, continue...\n",ip,cmdport,strport);
	return 0;
}

void* runCommands(void* server)
{
	struct UDPServer* p = (struct UDPServer*)(server);
	while(p->command_thread_active)
	{
		int ret;
		char msg[MESSAGE_LENGTH_CHAR];
		struct sockaddr_in cliAddr;
		unsigned int cliAddrLen = sizeof(cliAddr);
		if ((ret = recvfrom(p->commandSocket, msg, MESSAGE_LENGTH_CHAR, 0,(struct sockaddr *) &(cliAddr), &cliAddrLen)) < 0 )
	        	printf("Unable to receive first command\n");

		if(ret == 0)
		{
			printf("UDP empty datagram received, continue...\n");
			continue;
		}

		if(strcmp(msg,"init")==0)
		{
			// A new client is there, fix the addresses
			if(initClient(cliAddr)<0)
			{
				printf("Init the new client failed.\n");
				break;
			}

			continue;
		}

		if(!p->clientInitiated)
		{
			// Client init did not happen
			printf("Client has not been initialized, continue...\n");
			continue;
		}
			
		// Send the string up! Process the command
		char res[MESSAGE_LENGTH_CHAR];
		runCommand(msg,ret,res);

		// Send back the response
		int sendMsgSize = sendto(p->commandSocket, res, strlen(res), 0,(struct sockaddr*) &(p->clntCMDAddr), sizeof(p->clntCMDAddr));
        	if (sendMsgSize<0)
	        	printf("Unable to respond to the incomming commands\n");

	}

	// call the upper layer to stop everything
	stop(RX);
	stop(TX);
		
	close(p->commandSocket);
	close(p->streamSocket);
	
	printf("Network UDP command serving thread is stopped\n");
	return NULL;
}

int sendStreamBuffer(char* pBuffer)
{
	int ret = sendto(us->streamSocket, pBuffer, us->txBufferSizeByte, 0, (struct sockaddr*) &(us->clntSTRAddr), sizeof(us->clntSTRAddr));
	if(ret < 0)
	{
        	printf("Sending the buffer failed\n");
	}
	else
		us->send_count += ret;

	return ret;
}

int receiveStreamBuffer(char* pBuffer)
{
	int ret = recv(us->streamSocket, pBuffer, us->rxBufferSizeByte, 0);
        if(ret < 0)
	{
        	printf("Receiving the buffer faild\n");
	}
	else
		us->recv_count += ret;
	
	return ret;
}
