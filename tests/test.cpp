#include <iostream>
#include <vector>
#include <string>

#include "UDPClient.h"

/*#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Time.h>
#include <SoapySDR/Logger.h>
#include <Types.h>*/

using namespace std;

#define WRITE_FILE 0

void writeFileRx(string file_name,uint64_t* rxtsp,int size)
{
	FILE *write_ptr;
	write_ptr = fopen(file_name.c_str(),"wb");  // w for write, b for binary
	fwrite(rxtsp,sizeof(uint64_t)*size,1,write_ptr);
	fclose(write_ptr);
}

void writeFileTxDif(string file_name, int64_t* txtsp,int size)
{
	FILE *write_ptr;
	write_ptr = fopen(file_name.c_str(),"wb");  // w for write, b for binary
	fwrite(txtsp,sizeof(int64_t)*size,1,write_ptr);
	fclose(write_ptr);
}

void stream(UDPClient* udpc, int num)
{
	// Start 10 seconds RX/TX
	int64_t dif_timestamp = 0;
	int64_t tx_dif_timestamp = 0;
	uint64_t rx_timestamp = 0;
	int size = udpc->getRXBufferSizeByte();
	char Buffer[size];
	int recv_count = 0;
	int sent_count = 0;
	int recv_packets = 0;
	int sent_packets = 0;
	int failed_packets = 0;
	int tsize = num;
	//uint64_t* rx_timestamps = new (sizeof(uint64_t)*tsize);
	//int64_t* txdif_timestamps = new (sizeof(int64_t)*tsize);
	uint64_t* rx_timestamps = (uint64_t*)malloc(sizeof(uint64_t)*tsize);
	int64_t* txdif_timestamps = (int64_t*)malloc(sizeof(int64_t)*tsize);
	while(true)
	{
		// Receive the RX buffer
		int recvMsgSize = udpc->receiveStreamBuffer(Buffer);
		recv_count += recvMsgSize;
		recv_packets++;

		// Read the RX timestamp and tx_dif_timestamp
		uint64_t* rx_timestamp_pointer = (uint64_t*)(Buffer+size-8);
		uint64_t* tx_dif_timestamp_pointer = (uint64_t*)(Buffer+size-16);
		dif_timestamp = *rx_timestamp_pointer - rx_timestamp;
		tx_dif_timestamp = *tx_dif_timestamp_pointer;
		rx_timestamp = *rx_timestamp_pointer;
		//printf("RX timestamp: %llu, dif: %llu, tx_dif %lld\n",*rx_timestamp_pointer,dif_timestamp,tx_dif_timestamp);

		// Save the timestamps into the struct
		rx_timestamps[sent_packets % tsize] = rx_timestamp;
		txdif_timestamps[sent_packets % tsize] = tx_dif_timestamp;

		// Schedule TX buffer by TX timestamp
		uint64_t* tx_timestamp_pointer = (uint64_t*)(Buffer+size-8);
		uint64_t old_val = *tx_timestamp_pointer;
		*tx_timestamp_pointer = rx_timestamp;
		//printf("TX timestamp: %llu, old_val: %llu\n",*tx_timestamp_pointer,old_val);
		//memset(&Buffer,0,sizeof(Buffer) );

		// Send the TX Buffer
		int sendMsgSize;
	       	if((sendMsgSize = udpc->sendStreamBuffer(Buffer))>0)
		{
	        	sent_count+=sendMsgSize;
			sent_packets++;

		}
		else
			failed_packets++;

		if(sent_packets == num)
			break;
	}
	printf("UDP bytes received %d MB in total, %d packets\n",recv_count/1024/1024,recv_packets);
	printf("UDP bytes sent %d MB in total, %d packets, %d failed packets\n",sent_count/1024/1024,sent_packets,failed_packets);

#if WRITE_FILE

	// Write the timestamps into the file
	writeFileRx("rx_timestamps_udp.dat",rx_timestamps,tsize);
	writeFileTxDif("txdif_timestamps_udp.dat",txdif_timestamps,tsize);

#endif
	free(rx_timestamps);
	free(txdif_timestamps);
}

int main()
{
	try
	{
		UDPClient* udpc = new UDPClient(50707,50708, "10.0.9.1",15*1024*4,15*1024*4);
		string req;
		string res;
		
		// LO frequency TX
		req.clear(); res.clear();
		req = "get tx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx frequency 4000000000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "get tx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// LO frequency RX
		req.clear(); res.clear();
		req = "get rx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "set rx frequency 300000000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "get rx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// Sampling frequency RX
		req.clear(); res.clear();
		req = "get rx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx samplerate 5760000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Sampling frequency TX
		req.clear(); res.clear();
		req = "get tx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx samplerate 5760000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get tx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Gain RX
		req.clear(); res.clear();
		req = "get rx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx gain 40";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// Gain TX
		req.clear(); res.clear();
		req = "get tx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx gain 20";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get tx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// GainMode RX
		req.clear(); res.clear();
		req = "get rx gainmode";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx gainmode manual";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx gainmode";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Antenna RX
		req.clear(); res.clear();
                req = "get rx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set rx antenna B_BALANCED";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get rx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

                // Antenna TX
		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set tx antenna B";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Bandwidth RX
		req.clear(); res.clear();
                req = "get rx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set rx bandwidth 2200000";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get rx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Bandwidth TX
		req.clear(); res.clear();
                req = "get tx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set tx bandwidth 3100000";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Enable Channels
		req.clear(); res.clear();
                req = "start rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "start tx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Stream for a number
		stream(udpc,10000);

		// Disable Channels
		req.clear(); res.clear();
                req = "stop rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "stop tx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Buffer size RX
		req.clear(); res.clear();
                req = "get rx buffersize";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		udpc->setRXBufferSizeByte(8*1024*4);
		cout << "set rx buffersize 8*1024*4" << endl;
		
		req.clear(); res.clear();
                req = "get rx buffersize";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Buffer size TX
		req.clear(); res.clear();
                req = "get tx buffersize";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		udpc->setTXBufferSizeByte(8*1024*4);
                cout << "Set tx buffersize 8*1024*4 " << endl;

		req.clear(); res.clear();
                req = "get tx buffersize";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Enable Channels
		req.clear(); res.clear();
                req = "start rx ";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "start tx ";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Stream for a number
		stream(udpc,10000);

		// Disable Channels
		req.clear(); res.clear();
                req = "stop rx ";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "stop tx ";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
        }
	




	return 0;
}
