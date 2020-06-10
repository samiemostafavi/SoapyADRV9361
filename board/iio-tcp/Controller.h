#ifndef CONTROLLERN_H
#define CONTROLLERN_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "IIODevice.h"
#include "UDPServer.h"

struct UDPServer;

struct Controller
{
	struct UDPServer* server;
	bool rx_thread_active;
	bool tx_thread_active;
	pthread_t rx_thread;
	pthread_t tx_thread;
};

static void* streamRX(void* cont);
static void* streamTX(void* cont);

void ControllerInit();
void ControllerDelete();
int runCommand(char* cmdStr,int cmdLen, char* res);
void stop(enum iodev d);
void setServer(struct UDPServer* _server);


#endif

