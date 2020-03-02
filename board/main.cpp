#include <iostream>
#include <string>
#include <vector>

#include "IIODevice.h"
#include "UDPServer.h"
#include "Controller.h"

using namespace std;

IIODevice* iiodev;
Controller* controller;
UDPServer* server;

void shutdown(int num)
{
	free(controller); // First
	free(iiodev); // Second
	free(server); // Third
	exit(0);
}

/* simple configuration and streaming */
int main (int argc, char **argv)
{
	// Listen to ctrl+c and assert
	signal(SIGINT, shutdown);

	// Start IIODevice
	struct stream_cfg initRXConf;

	initRXConf.bw_hz = MHZ(4.695); 
	initRXConf.fs_hz = MHZ(5.76);
	initRXConf.lo_hz = GHZ(2.680005);
	initRXConf.rfport = "A_BALANCED";

	struct stream_cfg initTXConf;
	
	initTXConf.bw_hz = MHZ(4.373);
	initTXConf.fs_hz = MHZ(5.76);
	initTXConf.lo_hz = GHZ(2.560005);
	initTXConf.rfport = "A";

	int rxBufferSizeSample = 15*1024; // AD9361 IIO RX
	int txBufferSizeSample = 15*1024; // AD9361 IIO TX

	int commandPort = 50707;
	int streamPort = 50708;

	try
	{
	        iiodev = new IIODevice(rxBufferSizeSample,txBufferSizeSample, initRXConf, initTXConf);
		controller = new Controller(iiodev);	
		server = new UDPServer(commandPort,streamPort,txBufferSizeSample*4,rxBufferSizeSample*4,controller);

		while(true)
			server->runCommand();

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in initialization: " << re.what() << endl;
        }
	shutdown(0);
	return 0;
}




