#include "Controller.h"

struct Controller* co = NULL;

void ControllerInit()
{
	co = (struct Controller*) malloc(sizeof(struct Controller));
	co->server = NULL;
}

void ControllerDelete()
{
	stop(RX);
	stop(TX);

	free(co);
	co = NULL;
}


void setServer(struct UDPServer* _server)
{
	co->server = _server;
}

int runCommand(char* cmdStr,int cmdLen, char* res)
{
	// Convert to lower case
	for(int i=0;i<cmdLen;i++)
		cmdStr[i] = tolower(cmdStr[i]);

	// Parse the lower case string
   	char* vstrings0 = strtok(cmdStr, " ");
   	char* vstrings1 = strtok(NULL, " ");
   	char* vstrings2 = strtok(NULL, " ");
   	char* vstrings3 = strtok(NULL, " ");

	enum iodev d;
	char* endptr = NULL;

	// Switch case on the commands
	if (strcmp(vstrings1,"rx") == 0)
  	{
		d = RX;
	}
	else if (strcmp(vstrings1,"tx") == 0)
	{
		d = TX;
	}
	else
	{
		res = "wrong RX/TX command";
		return -1;
	}

	if (strcmp(vstrings0,"set") == 0)
  	{
		if( strcmp(vstrings2,"antenna") == 0)
		{
			// configure
			if(setRFPort(vstrings3,d)<0)
			{
				res = "Could not set antenna port";
				return -1;
			}
			else
			{
				res = "done";
				return 0;
			}
		}
		else if( strcmp(vstrings2,"gain") == 0)
		{
			// convert string to long long
			long long val;
			val = strtoll(vstrings3, &endptr, 10);
			
			// configure
			if(setGain(d,val)<0)
			{
				res = "Could not set gain";
				return -1;
			}
			else
			{
				res = "done";
				return 0;
			}
		}
		else if( strcmp(vstrings2,"frequency") == 0)
		{
			// convert string to long long
			long long val;
			val = strtoll(vstrings3, &endptr, 10);
			
			// configure
			if(setLO(val,d)<0)
			{
				res = "Could not set carrier frequency";
				return -1;
			}
			else
			{
				res = "done";
				return 0;
			}
		}
		else if( strcmp(vstrings2,"samplerate") == 0)
		{
			// convert string to long long
			long long val;
			val = strtoll(vstrings3, &endptr, 10);

			// configure
			if(setFS(val,d)<0)
			{
				res = "Could not set sampling frequency";
				return -1;
			}
			else
			{
				res = "done";
				return 0;
			}
		}
		else if( strcmp(vstrings2,"bandwidth") == 0)
		{
			// convert string to long long
			long long val;
			val = strtoll(vstrings3, &endptr, 10);

			// configure
			if(setBW(val,d)<0)
			{
				res = "Could not set bandwidth";
				return -1;
			}
			else
			{
				res = "done";
				return 0;
			}
		}
		else
		{
			res = "wrong set command";
			return -1;
		}


	}
	else if (strcmp(vstrings0,"get") == 0)
	{
		if( strcmp(vstrings2,"antenna") == 0)
		{
			if(getRFPort(d,res)<0)
			{
				res = "Could not get rfport";
				return -1;
			}
			else
				return 0;
		}
		else if( strcmp(vstrings2,"gain") == 0)
		{
			long long val;
		       	if(getGain(d,&val)<0)
			{
				res = "Could not get gain";
				return -1;
			}

			sprintf(res, "%lld", val);
			return 0;
		}
		else if( strcmp(vstrings2,"frequency") == 0)
		{
			long long val;
		       	if(getLO(d,&val)<0)
			{
				res = "Could not get carrier frequency";
				return -1;
			}
			
			sprintf(res, "%lld", val);
			return 0;
		}
		else if( strcmp(vstrings2,"samplerate") == 0)
		{
			long long val;
		       	if(getFS(d,&val)<0)
			{
				res = "Could not get sampling frequency";
				return -1;
			}

			sprintf(res, "%lld", val);
			return 0;
		}
		else if( strcmp(vstrings2,"bandwidth") == 0)
		{
			long long val;
		       	if(getBW(d,&val)<0)
			{
				res = "Could not get bandwidth";
				return -1;
			}

			sprintf(res, "%lld", val);
			return 0;
		}
		else
		{
			res = "wrong get command";
			return -1;
		}
  	}
	else if (strcmp(vstrings0,"start") == 0)
	{
		// first a clean stop
		stop(d);

		// enable streaming
		enableChannels(d);

		// Start streaming thread
		switch (d)
		{
	        	case RX:
		        {
				// Start the rx streamer thread
				if(pthread_create(&co->rx_thread, NULL, &streamRX,co) != 0)
				{
				        res = "Unable to start stream RX thread";
					return -1;
				}
				else
				{
					res = "done";
					return 0;
				}

	                	break;
	        	}
		        case TX:
		        {
				// Start the tx streamer thread
				if(pthread_create(&co->tx_thread, NULL, &streamTX,co) != 0)
				{
				        res = "Unable to start stream TX thread";
					return -1;
				}
				else
				{
					res = "done";
					return 0;
				}

	                	break;
	        	}
		}

  	}
	else if (strcmp(vstrings0,"stop") == 0)
	{
		stop(d);
		res = "done";
		return 0;
  	}
	else
	{
		res = "Wrong command, not parsable";
		return -1;
	}
	return 0;
}

void* streamRX(void* controller)
{
        struct Controller* p = (struct Controller*)(controller);
	
	if(p->server == NULL)
	{
		printf("Server is not initialized yet in streamTX.\n");
		return NULL;
	}
	
	p->rx_thread_active = true;

	while(p->rx_thread_active)
        {
		// get the buffer from IIO
		char* buffer = receiveBuffer();
		if(buffer == NULL)
		{
			printf("Could not get RX buffer from dev\n");
			break;
		}

		// send it to the network
		if(sendStreamBuffer(buffer)<0)
		{
			printf("Could not send buffer to UDP server\n");
			break;
		}
	}

	// Announce
	p->rx_thread_active = false;
	printf("Controller RX thread is stopped\n");

}

void* streamTX(void* controller)
{
        struct Controller* p = (struct Controller*)(controller);

	if(p->server == NULL)
	{
		printf("Server is not initialized yet in streamTX.\n");
		return NULL;
	}

	p->tx_thread_active = true;
	
	while(p->tx_thread_active)
        {
		// get the IIO buffer pointer
		char* buffer = getTXBufferPointer();
		if(buffer == NULL)
		{
			printf("Could not get TX buffer pointer from dev\n");
			break;
		}

		// fill it from the network
		if(receiveStreamBuffer(buffer) < 0)
		{
			printf("Could not receive buffer from UDP server\n");
			break;
		}

		// push IIO buffer
		if(sendBufferFast()<0)
		{
			printf("Could not push TX buffer\n");
			break;
		}
	}

	// Announce
	p->tx_thread_active = false;
	printf("Controller TX thread is stopped\n");
}

void stop(enum iodev d)
{
	if(d==RX)
	{
		// Stop the rx streamer thread
	        co->rx_thread_active = false;
        	pthread_join(co->rx_thread, NULL);
		
		// Stop streaming
	        disableChannels(RX);
        
	}
	else if(d==TX)
	{
		// Stop the tx streamer thread
	        co->tx_thread_active = false;
        	pthread_join(co->tx_thread, NULL);
		
		// Stop streaming
	        disableChannels(TX);
	}
}
