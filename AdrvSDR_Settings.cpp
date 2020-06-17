#include "SoapyAdrvSDR.hpp"

SoapyAdrvSDR::SoapyAdrvSDR( const SoapySDR::Kwargs &args ): rx_stream(nullptr), tx_stream(nullptr)
{
	if (args.count("label") != 0)
		SoapySDR_logf(SOAPY_SDR_INFO, "Opening %s...", args.at("label").c_str());

	try
	{
		udpc = UDPClient::findServer(stoi(args.at("cmdport")),stoi(args.at("strport")),args.at("hostname"),stoi(args.at("rxbuffersize"))*4,stoi(args.at("txbuffersize"))*4);
	}
	catch(runtime_error& re)
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, re.what());
		throw re;
	}
	
	phandler = (pluto_handler_t*) malloc(sizeof(pluto_handler_t));
  	bzero(phandler, sizeof(pluto_handler_t));

}

SoapyAdrvSDR::~SoapyAdrvSDR(void)
{
	if(udpc)
	{
		delete(udpc);
		udpc = nullptr;
	}

	if(phandler)
	{
		free(phandler);
		phandler = NULL;
	}
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyAdrvSDR::getDriverKey( void ) const
{
	return "UDP";
}

std::string SoapyAdrvSDR::getHardwareKey( void ) const
{
	return "ADRV9361-UDP";
}

SoapySDR::Kwargs SoapyAdrvSDR::getHardwareInfo( void ) const
{
	SoapySDR::Kwargs info;
	return info;
}


/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyAdrvSDR::getNumChannels( const int dir ) const
{
	return 1;
}

bool SoapyAdrvSDR::getFullDuplex( const int direction, const size_t channel ) const
{
	return false;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyAdrvSDR::getSettingInfo(void) const
{
	SoapySDR::ArgInfoList setArgs;
	return setArgs;
}

void SoapyAdrvSDR::writeSetting(const std::string &key, const std::string &value)
{
	return;
}


std::string SoapyAdrvSDR::readSetting(const std::string &key) const
{
	std::string info;
	return info;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyAdrvSDR::listAntennas( const int direction, const size_t channel ) const
{
	std::vector<std::string> options;
	if(direction == SOAPY_SDR_RX)
	{
		options.push_back( "A_BALANCED" );
		options.push_back( "B_BALANCED" );
	}
	if(direction == SOAPY_SDR_TX) 
	{
		options.push_back( "A" );
		options.push_back( "B" );
	}
	return(options);
}

void SoapyAdrvSDR::setAntenna( const int direction, const size_t channel, const string &name )
{
	string res;

	if (direction == SOAPY_SDR_RX) 
	{
		std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

		string req = string("set rx antenna ") + name;
                res = udpc->sendCommand(req);
	}
	else if (direction == SOAPY_SDR_TX) 
	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

		string req = string("set tx antenna ") + name;
                res = udpc->sendCommand(req);
	}
	
	if(res.find("error") != string::npos)
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
	}
}


std::string SoapyAdrvSDR::getAntenna( const int direction, const size_t channel ) const
{
	string res;

	if (direction == SOAPY_SDR_RX) 
	{
		string req = "get rx antenna";
                res = udpc->sendCommand(req);
	}
	else if (direction == SOAPY_SDR_TX) 
	{
		string req = "get tx antenna";
                res = udpc->sendCommand(req);
	}

	if(res.find("error") != string::npos)
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
	}

	return res;
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyAdrvSDR::hasDCOffsetMode( const int direction, const size_t channel ) const
{
	return true;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyAdrvSDR::listGains( const int direction, const size_t channel ) const
{
	std::vector<std::string> options;
	options.push_back("PGA");
	return options;
}

bool SoapyAdrvSDR::hasGainMode(const int direction, const size_t channel) const
{
	if (direction == SOAPY_SDR_RX)
		return true;
	return false;
}

void SoapyAdrvSDR::setGainMode( const int direction, const size_t channel, const bool automatic )
{
	string res;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if (automatic)
		{
			string req = "set rx gainmode slow_attack";
	                res = udpc->sendCommand(req);
		}
		else
		{
			string req = "set rx gainmode manual";
	                res = udpc->sendCommand(req);
		}
	}
	
	if(res.find("error") != string::npos)
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
	}

}

bool SoapyAdrvSDR::getGainMode(const int direction, const size_t channel) const
{
	string res;

	if (direction == SOAPY_SDR_RX)
        {
                string req = "get rx gainmode";
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                string req = "get tx gainmode";
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

	if(res != "manual")
		return true;
	else
		return false;
}

void SoapyAdrvSDR::setGain( const int direction, const size_t channel, const double value )
{
	//return;

	string res;

        if (direction == SOAPY_SDR_RX)
        {

                std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

                string req = string("set rx gain ") + to_string(value);
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

                string req = string("set tx gain ") + to_string(value-89);
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }
}

void SoapyAdrvSDR::setGain( const int direction, const size_t channel, const std::string &name, const double value )
{
	this->setGain(direction,channel,value);
}

double SoapyAdrvSDR::getGain( const int direction, const size_t channel, const std::string &name ) const
{
	//return 50;

	string res;
	double gain;

        if (direction == SOAPY_SDR_RX)
        {
                string req = "get rx gain";
                res = udpc->sendCommand(req);
        	if(res.find("error") != string::npos)
	        {
        	        SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                	throw runtime_error(res);
	        }
	        gain = double(stoi(res));
        }
        else if (direction == SOAPY_SDR_TX)
        {
                string req = "get tx gain";
                res = udpc->sendCommand(req);
        	if(res.find("error") != string::npos)
	        {
        	        SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                	throw runtime_error(res);
	        }
	        gain = double(stoi(res)+89);
        }

	return gain;
}

SoapySDR::Range SoapyAdrvSDR::getGainRange( const int direction, const size_t channel, const std::string &name ) const
{
	if(direction==SOAPY_SDR_RX)
		return(SoapySDR::Range(0, 73));
	return(SoapySDR::Range(0,89));

}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyAdrvSDR::setFrequency( const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args )
{
	string res;

        if (direction == SOAPY_SDR_RX)
        {
		// manual CFO compensation RX
		// double frequencyn = frequency;
		double frequencyn = frequency;

                std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

                string req = string("set rx frequency ") + to_string((long long)frequencyn);
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
		// manual CFO compensation TX
		//double frequencyn = frequency + 13000;
		double frequencyn = frequency;
                
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

                string req = string("set tx frequency ") + to_string((long long)frequencyn);
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }
}

double SoapyAdrvSDR::getFrequency( const int direction, const size_t channel, const std::string &name ) const
{
	string res;
        if (direction == SOAPY_SDR_RX)
        {
                string req = "get rx frequency";
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                string req = "get tx frequency";
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

        return double(stoll(res));
}

SoapySDR::ArgInfoList SoapyAdrvSDR::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList freqArgs;
	return freqArgs;
}

std::vector<std::string> SoapyAdrvSDR::listFrequencies( const int direction, const size_t channel ) const
{
	std::vector<std::string> names;
	names.push_back( "RF" );
	return(names);
}

SoapySDR::RangeList SoapyAdrvSDR::getFrequencyRange( const int direction, const size_t channel, const std::string &name ) const
{
	return(SoapySDR::RangeList( 1, SoapySDR::Range( 70000000, 6000000000ull ) ) );
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/
void SoapyAdrvSDR::setSampleRate( const int direction, const size_t channel, const double samplerate )
{
	// manual SFO compensation
	//double samplerateN = samplerate + 34;
	double samplerateN = samplerate;
	
	
	string res;
	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if (samplerate<(25e6 / 48))
		{
			if (samplerate * 8 < (25e6 / 48)) 
				SoapySDR_logf(SOAPY_SDR_ERROR, "sample rate is not supported.");
		}

		string req = string("set rx samplerate ") + to_string((long long)samplerate);
                res = udpc->sendCommand(req);

		if(res.find("error") != string::npos)
                {
                        SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                        throw runtime_error(res);
                }
		
		phandler->rxSamplingFrequency = samplerate;
		
		// FIXME
		// if(rx_stream)
		//	rx_stream->set_buffer_size_by_samplerate(samplerate);
		
		printf("[SoapyAdrv][setSampleRate] RX SamplingRate set to  %llu \n",samplerate);
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		if (samplerate<(25e6 / 48)) 
		{
			if (samplerate * 8 < (25e6 / 48)) 
			{
				SoapySDR_logf(SOAPY_SDR_ERROR, "sample rate is not supported.");
			}
		}
		 
		
		string req = string("set tx samplerate ") + to_string((long long)samplerateN);
                res = udpc->sendCommand(req);

                if(res.find("error") != string::npos)
                {
                        SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                        throw runtime_error(res);
                }

                phandler->txSamplingFrequency = samplerate;
		
		// FIXME
		// if(tx_stream)
		//	tx_stream->set_buffer_size_by_samplerate(samplerate);
		
		printf("[SoapyAdrv][setSampleRate] TX SamplingRate set to  %llu \n",samplerate);
	}

	// FIXME
	// if(ad9361_set_bb_rate(dev,(unsigned long)samplerate))
	//	SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to set BB rate.");	
}

double SoapyAdrvSDR::getSampleRate( const int direction, const size_t channel ) const
{
	string res;
	if (direction == SOAPY_SDR_RX)
        {
                string req = "get rx samplerate";
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                string req = "get tx samplerate";
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

        return double(stoll(res));
}

std::vector<double> SoapyAdrvSDR::listSampleRates( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	return(options);
}

/*******************************************************************
 * Bandwidth API
 ******************************************************************/

void SoapyAdrvSDR::setBandwidth( const int direction, const size_t channel, const double bw )
{
	string res;

        if (direction == SOAPY_SDR_RX)
        {
                std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

                string req = string("set rx bandwidth ") + to_string((long long)bw);
                res = udpc->sendCommand(req);
		printf("[SoapyADRV][setBandwidth] RX bandwidth set to  %llu \n",(long long)bw);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

                string req = string("set tx bandwidth ") + to_string((long long)bw);
                res = udpc->sendCommand(req);
		printf("[SoapyADRV][setBandwidth] TX bandwidth set to  %llu \n",(long long)bw);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

}

double SoapyAdrvSDR::getBandwidth( const int direction, const size_t channel ) const
{
	string res;
	if (direction == SOAPY_SDR_RX)
        {
                string req = "get rx bandwidth";
                res = udpc->sendCommand(req);
        }
        else if (direction == SOAPY_SDR_TX)
        {
                string req = "get tx bandwidth";
                res = udpc->sendCommand(req);
        }

        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

        return double(stoll(res));
}

std::vector<double> SoapyAdrvSDR::listBandwidths( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	return(options);
}
