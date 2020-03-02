#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <iterator>

#include "IIODevice.h"
#include "UDPServer.h"

using namespace std;

class UDPServer;

class Controller
{
        public:
                Controller(IIODevice* _dev) : dev(_dev),server(NULL),rxdev(NULL),txdev(NULL) {}
                ~Controller() { stop(RX); stop(TX); }
                string runCommand(string cmdStr);
		void stop(enum iodev d);
		void setServer(UDPServer* _server) { server = _server; }
        private:
                IIODevice* dev;
                IIODevice* rxdev;
                IIODevice* txdev;
		UDPServer* server;
		bool rx_thread_active;
		bool tx_thread_active;
		pthread_t rx_thread;
		pthread_t tx_thread;
		static void* streamRX(void* cont);
		static void* streamTX(void* cont);
};

#endif

