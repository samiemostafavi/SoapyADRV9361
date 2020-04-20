#include "UDPClient.h"

static UDPClient* clnt = NULL;

UDPClient* UDPClient::findServer(int _serverCommandPort,int _serverStreamPort, string _serverIP)
{
	try
	{
		if(clnt == NULL)
			clnt = new UDPClient(_serverCommandPort,_serverStreamPort,_serverIP,(DEFAULT_RX_BUFFER_SIZE)*4,(DEFAULT_TX_BUFFER_SIZE)*4);
	}
	catch(runtime_error& re)
        {
		printf("Error find ADRV: %s \n",re.what());
		clnt = NULL;
        }
	return clnt;
}

UDPClient::UDPClient(int _serverCommandPort,int _serverStreamPort,string _serverIP,int _rxBufferSizeByte,int _txBufferSizeByte) : 
	serverCommandPort(_serverCommandPort), serverStreamPort(_serverStreamPort), serverIP(_serverIP) , rxBufferSizeByte(_rxBufferSizeByte), txBufferSizeByte(_txBufferSizeByte)
{
	send_count = 0;
	recv_count = 0;

	// create servCMDAddr
        memset(&servCMDAddr, 0, sizeof(servCMDAddr));
        servCMDAddr.sin_family = AF_INET;                   	// Internet address family
	servCMDAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());      // Server IP address
        servCMDAddr.sin_port = htons(serverCommandPort);    	// Local port
        
	// Create the command socket for receiving datagrams
        if ((commandSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the command socket");

	// Set waiting time limit
        /*tv.tv_sec = 2;
        if (setsockopt(commandSocket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
                throw runtime_error("Unable to set waiting time for the command socket");*/
        
	// Set the socket as reusable
        int true_v = 1;
        if (setsockopt(commandSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the command socket reusable");

        // Connecting to the cmd socket of the server
        if (connect(commandSocket,(struct sockaddr*) &servCMDAddr, sizeof(servCMDAddr)) < 0)
                throw runtime_error("Command socket connection failed, ip: " + serverIP + " port: " + to_string(serverCommandPort));

	// create servSTRAddr
        memset(&servSTRAddr, 0, sizeof(servSTRAddr));
        servSTRAddr.sin_family = AF_INET;                   /* Internet address family */
	servSTRAddr.sin_addr.s_addr = inet_addr(serverIP.c_str());      /* Server IP address */
        servSTRAddr.sin_port = htons(serverStreamPort);    /* Local port */

	// Create streaming socket for receiving datagrams
        if ((streamSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                throw runtime_error("Unable to create the stream socket");

	// Set the socket as reusable
        true_v = 1;
        if (setsockopt(streamSocket, SOL_SOCKET, SO_REUSEADDR, &true_v, sizeof (int))!=0)
                throw runtime_error("Unable to make the streaming socket reusable");

        // Connecting to the streaming socket of the server
        if (connect(streamSocket,(struct sockaddr*) &servSTRAddr, sizeof(servSTRAddr)) < 0)
                throw runtime_error("Stream socket connection failed, ip: " + serverIP + " port: " + to_string(serverStreamPort));

	// Init procedure (send cmd init, receive cmd ack, send buffer dummy)
	initProcedure();
}

UDPClient::~UDPClient()
{
	close(commandSocket);
	close(streamSocket);
}


void UDPClient::initProcedure()
{
	char dummy[txBufferSizeByte];
	string res = sendCommand("init");
	if(res == string("ack"))
		sendStreamBuffer(dummy);
	else
		throw runtime_error(string("Did not received ack from the server: ")+res);

	cout << "Connected to the ADRV board (server), ip: " << serverIP << " port: " << to_string(serverStreamPort) << endl;
}

string UDPClient::sendCommand(string cmd)
{
	// send the command over the command socket
	if(send(commandSocket,cmd.c_str(), cmd.length(), 0) < 0)
        	throw runtime_error("Send command failed");

	// wait for the response (1 second)
	char recvMsg[MESSAGE_LENGTH_CHAR];
	int recvMsgSize = recv(commandSocket, recvMsg, MESSAGE_LENGTH_CHAR, 0);
        if(errno==EAGAIN)
        {
        	setsockopt(commandSocket, SOL_SOCKET, 0,&tv,sizeof(tv));
        	throw runtime_error("Command response timeout is reached");
        }
        if(recvMsgSize<0)
        	throw runtime_error("Unable to receive the command response");

	return string(recvMsg,recvMsgSize);
}


void UDPClient::setTXBufferSizeByte(int sizeByte)
{
	if(sizeByte > 65535)
		throw runtime_error("Maximum udp packet size: 65535");

	string res = sendCommand("set tx buffersize " + to_string(sizeByte));

	if(res == "done")
		txBufferSizeByte = sizeByte;
	else
		throw runtime_error(res);
}

void UDPClient::setRXBufferSizeByte(int sizeByte)
{
	if(sizeByte > 65535)
		throw runtime_error("Maximum udp packet size: 65535");
	
	string res = sendCommand("set rx buffersize " + to_string(sizeByte));

	if(res == "done")
		rxBufferSizeByte = sizeByte;
	else
		throw runtime_error(res);
}

int UDPClient::sendStreamBuffer(char* pBuffer)
{
	int ret = send(streamSocket, pBuffer, txBufferSizeByte, 0);
	if(ret < 0)
		return ret;

	send_count += ret;
	return ret;
}

int UDPClient::receiveStreamBuffer(char* pBuffer)
{
	int ret = recv(streamSocket, pBuffer, rxBufferSizeByte, 0);
        if(ret < 0)
		return ret;

	recv_count += ret;
	return ret;
}
