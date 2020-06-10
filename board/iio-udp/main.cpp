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

	// Get the CPU affinity of the current thread and set it to CPU 1
	pthread_t mainThread = pthread_self();
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(1,&cpuset);
	if(pthread_setaffinity_np(mainThread,sizeof(cpu_set_t), &cpuset) < 0)
		cout << "Set main thread affinity error" << endl;

	if(pthread_getaffinity_np(mainThread,sizeof(cpu_set_t), &cpuset) < 0)
		cout << "Get main thread affinity error" << endl;

	string cpuSetStr = "";
	if(CPU_ISSET(0,&cpuset))
		cpuSetStr += "0 ";
	if(CPU_ISSET(1,&cpuset))
		cpuSetStr += "1";	

	cout << "PID of the main thread: " << ::getpid() << ", CPU: " << cpuSetStr << endl;

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

	// srsLTE 5.76 MHz
	int rxBufferSizeSample = 5760+6; // AD9361 IIO RX
	int txBufferSizeSample = 5760+6+1500; // AD9361 IIO TX

	// OAI 7.680 MHz	
	//int rxBufferSizeSample = 7680+6; // AD9361 IIO RX
	//int txBufferSizeSample = 7680+6+1500; // AD9361 IIO TX

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




