#include "Controller.h"


string Controller::runCommand(string cmdStr)
{
	// Parse the lower case string
	stringstream ss(cmdStr);
	istream_iterator<string> begin(ss);
	istream_iterator<string> end;
	vector<string> vstrings(begin, end);

	enum iodev d;
	char* endptr = NULL;

	string response;

	try
	{

		// Switch case on the commands
		if (vstrings[1]== "rx")
	  	{
			d = RX;
		}
		else if (vstrings[1]=="tx")
		{
			d = TX;
		}
		else
		{
			throw runtime_error("wrong RX/TX command");
		}

		if (vstrings[0]=="set")
	  	{
			if(vstrings[2]=="antenna")
			{
				struct stream_cfg conf = dev->getConfig(d);
				conf.rfport = vstrings[3];
				dev->setConfig(conf,d);

				response = "done";
			}
			else if(vstrings[2]=="gain")
			{
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);
				dev->setGain(d,val);

				response = "done";
			}
			else if(vstrings[2]=="gainmode")
			{
				dev->setGainMode(d,vstrings[3]);
				response = "done";
			}
			else if(vstrings[2]=="frequency")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.lo_hz = val;

				// configure
				dev->setConfig(conf,d);

				response = "done";

			}
			else if(vstrings[2]=="samplerate")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.fs_hz = val;

				// configure
				dev->setConfig(conf,d);

				response = "done";
			}
			else if(vstrings[2]=="bandwidth")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);

				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// set the value
				conf.bw_hz = val;

				// configure
				dev->setConfig(conf,d);

				response = "done";
			}
			else if(vstrings[2]=="buffersize")
			{
				if(d==RX)
				{
					if(rxdev != NULL)
						throw runtime_error("set buffer size while streaming rx");
				}
				if(d==TX)
				{
					if(txdev != NULL)
						throw runtime_error("set buffer size while streaming tx");
				}
					
				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);
				
				// set IIODevice buffer size
				dev->setBufferSizeSample(d,int(val/4));

				// set UDP server buffer size
				if(d==RX)
					server->setTXBufferSizeByte(val);
				if(d==TX)
					server->setRXBufferSizeByte(val);

				response = "done";
			}
			else
			{
				throw runtime_error("wrong set command");
			}

		}
		else if (vstrings[0]=="get")
		{
			if(vstrings[2]=="antenna")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				response = conf.rfport;
			}
			else if(vstrings[2]=="gain")
			{
				long long gainVal = dev->getGain(d);
				stringstream ss;
				ss << gainVal;
				response = ss.str();
			}
			else if(vstrings[2]=="gainmode")
			{
				string gainMode = dev->getGainMode(d);
				response = gainMode;
			}
			else if(vstrings[2]=="frequency")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				stringstream ss;
				ss << conf.lo_hz;
				response = ss.str();
			}
			else if(vstrings[2]=="samplerate")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				stringstream ss;
				ss << conf.fs_hz;
				response = ss.str();
			}
			else if(vstrings[2]=="bandwidth")
			{
				// get the config struct
				struct stream_cfg conf = dev->getConfig(d);
				stringstream ss;
				ss << conf.bw_hz;
				response = ss.str();
			}
			else if(vstrings[2]=="buffersize")
			{
				int val;
				// get UDP buffer size
                                if(d==RX)
                                        val = server->getRXBufferSizeByte();
                                if(d==TX)
                                        val = server->getTXBufferSizeByte();

				stringstream ss;
				ss << val;
				response = ss.str();
			}
			else
			{
				throw runtime_error("wrong get command");
			}
	  	}
		else if (vstrings[0]=="start")
		{
			// Start streaming thread
			switch (d)
			{
		        	case RX:
			        {
					// Start the rx streamer thread
			                rx_thread_active = true;
					
					// Start the rx streamer thread
					pthread_t newThread;
					if(pthread_create(&newThread, NULL, &Controller::streamRX,this) != 0)
					        throw runtime_error("Unable to start stream RX thread");
					
					rx_thread = newThread;
	
					// Get the CPU affinity of the current thread and set it to CPU 1
				        cpu_set_t cpuset;
				        CPU_ZERO(&cpuset);
				        CPU_SET(1,&cpuset);
				        if(pthread_setaffinity_np(rx_thread,sizeof(cpu_set_t), &cpuset) < 0)
				                cout << "Set RX thread affinity error" << endl;

				        if(pthread_getaffinity_np(rx_thread,sizeof(cpu_set_t), &cpuset) < 0)
				                cout << "Get RX thread affinity error" << endl;

				        string cpuSetStr = "";
				        if(CPU_ISSET(0,&cpuset))
				                cpuSetStr += "0 ";
				        if(CPU_ISSET(1,&cpuset))
				                cpuSetStr += "1";

				        //cout << "PID of the RX thread: " << ::getpid() << ", CPU: " << cpuSetStr << endl;
				        cout << "RX thread CPU: " << cpuSetStr << endl;
					
					// Set thread priority realtime SCHED_FIFO
					/*struct sched_param params;	
					params.sched_priority = sched_get_priority_max(SCHED_FIFO);					
					if(pthread_setschedparam(rx_thread,SCHED_FIFO,&params)<0)
				                cout << "Set RX thread priority realtime error" << endl;*/
					
		                	break;
		        	}
			        case TX:
			        {
					// Start the rx streamer thread
			                tx_thread_active = true;

					// Start the tx streamer thread
					pthread_t newThread;
					if(pthread_create(&newThread, NULL, &Controller::streamTX,this) != 0)
					        throw runtime_error("Unable to start stream TX thread");
					
					tx_thread = newThread;
	
					// Get the CPU affinity of the current thread and set it to CPU 0
				        cpu_set_t cpuset;
				        CPU_ZERO(&cpuset);
				        CPU_SET(0,&cpuset);
				        if(pthread_setaffinity_np(tx_thread,sizeof(cpu_set_t), &cpuset) < 0)
				                cout << "Set TX thread affinity error" << endl;

				        if(pthread_getaffinity_np(tx_thread,sizeof(cpu_set_t), &cpuset) < 0)
				                cout << "Get TX thread affinity error" << endl;

				        string cpuSetStr = "";
				        if(CPU_ISSET(0,&cpuset))
				                cpuSetStr += "0 ";
				        if(CPU_ISSET(1,&cpuset))
				                cpuSetStr += "1";

				        //cout << "PID of the TX thread: " << ::getpid() << ", CPU: " << cpuSetStr << end;
				        cout << "TX thread CPU: " << cpuSetStr << endl;
					
					// Set thread priority realtime SCHED_FIFO
					struct sched_param params;	
					params.sched_priority = sched_get_priority_max(SCHED_FIFO);					
					if(pthread_setschedparam(tx_thread,SCHED_FIFO,&params)<0)
				                cout << "Set TX thread priority realtime error" << endl;


		                	break;
		        	}
			}

			response = "done";

	  	}
		else if (vstrings[0]=="stop")
		{
			switch (d)
                        {
                                case RX:
                                {
					if(rx_thread_active)
						stop(RX);
					break;
				}
				case TX:
				{
					if(tx_thread_active)
						stop(TX);
					break;
				}
			}
			response = "done";
	  	}
		else
		{
			throw runtime_error("Wrong command, not parsable");
		}
	}
	catch(runtime_error& re)
	{
		response = string("error : ") + re.what();
	}
	
	// printout the request and the response
	cout << cmdStr << " : " << response << endl;

	return response;
}

void* Controller::streamRX(void* controller)
{
        Controller* p = static_cast<Controller*>(controller);
	
	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamRX.");
	
	p->rx_thread_active = true;
	
	// new rx dev 
	p->rxdev = new IIODevice(p->dev->getRXBufferSizeSample(),p->dev->getTXBufferSizeSample()); 
	p->rxdev->enableChannels(RX);
	try
	{
		while(p->rx_thread_active)
                {
			// get the buffer from IIO
			char* buffer = p->rxdev->receiveBuffer();
			// send it to the network
			p->server->sendStreamBuffer(buffer);
		}

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }
	
	p->rxdev->disableChannels(RX);
	delete(p->rxdev);
	p->rxdev = NULL;

	// Announce
	p->rx_thread_active = false;
	printf("Controller RX thread is stopped\n");

}

void* Controller::streamTX(void* controller)
{
        Controller* p = static_cast<Controller*>(controller);

	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamTX.");

	p->tx_thread_active = true;


	// new tx dev 
        p->txdev = new IIODevice(p->dev->getRXBufferSizeSample(),p->dev->getTXBufferSizeSample());
	p->txdev->enableChannels(TX);
	
	try
	{
		while(p->tx_thread_active)
                {
			// get the IIO buffer pointer
			char* buffer = p->txdev->getTXBufferPointer();
			// fill it from the network
			p->server->receiveStreamBuffer(buffer);
			// push IIO buffer
			p->txdev->sendBufferFast();
			
			// Debug FIXME
			//p->server->receiveStreamDiscard();
		}
	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }
	
	// delete and finish txdev
	p->txdev->disableChannels(TX);
	delete(p->txdev);
	p->txdev = NULL;

	// Announce
	p->tx_thread_active = false;
	printf("Controller TX thread is stopped\n");
}

void Controller::stop(enum iodev d)
{
	if(d==RX)
	{
		// Stop the rx streamer thread
	        rx_thread_active = false;
		
		// Join thread
        	pthread_cancel(rx_thread);
        	pthread_join(rx_thread, NULL);
	
		// Delete rxdev if exists	
		if(rxdev != NULL)
		{
		        rxdev->disableChannels(RX);
			delete(rxdev);
			rxdev = NULL;
		}	
	}
	else if(d==TX)
	{
		// Stop the rx streamer thread
	        tx_thread_active = false;
		
		// Force stop the tx streamer thread
        	pthread_cancel(tx_thread);
        	pthread_join(tx_thread, NULL);
		
		// Delete txdev if exists
		if(txdev != NULL)
		{
		        txdev->disableChannels(TX);
			delete(txdev);
			txdev = NULL;
		}
	}
}
