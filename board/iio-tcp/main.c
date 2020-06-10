#include <stdlib.h>
#include <stdio.h>

#include "IIODevice.h"
#include "UDPServer.h"
#include "Controller.h"

void mshutdown(int num)
{
	ControllerDelete();
	IIODeviceDelete();
	UDPServerDelete();
}

/* simple configuration and streaming */
int main (int argc, char **argv)
{
	// Listen to ctrl+c and assert
	signal(SIGINT, mshutdown);


	long long rxBW = MHZ(4.695); 
	long long rxFS = MHZ(1.92);
	long long rxLO = GHZ(2.680005);
	char rxRFPort[64] = "A_BALANCED";

	long long txBW = MHZ(4.373); 
	long long txFS = MHZ(1.92);
	long long txLO = GHZ(2.560005);
	char txRFPort[64] = "A";

	int rxBufferSizeSample = 15*1024; // AD9361 IIO RX
	int txBufferSizeSample = 15*1024; // AD9361 IIO TX

	int commandPort = 50707;
	int streamPort = 50708;

	if(IIODeviceInit(rxBufferSizeSample,txBufferSizeSample,rxRFPort,rxBW,rxFS,rxLO,txRFPort,txBW,txFS,txLO)<0)
		printf("Error in IIODevice init\n");

	ControllerInit();

	if(UDPServerInit(commandPort,streamPort,txBufferSizeSample*4,rxBufferSizeSample*4)<0)
		printf("Error in UDPServer init\n");
	
	mshutdown(0);
	return 0;
}




