#include <iostream>
#include <vector>
#include <string>

#include "../UDPClient.h"

#define WRITE_FILE 1

using namespace std;

void writeFile(string file_name,uint64_t* array,int size)
{
        FILE *fpOut;
        fpOut = fopen(file_name.c_str(),"w");  // w for write
        for(int i=0; i<size; i++)
        {
            fprintf(fpOut, "%llu", array[i]);
            fprintf(fpOut, "\n");
        }
        fclose(fpOut);
}

void stream(UDPClient* udpc, int num, int bufferSize)
{
	// Start 10 seconds RX
	char rxBuffer[bufferSize];
	uint64_t recvCount = 0;
	uint64_t recvPackets = 0;
	uint64_t rxIdCounter = 0;
	uint64_t rxIdOffset = 0;
	uint64_t* rxTimestamps = (uint64_t*)malloc(sizeof(uint64_t)*num);
	vector<uint64_t> rxDrops;
	double ts_to_ns = 10.0;

	do
	{
		// Receive the RX buffer
		int recvMsgSize = udpc->receiveStreamBuffer(rxBuffer);

		// Read the RX timestamp
                uint64_t* pRxTimestamp = (uint64_t*)(rxBuffer+bufferSize-8);
		uint64_t rxTimestamp = ((double)(*pRxTimestamp))*((double)ts_to_ns);

                // Read RX link stats data
                uint64_t* pRxIdCounter = (uint64_t*)(rxBuffer+bufferSize-24);
                if (rxIdCounter == 0)
                {
                	rxIdOffset = (*pRxIdCounter)-1;
                }
                uint64_t rxIdC = (*pRxIdCounter) - rxIdOffset;

		if(rxIdC - rxIdCounter < 1)
		{
			cout << "Dulicate ids:" << rxIdC << endl;
		}

                if(rxIdC - rxIdCounter > 1)
                {
                	for(uint64_t i = rxIdCounter+1; i<rxIdC; i++)
                        	rxDrops.push_back(i);
                }
                rxIdCounter = rxIdC;
                
		rxTimestamps[recvPackets] = rxTimestamp;
		recvCount += recvMsgSize;
		recvPackets++;
		
	}
	while(rxIdCounter < num);

	printf("%lu MB received in total, %lu UDP packets received\n",recvCount/1024/1024,recvPackets);
	printf("%d rx frames was dropped out of %lu\n",rxDrops.size(),rxIdCounter);

#if WRITE_FILE

	// Write the timestamps into the file
	writeFile("/tmp/Adrv_rxtimestamps.txt",rxTimestamps,num);
	// Write dropped ids into the file
	writeFile("/tmp/Adrv_droppedids.txt",&rxDrops[0],rxDrops.size());

#endif
	free(rxTimestamps);
}

int main()
{
	try
	{
		int rxBufferSize = 5760+6;
                int txBufferSize = 5760+50+6;
		UDPClient* udpc = new UDPClient(50707,50708, "10.0.9.1",rxBufferSize*4,txBufferSize*4);
		string req;
		string res;

		long long rxFreq = 2680000000;
		int rxSamplingRate = 5760000;
		int rxGain = 40;
		int rxBandwidth = 5000000;
		int frameCount = 100000;

		// LO frequency RX
		req.clear(); res.clear();
		req = "set rx frequency " + to_string(rxFreq);
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Sampling frequency RX
		req.clear(); res.clear();
		req = "set rx samplerate " + to_string(rxSamplingRate);
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Gain RX
		req.clear(); res.clear();
		req = "set rx gain " + to_string(rxGain);
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// GainMode RX
		req.clear(); res.clear();
		req = "set rx gainmode manual";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// Antenna RX
		req.clear(); res.clear();
                req = "set rx antenna A_BALANCED";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Bandwidth RX
		req.clear(); res.clear();
                req = "set rx bandwidth " + to_string(rxBandwidth);
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Buffer size RX
		udpc->setRXBufferSizeByte(rxBufferSize*4);
		cout << "set rx buffersize " << rxBufferSize*4 << endl;
		
		req.clear(); res.clear();
                req = "get rx buffersize";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Enable Channels
		req.clear(); res.clear();
                req = "start rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Stream for a number
		stream(udpc,frameCount,rxBufferSize*4);

		// Disable Channels
		req.clear(); res.clear();
                req = "stop rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
        }
	
	return 0;
}
