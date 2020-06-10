#include "UDPServer.h"

UDPServer::UDPServer(int _commandPort,int _streamPort,int _rxBufferSizeByte,int _txBufferSizeByte, Controller* _controller) :
	commandPort(_commandPort), streamPort(_streamPort), rxBufferSizeByte(_rxBufferSizeByte), txBufferSizeByte(_txBufferSizeByte), controller(_controller)
{
	send_count = 0;
	recv_count = 0;
	recv_fr_count = 0;
	send_fr_count = 0;
	sendout_fr_count = 0;
	clientInitiated = false;
	rxThreadRunning = false;

	command_thread_active = true;

        memset(&servCMDAddr, 0, sizeof(servCMDAddr));
        servCMDAddr.sin_family = AF_INET;                  // Internet address family
        servCMDAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        servCMDAddr.sin_port = htons(commandPort);           // Local port

        // Create socket for receiving datagrams
        if ((commandSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the command socket");

        // Set the socket as reusable
        int true_v = 1;
        if (setsockopt(commandSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the command socket reusable");

        // Bind to the local address
        if (bind(commandSocket, (struct sockaddr *) &servCMDAddr, sizeof(servCMDAddr)) < 0)
                throw runtime_error("Unable to bind the socket");

       	// Streaming  
	memset(&servSTRAddr, 0, sizeof(servSTRAddr));
        servSTRAddr.sin_family = AF_INET;                  // Internet address family
        servSTRAddr.sin_addr.s_addr = htonl(INADDR_ANY);   // Any incoming interface
        servSTRAddr.sin_port = htons(streamPort);           // Local port
        
	// Create socket for receiving datagrams
        if ((streamSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the command socket");

        // Set the socket as reusable
        true_v = 1;
        if (setsockopt(streamSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the command socket reusable");

        // Bind to the local address
        if (bind(streamSocket, (struct sockaddr *) &servSTRAddr, sizeof(servSTRAddr)) < 0)
                throw runtime_error("Unable to bind the socket");

	// Init controller server
	controller->setServer(this);
	
	// A clean controller stop
	controller->stop(RX);
	controller->stop(TX);
	
	// Start the command runner thread (waits for the client "init" command)
        //if(pthread_create(&command_thread, NULL, &UDPServer::runCommands,this) != 0)
        //        throw runtime_error("Unable to start command thread");


	// Init rx circullar buffer
	rxCBufferSize = 1000;
	rxCBuffer = vector<char>(rxCBufferSize*rxBufferSizeByte,0);


}

UDPServer::~UDPServer()
{
	// Stop the rx streamer thread
	rxThreadRunning = false;

        // Join thread
	pthread_cancel(rxThread);
        pthread_join(rxThread, NULL);

	command_thread_active = false;
	pthread_join(command_thread, NULL);
	close(commandSocket);
	close(streamSocket);
}

void UDPServer::initClient(struct sockaddr_in cliAddr)
{
	if(rxThreadRunning)
	{
		// stop circullar buffer thread
		rxThreadRunning = false;

        	// Join thread
	        pthread_cancel(rxThread);
        	pthread_join(rxThread, NULL);
	}
	
	// stop controller
	controller->stop(RX);
	controller->stop(TX);

	// set the new address
	clntCMDAddr = cliAddr;

	// Send the response
	string a = string("ack");
        int sendMsgSize = sendto(commandSocket, a.c_str(), a.length(), 0,(struct sockaddr*) &(clntCMDAddr), sizeof(clntCMDAddr));
        if (sendMsgSize<0)
        	throw runtime_error("Unable to respond to commands");

        // Blocking receive of the first stream buffer (dummy)
        char dummy[rxBufferSizeByte];
	unsigned int cliAddrLen = sizeof(clntSTRAddr);
	int recvDummySize = recvfrom(streamSocket, dummy, rxBufferSizeByte, 0,(struct sockaddr *) &(clntSTRAddr), &cliAddrLen);
        if(recvDummySize < 0)
                throw runtime_error("Failed to receive the dummy buffer");
	
	// Printout the result
	char ip[INET_ADDRSTRLEN];
	uint16_t cmdport;
	uint16_t strport;

	inet_ntop(AF_INET, &(clntCMDAddr.sin_addr), ip, sizeof(ip));
	cmdport = htons(clntCMDAddr.sin_port);
	strport = htons(clntSTRAddr.sin_port);

	clientInitiated = true;

	// Start a new rx circullar buffer thread
	rxThreadRunning = true;
	if(pthread_create(&rxThread, NULL, &UDPServer::receiveStreamBufferCon, this) != 0)
                throw runtime_error("Unable to start UDP rx stream thread");

	printf("UDPServer is connected to %s, cmd port: %d, str port: %d, continue...\n",ip,cmdport,strport);
}

void* UDPServer::runCommands(void* server)
{
	UDPServer* p = static_cast<UDPServer*>(server);
	try
	{
		while(p->command_thread_active)
		{
			int ret;
			char msg[MESSAGE_LENGTH_CHAR];
			struct sockaddr_in cliAddr;
			unsigned int cliAddrLen = sizeof(cliAddr);
			if ((ret = recvfrom(p->commandSocket, msg, MESSAGE_LENGTH_CHAR, 0,(struct sockaddr *) &(cliAddr), &cliAddrLen)) < 0 )
        	        	throw runtime_error("Unable to receive first command");
			
			if(ret == 0)
			{
				cout << "UDP empty datagram received, continue..." << endl;
				continue;
			}


			if(string(msg,ret)=="init")
			{
				// A new client is there, fix the addresses
				p->initClient(cliAddr);
				continue;
			}
	
			if(!p->clientInitiated)
			{
				// Client init did not happen
				cout << "Client has not been initialized, continue..." << endl;
				continue;
			}
				
			// Send the string up! Process the command
			string response = p->controller->runCommand(string(msg,ret));

			// Send back the response
			int sendMsgSize = sendto(p->commandSocket, response.c_str(), response.length(), 0,(struct sockaddr*) &(p->clntCMDAddr), sizeof(p->clntCMDAddr));
                	if (sendMsgSize<0)
        	        	throw runtime_error("Unable to respond to the incomming commands");
	
		}
	}
	catch(runtime_error& re)
	{
		cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
	}

	// call the upper layer to stop everything
	p->controller->stop(RX);
	p->controller->stop(TX);
		
	close(p->commandSocket);
	close(p->streamSocket);
	
	printf("Network UDP command serving thread is stopped\n");
}

void UDPServer::runCommand()
{
	int ret;
	char msg[MESSAGE_LENGTH_CHAR];
	memset(msg,0,MESSAGE_LENGTH_CHAR);
	struct sockaddr_in cliAddr;
	unsigned int cliAddrLen = sizeof(cliAddr);
	ret = recvfrom(commandSocket, msg, MESSAGE_LENGTH_CHAR, 0,(struct sockaddr *) &(cliAddr), &cliAddrLen);
	if (ret <= 0)
               	throw runtime_error("Unable to receive first command");

	//cout << "char: " << ret << " " << msg << endl;
	
	char res[ret];
	strcpy(res,msg);
	
	string smsg;
	smsg.assign(res,ret);
	
	//cout << "Received cmdstr: " << smsg << endl;
	
	if(ret == 0)		
	{
		cout << "UDP empty datagram received, continue..." << endl;
		return;
	}


	if(smsg=="init")
	{
		// A new client is there, fix the addresses
		initClient(cliAddr);
		return;
	}
	
	if(!clientInitiated)
	{
		// Client init did not happen
		cout << "Client has not been initialized, continue..." << endl;
		return;
	}
				
	// Send the string up! Process the command
	string response = controller->runCommand(smsg);

	// Send back the response
	int sendMsgSize = sendto(commandSocket, response.c_str(), response.length(), 0,(struct sockaddr*) &(clntCMDAddr), sizeof(clntCMDAddr));
        if (sendMsgSize<0)
        	throw runtime_error("Unable to respond to the incomming commands");
}

int UDPServer::sendStreamBuffer(char* pBuffer)
{
	int ret = sendto(streamSocket, pBuffer, txBufferSizeByte, 0, (struct sockaddr*) &(clntSTRAddr), sizeof(clntSTRAddr));
	if(ret < 0)
        	throw runtime_error("Sending the buffer failed");
	
	send_count += ret;
	send_fr_count ++;
	return ret;
}

uint16_t oldid = 0;
uint16_t accdrops = 0;

void* UDPServer::receiveStreamBufferCon(void* server)
{
	UDPServer* p = static_cast<UDPServer*>(server);
	try
	{
		while(p->rxThreadRunning)
		{
			// indx = rxBufferSizeByte*(recv_fr_count % rxCBufferSize)
			int index = (p->rxBufferSizeByte)*((p->recv_fr_count) % (p->rxCBufferSize));
			int ret = recv(p->streamSocket, &(p->rxCBuffer[index]), p->rxBufferSizeByte, 0);
	        	if(ret < 0)
        			throw runtime_error("Receiving the buffer faild");
	        	
			if(ret != p->rxBufferSizeByte)
        			throw runtime_error("Received less than expected");

			// Just a little test for reading the id
			char* pFirst = &(p->rxCBuffer[index]);
			uint16_t* txidp = (uint16_t*)(pFirst + (5760+1500)*4 + 4);
			if(*txidp - oldid > 1)
			{
				accdrops += *txidp - oldid -1;
				cout << accdrops << endl;
			}
			oldid = *txidp;

			p->recv_count += ret;
			p->recv_fr_count++;
		}

	}
        catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer receiveBufferCon: " << re.what() << endl;
        }

        // call the upper layer to stop everything
        p->controller->stop(RX);
        p->controller->stop(TX);

        close(p->commandSocket);
        close(p->streamSocket);

        printf("Network UDP command serving thread is stopped\n");

	return NULL;
}

int behind = 0;

int UDPServer::receiveStreamBuffer(char* pBuffer)
{

	if(recv_fr_count == sendout_fr_count)
	{
		do
		{
			usleep(500); // 0.5ms
		}
		while(recv_fr_count == sendout_fr_count);
	}
	// indx = rxBufferSizeByte*(sendout_fr_count % rxCBufferSize)
	memcpy( pBuffer, &rxCBuffer[rxBufferSizeByte*(sendout_fr_count % rxCBufferSize)], rxBufferSizeByte);

	sendout_fr_count ++;
	return rxBufferSizeByte;
}


void UDPServer::setTXBufferSizeByte(int size)
{
	if(size > 65535)
		throw runtime_error("Unable to set larger than 65535 buffer size TX");

	txBufferSizeByte = size;
}

void UDPServer::setRXBufferSizeByte(int size)
{
	if(size > 65535)
		throw runtime_error("Unable to set larger than 65535 buffer size RX");

	rxBufferSizeByte = size;
}
