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

	cout << "cmd: " << cmdStr << endl;

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
				// convert string to long long
				long long val;
				val = strtoll(vstrings[3].c_str(), &endptr, 10);

				// configure
				dev->setSamplingFrequency(d,val);

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
				// HERE TX is RX and RX is TX
				if(d==RX)
					server->setTXBufferSizeByte(val);
				if(d==TX)
					server->setRXBufferSizeByte(val);

				response = "done";
			}
			else if(vstrings[2]=="timeroffset")
                        {
                                // convert string to int64_t
                                int64_t val;
				sscanf(vstrings[3].c_str(),"%lld",&val);

				// set timer offset
				dev->setTimerOffset(val);	

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
				// get the sf
				long long sf = dev->getSamplingFrequency(d);
				stringstream ss;
				ss << sf;
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
			else if(vstrings[2]=="hwtimestamp")
                        {
                                // get the hardware timestamp
                                uint64_t ts = dev->getHWTimestamp();
                                stringstream ss;
                                ss << ts;
                                response = ss.str();
                        }
			else if(vstrings[2]=="buffersize")
			{
				int val;
				// get UDP buffer size
				// HERE TX is RX and RX is TX
                                if(d==RX)
                                        val = server->getTXBufferSizeByte();
                                if(d==TX)
                                        val = server->getRXBufferSizeByte();

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
			start(d);
			response = "done";

	  	}
		else if (vstrings[0]=="stop")
		{
			// Stop streaming thread
			stop(d);
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
	cout << "res: " << response << endl;

	return response;
}

void* Controller::streamRX(void* controller)
{
	int ret = 0;

	// Get current thread pointer
	pthread_t this_thread = pthread_self();
					
	// Get the CPU affinity of the current thread and set it to CPU 1
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	ret = pthread_setaffinity_np(this_thread,sizeof(cpu_set_t), &cpuset);
	if(ret != 0)
		cout << "RX thread set CPU affinity error ret: " << ret <<  endl;

	ret = pthread_getaffinity_np(this_thread,sizeof(cpu_set_t), &cpuset);
	if(ret != 0)
		cout << "RX thread get CPU affinity error ret: " << ret << endl;

	string cpuSetStr = "";
	if(CPU_ISSET(0,&cpuset))
		cpuSetStr += "0 ";

	if(CPU_ISSET(1,&cpuset))
		cpuSetStr += "1";

	//cout << "PID of the RX thread: " << ::getpid() << ", CPU: " << cpuSetStr << endl;
	cout << "RX thread is running on CPU: " << cpuSetStr << endl;
	
	// Set thread priority realtime SCHED_FIFO
	struct sched_param params;
	
	// Processes with numerically higher priority values are scheduled before processes with numerically lower priority values 
	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ret = pthread_setschedparam(this_thread,SCHED_FIFO,&params);
	if(ret != 0)
		cout << "RX thread set priority realtime error, ret: " << ret << endl;

	// Now verify the change in thread priority
	int policy = 0;
     	if(pthread_getschedparam(this_thread, &policy, &params) != 0)
        	cout << "RX thread couldn't retrieve real-time scheduling paramers" << endl;
 
     	// Check the correct policy was applied
     	if(policy != SCHED_FIFO) 
        	cout << "RX thread scheduling is NOT SCHED_FIFO!" << endl;
	else 
        	cout << "RX thread SCHED_FIFO OK" << endl;
 
	// Print thread scheduling priority
	cout << "RX Thread priority is " << params.sched_priority << endl;

        Controller* p = static_cast<Controller*>(controller);
	
	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamRX.");
	
	p->rx_thread_active = true;

	// IIO inits
	int nbytes_rx = 0;
	char* buffer = NULL;
	struct iio_buffer* rxbuf = p->dev->enableChannels(RX);
	struct iio_channel* rx0_i = p->dev->getRx0_i();	

	// Server inits
	int streamSocket = p->server->getStreamSocket();
	int txBufferSizeByte = p->server->getTXBufferSizeByte();
	struct sockaddr_in clntSTRAddr = p->server->getClientSTRAddr();

	try
	{
		while(p->rx_thread_active)
                {
                	// Receive a buffer from IIO
		        nbytes_rx = iio_buffer_refill(rxbuf);
                	if (nbytes_rx < 0)
		        	throw runtime_error("Error refilling buf from IIO ad9361");
	
			// Get the pointer from IIO
			buffer = (char*)iio_buffer_first(rxbuf,rx0_i);

			// Send it to the network
			int ret = sendto(streamSocket, buffer, txBufferSizeByte, 0, (struct sockaddr*) &(clntSTRAddr), sizeof(clntSTRAddr));
        		if(ret < 0)
                		throw runtime_error("Server sending the buffer failed");
		}

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }
	p->dev->disableChannels(RX);

	// Announce
	p->rx_thread_active = false;
	printf("Controller RX thread is stopped\n");

}

void* Controller::streamTX(void* controller)
{
	int ret = 0;

        // Get current thread pointer
        pthread_t this_thread = pthread_self();

        // Get the CPU affinity of the current thread and set it to CPU 0
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(1,&cpuset);
        ret = pthread_setaffinity_np(this_thread,sizeof(cpu_set_t), &cpuset);
        if(ret != 0)
                cout << "TX thread set CPU affinity error ret: " << ret <<  endl;

        ret = pthread_getaffinity_np(this_thread,sizeof(cpu_set_t), &cpuset);
        if(ret != 0)
                cout << "TX thread get CPU affinity error ret: " << ret << endl;

        string cpuSetStr = "";
        if(CPU_ISSET(0,&cpuset))
                cpuSetStr += "0 ";

        if(CPU_ISSET(1,&cpuset))
                cpuSetStr += "1";

        //cout << "PID of the TX thread: " << ::getpid() << ", CPU: " << cpuSetStr << endl;
        cout << "TX thread is running on CPU: " << cpuSetStr << endl;

        // Set thread priority realtime SCHED_FIFO
        struct sched_param params;

	// Processes with numerically higher priority values are scheduled before processes with numerically lower priority values 
        params.sched_priority = sched_get_priority_max(SCHED_FIFO);
        ret = pthread_setschedparam(this_thread,SCHED_FIFO,&params);
        if(ret != 0)
                cout << "TX thread set priority realtime error, ret: " << ret << endl;

        // Now verify the change in thread priority
        int policy = 0;
        if(pthread_getschedparam(this_thread, &policy, &params) != 0)
                cout << "TX thread couldn't retrieve real-time scheduling paramers" << endl;

        // Check the correct policy was applied
        if(policy != SCHED_FIFO)
                cout << "TX thread scheduling is NOT SCHED_FIFO!" << endl;
        else
                cout << "TX thread SCHED_FIFO OK" << endl;

        // Print thread scheduling priority
        cout << "TX Thread priority is " << params.sched_priority << endl;


        Controller* p = static_cast<Controller*>(controller);

	if(p->server == NULL)
		throw runtime_error("Server is not initialized yet in streamTX.");

	p->tx_thread_active = true;
	
	// IIO inits
	char* buffer;
        struct iio_buffer* txbuf = p->dev->enableChannels(TX);
	struct iio_channel* tx0_i = p->dev->getTx0_i();

        // Server inits
	ret = 0;
	int rxBufferSizeByte = p->server->getRXBufferSizeByte();
	int streamSocket = p->server->getStreamSocket();
	int nbytes_tx = 0;

	try
	{
		while(p->tx_thread_active)
                {
			// Get IIO buffer pointer, copy the data
	                buffer = (char*) iio_buffer_first(txbuf,tx0_i);

			ret = recv(streamSocket, buffer, rxBufferSizeByte, 0);
        		if((ret < 0) || (ret != rxBufferSizeByte))
		                throw runtime_error("Receiving the buffer from server faild");
			
			// Push the buffer only
	                nbytes_tx = iio_buffer_push(txbuf);
        	        if (nbytes_tx < 0)
                	        throw runtime_error("Error pushing the buffer to IIO ad9361");
		}
	}
	catch(runtime_error& re)
        {
                cout << "Runtime error: " << re.what() << endl;
        }
	p->dev->disableChannels(TX);

	// Announce
	p->tx_thread_active = false;
	printf("Controller TX thread is stopped\n");
}

void Controller::start(enum iodev d)
{
	if(d==RX)
	{
		// Check if RX is active
                if(rx_thread_active)
                	return;
		
		// Start the rx streamer thread
                rx_thread_active = true;

                // Start the rx streamer thread
                pthread_t newThread;
                if(pthread_create(&newThread, NULL, &Controller::streamRX,this) != 0)
                     	throw runtime_error("Unable to start stream RX thread");

		rx_thread = newThread;
	}
	else if(d==TX)
	{
		// Check if TX is active
                if(tx_thread_active)
                	return;
		
		// Start the rx streamer thread
                tx_thread_active = true;

                // Start the tx streamer thread
                pthread_t newThread;
                if(pthread_create(&newThread, NULL, &Controller::streamTX,this) != 0)
                       	throw runtime_error("Unable to start stream TX thread");

                tx_thread = newThread;
	}
}

void Controller::stop(enum iodev d)
{
	if(d==RX)
	{
		if(rx_thread_active)
		{
			// Force stop the rx streamer thread
        		pthread_cancel(rx_thread);
		        pthread_join(rx_thread, NULL);

			// Stop the rx streamer thread
			rx_thread_active = false;
		}
		dev->disableChannels(RX);
	}
	else if(d==TX)
	{
		if(tx_thread_active)
		{	
			// Force stop the tx streamer thread
        		pthread_cancel(tx_thread);
		        pthread_join(tx_thread, NULL);
		
			// Stop the rx streamer thread
			tx_thread_active = false;
		}
		dev->disableChannels(TX);
	}
}

