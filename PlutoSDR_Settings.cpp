#include "SoapyPlutoSDR.hpp"

SoapyPlutoSDR::SoapyPlutoSDR( const SoapySDR::Kwargs &args ):
	dev(nullptr), rx_dev(nullptr),tx_dev(nullptr), decimation(false), interpolation(false), rx_stream(nullptr)
{
	gainMode = false;

	if (args.count("label") != 0)
		SoapySDR_logf( SOAPY_SDR_INFO, "Opening %s...", args.at("label").c_str());

	if (ctx == nullptr) 
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, "not device found.");
		throw std::runtime_error("not device found");
	}

	phandler = (pluto_handler_t*) malloc(sizeof(pluto_handler_t));
  	bzero(phandler, sizeof(pluto_handler_t));
}

SoapyPlutoSDR::~SoapyPlutoSDR(void)
{
	if(phandler)
	{
		free(phandler);
		phandler = NULL;
	}
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyPlutoSDR::getDriverKey( void ) const
{
	return "UDP";
}

std::string SoapyPlutoSDR::getHardwareKey( void ) const
{
	return "ADRV9361-UDP";
}

SoapySDR::Kwargs SoapyPlutoSDR::getHardwareInfo( void ) const
{
	SoapySDR::Kwargs info;
	return info;
}


/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyPlutoSDR::getNumChannels( const int dir ) const
{
	return 1;
}

bool SoapyPlutoSDR::getFullDuplex( const int direction, const size_t channel ) const
{
	return true;
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyPlutoSDR::getSettingInfo(void) const
{
	SoapySDR::ArgInfoList setArgs;
	return setArgs;
}

void SoapyPlutoSDR::writeSetting(const std::string &key, const std::string &value)
{
	return;
}


std::string SoapyPlutoSDR::readSetting(const std::string &key) const
{
	std::string info;
	return info;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyPlutoSDR::listAntennas( const int direction, const size_t channel ) const
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

void SoapyPlutoSDR::setAntenna( const int direction, const size_t channel, const std::string &name )
{
	return;
	
	if (direction == SOAPY_SDR_RX) 
	{
		std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		iio_channel_attr_write(iio_device_find_channel(dev, "voltage0", false), "rf_port_select", name.c_str());
	}
	else if (direction == SOAPY_SDR_TX) 
	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		iio_channel_attr_write(iio_device_find_channel(dev, "voltage0", true), "rf_port_select", name.c_str());
	} 
}


std::string SoapyPlutoSDR::getAntenna( const int direction, const size_t channel ) const
{
	std::string options;

	if (direction == SOAPY_SDR_RX) 
	{
		options = "A_BALANCED";
	}
	else if (direction == SOAPY_SDR_TX) 
	{
		options = "A";
	}
	return options;
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapyPlutoSDR::hasDCOffsetMode( const int direction, const size_t channel ) const
{
	return true;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyPlutoSDR::listGains( const int direction, const size_t channel ) const
{
	std::vector<std::string> options;
	options.push_back("PGA");
	return options;
}

bool SoapyPlutoSDR::hasGainMode(const int direction, const size_t channel) const
{
	if (direction == SOAPY_SDR_RX)
		return true;
	return false;
}

void SoapyPlutoSDR::setGainMode( const int direction, const size_t channel, const bool automatic )
{
	return;
	
	gainMode = automatic;
	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if (gainMode) 
		{
			iio_channel_attr_write(iio_device_find_channel(dev, "voltage0", false), "gain_control_mode", "slow_attack");
		}
		else
		{
			iio_channel_attr_write(iio_device_find_channel(dev, "voltage0", false), "gain_control_mode", "manual");
		}
	}
}

bool SoapyPlutoSDR::getGainMode(const int direction, const size_t channel) const
{
	return gainMode;
}

void SoapyPlutoSDR::setGain( const int direction, const size_t channel, const double value )
{
	return;
	long long gain = (long long) value;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", false),"hardwaregain", gain);
		//printf("[SoapyPluto][setGain] RX gain set to %d \n",gain);
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		gain = gain - 89;
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", true),"hardwaregain", gain);
		//printf("[SoapyPluto][setGain] TX gain (attenuation) set to %d \n",gain);
	}
}

void SoapyPlutoSDR::setGain( const int direction, const size_t channel, const std::string &name, const double value )
{
	return;
	
	this->setGain(direction,channel,value);
}

double SoapyPlutoSDR::getGain( const int direction, const size_t channel, const std::string &name ) const
{
	long long gain = 0;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", false),"hardwaregain",&gain )!=0)
			return 0;
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", true),"hardwaregain",&gain )!=0)
			return 0;
		gain = gain + 89;
	}
	return double(gain);
}

SoapySDR::Range SoapyPlutoSDR::getGainRange( const int direction, const size_t channel, const std::string &name ) const
{
	if(direction==SOAPY_SDR_RX)
		return(SoapySDR::Range(0, 73));
	return(SoapySDR::Range(0,89));

}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyPlutoSDR::setFrequency( const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args )
{
	return;
	
	long long freq = (long long)frequency;
	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "altvoltage0", true),"frequency", freq);
		printf("[SoapyPluto][setFrequency] RX frequency set to  %llu \n",freq);
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "altvoltage1", true),"frequency", freq);
		printf("[SoapyPluto][setFrequency] TX frequency set to  %llu \n",freq);
	}

}

double SoapyPlutoSDR::getFrequency( const int direction, const size_t channel, const std::string &name ) const
{
  	long long freq;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "altvoltage0", true),"frequency",&freq )!=0)
			return 0;
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "altvoltage1", true),"frequency",&freq )!=0)
			return 0;
	}

	return double(freq);
}

SoapySDR::ArgInfoList SoapyPlutoSDR::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList freqArgs;
	return freqArgs;
}

std::vector<std::string> SoapyPlutoSDR::listFrequencies( const int direction, const size_t channel ) const
{
	std::vector<std::string> names;
	names.push_back( "RF" );
	return(names);
}

SoapySDR::RangeList SoapyPlutoSDR::getFrequencyRange( const int direction, const size_t channel, const std::string &name ) const
{
	return(SoapySDR::RangeList( 1, SoapySDR::Range( 70000000, 6000000000ull ) ) );

}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/
void SoapyPlutoSDR::setSampleRate( const int direction, const size_t channel, const double rate )
{
	return;

	long long samplerate =(long long) rate;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		decimation = false;
		if (samplerate<(25e6 / 48)) 
		{
			if (samplerate * 8 < (25e6 / 48)) 
			{
				SoapySDR_logf(SOAPY_SDR_CRITICAL, "sample rate is not supported.");
			}
			decimation = true;
			samplerate = samplerate * 8;
		}

		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", false),"sampling_frequency", samplerate);
		iio_channel_attr_write_longlong(iio_device_find_channel(rx_dev, "voltage0", false), "sampling_frequency", decimation?samplerate/8:samplerate);
		if(rx_stream)
			rx_stream->set_buffer_size_by_samplerate(decimation ? samplerate / 8 : samplerate);
		
		phandler->rxSamplingFrequency = samplerate;
		printf("[SoapyPluto][setSampleRate] RX SamplingRate set to  %llu \n",samplerate);
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		interpolation = false;
		if (samplerate<(25e6 / 48)) 
		{
			if (samplerate * 8 < (25e6 / 48)) 
			{
				SoapySDR_logf(SOAPY_SDR_CRITICAL, "sample rate is not supported.");
			}

			interpolation = true;
			samplerate = samplerate * 8;
		}

		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", true),"sampling_frequency", samplerate);
		iio_channel_attr_write_longlong(iio_device_find_channel(tx_dev, "voltage0", true), "sampling_frequency", interpolation?samplerate / 8:samplerate);
		
		phandler->txSamplingFrequency = samplerate;
		printf("[SoapyPluto][setSampleRate] TX SamplingRate set to  %llu \n",samplerate);
	}

	if(ad9361_set_bb_rate(dev,(unsigned long)samplerate))
		SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to set BB rate.");	
}

double SoapyPlutoSDR::getSampleRate( const int direction, const size_t channel ) const
{
	long long samplerate;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

		if(iio_channel_attr_read_longlong(iio_device_find_channel(rx_dev, "voltage0", false),"sampling_frequency",&samplerate )!=0)
			return 0;
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

		if(iio_channel_attr_read_longlong(iio_device_find_channel(tx_dev, "voltage0", true),"sampling_frequency",&samplerate)!=0)
			return 0;

	}

	return double(samplerate);

}

std::vector<double> SoapyPlutoSDR::listSampleRates( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	return(options);
}

void SoapyPlutoSDR::setBandwidth( const int direction, const size_t channel, const double bw )
{
	return;
	
	long long bandwidth = (long long) bw;
	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", false),"rf_bandwidth", bandwidth);
		
		printf("[SoapyPluto][setBandwidth] RX bandwidth set to  %llu \n",bandwidth);
	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		iio_channel_attr_write_longlong(iio_device_find_channel(dev, "voltage0", true),"rf_bandwidth", bandwidth);
		
		printf("[SoapyPluto][setBandwidth] TX bandwidth set to  %llu \n",bandwidth);
	}

}

double SoapyPlutoSDR::getBandwidth( const int direction, const size_t channel ) const
{
	long long bandwidth;

	if(direction==SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", false),"rf_bandwidth",&bandwidth )!=0)
			return 0;

	}
	else if(direction==SOAPY_SDR_TX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		if(iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", true),"rf_bandwidth",&bandwidth )!=0)
			return 0;
	}

	return double(bandwidth);

}

std::vector<double> SoapyPlutoSDR::listBandwidths( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	return(options);
}
