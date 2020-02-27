#include "IIODevice.h"


/* check return value of attr_write function */
void errchk(int v, const char* what) 
{
        if(v < 0)
	{
		char buffer [100];
		int n = sprintf(buffer,"Error %d writing/reading to/from channel %s", v, what);
		throw runtime_error(string(buffer,n));
	}
}

/* write attribute: long long int */
void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
        errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
        errchk(iio_channel_attr_write(chn, what, str), what);
}

/* read attribute: long long int */
long long rd_ch_lli(struct iio_channel *chn, const char* what)
{
	long long val;
        errchk(iio_channel_attr_read_longlong(chn, what, &val), what);
	return val;
}

/* read attribute: string */
string rd_ch_str(struct iio_channel *chn, const char* what)
{
	char val[100];
	int ret;
        errchk((ret = iio_channel_attr_read(chn, what, val,sizeof(val))), what);
	return string(val,ret);
}
	
char tmpstr[64];

/* helper function generating channel names */
char* get_ch_name(const char* type, int id)
{
        snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
        return tmpstr;
}

/* returns ad9361 phy device */
struct iio_device* get_ad9361_phy(struct iio_context *ctx)
{
        struct iio_device *dev =  iio_context_find_device(ctx, "ad9361-phy");
        if(dev==NULL)
		throw runtime_error("No ad9361-phy found");

        return dev;
}

/* finds AD9361 streaming IIO devices */
struct iio_device* get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d)
{
	struct iio_device* dev;
        switch(d)
	{
        	case TX: 
		{	
			dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc"); 
			if(dev == NULL)
				throw runtime_error("No ad9361 TX device found");
		}
        	case RX:
		{	
			dev = iio_context_find_device(ctx, "cf-ad9361-lpc"); 
			if(dev == NULL)
				throw runtime_error("No ad9361 RX device found");
		}
        	default:
		{
			throw runtime_error("Wrong enum iodev");
		}
        }
	return dev;
}

/* finds AD9361 streaming IIO channels */
struct iio_channel* get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid)
{
        struct iio_channel* chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
        if (chn == NULL)
	{
                chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
		if(chn == NULL)
			throw runtime_error("No ad9361 stream channel found");
	}
	return chn;
}

/* finds AD9361 phy IIO configuration channel with id chid */
bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
        switch (d) {
        case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
        case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), true);  return *chn != NULL;
        default: return false;
        }
}

/* finds AD9361 local oscillator IIO configuration channels */
bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn)
{
        switch (d) {
         // LO chan is always output, i.e. true
        case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 0), true); return *chn != NULL;
        case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 1), true); return *chn != NULL;
        default: return false;
        }
}


IIODevice::IIODevice(int _rxBufferSizeSample,int _txBufferSizeSample,struct stream_cfg _RXConfig, struct stream_cfg _TXConfig) :
	rxBufferSizeSample(_rxBufferSizeSample), txBufferSizeSample(_txBufferSizeSample)
{
	rxIsStreaming = false;
	txIsStreaming = false;

        RXConfig.bw_hz = 0;
        RXConfig.fs_hz = 0;
        RXConfig.lo_hz = 0;
        RXConfig.rfport = "";

        TXConfig.bw_hz = 0;
        TXConfig.fs_hz = 0;
        TXConfig.lo_hz = 0;
        TXConfig.rfport = "";

	rxbuf = NULL;
        rx0_i = NULL;
        rx0_q = NULL;
        txbuf = NULL;
        tx0_i = NULL;
        tx0_q = NULL;

        if((ctx = iio_create_default_context()) == NULL)
		throw runtime_error("No context");

        if(iio_context_get_devices_count(ctx) == 0);
		throw runtime_error("No IIO device found");

        rx = get_ad9361_stream_dev(ctx, RX);
        tx = get_ad9361_stream_dev(ctx, TX);

	setConfig(_RXConfig, RX);
	setConfig(_TXConfig, TX);

	getGain(RX);
	getGain(TX);
}

IIODevice::~IIODevice()
{
	disableChannels(RX);
	disableChannels(TX);

	printf("Destroying context\n");
        if (ctx)
                iio_context_destroy(ctx);
}

char* IIODevice::receiveBuffer()
{
	if(rxbuf)
	{
		// Receive a buffer
		int nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0)
			throw runtime_error("Error refilling buf from ad9361");

		// Send the buffer pointer out
		char* p_dat = (char*)iio_buffer_first(rxbuf,rx0_i);
		return p_dat;
	}
	else
		throw runtime_error("Buffer refill error, rxbuf does not exist");
}

void IIODevice::sendBuffer(char* buffer)
{
	if(txbuf)
	{
		// Get IIO buffer pointer, copy the data
		char* t_dat = (char*) iio_buffer_first(txbuf,tx0_i);
		memcpy(t_dat,buffer,txBufferSizeSample*4); // Since each sample is 4 bytes

		// Send the buffer
		int nbytes_tx = iio_buffer_push(txbuf);
		if (nbytes_tx < 0) 
			throw runtime_error("Error pushing the buffer to ad9361");
	}
	else
		throw runtime_error("Buffer refill error, txbuf does not exist");
}

void IIODevice::sendBufferFast()
{
	if(txbuf)
	{
		// Push the buffer only
		int nbytes_tx = iio_buffer_push(txbuf);
		if (nbytes_tx < 0) 
			throw runtime_error("Error pushing the buffer to ad9361");
	}
	else
		throw runtime_error("Buffer refill error, txbuf does not exist");
}

char* IIODevice::getTXBufferPointer()
{
	if(txbuf)
	{
		// Get IIO buffer pointer, copy the data
		char* t_dat = (char*) iio_buffer_first(txbuf,tx0_i);
		return t_dat;
	}
	else
		throw runtime_error("Buffer refill error, txbuf does not exist");
}

// applies streaming configuration through IIO
void IIODevice::setConfig(struct stream_cfg cfg, enum iodev type)
{
	// check if the streaming is stopped
        if(isStreaming(type))
		throw runtime_error(string("set config ") + string((type==RX)?("RX"):("TX"))  + string(" while streaming"));

        struct iio_channel *chn = NULL;

        // Configure phy and lo channels
        if (!get_phy_chan(ctx, type, 0, &chn))
		throw runtime_error("Could not find physical channel in setConfig");

	// get the local config
	struct stream_cfg lcfg = (type == RX) ? (RXConfig) : (TXConfig);

	// compare each parameter, update the new ones only
	if(cfg.rfport != lcfg.rfport)
	        wr_ch_str(chn, "rf_port_select",     cfg.rfport.c_str());

	if(cfg.bw_hz != lcfg.bw_hz)
	        wr_ch_lli(chn, "rf_bandwidth",       cfg.bw_hz);

	if(cfg.fs_hz != lcfg.fs_hz)
	        wr_ch_lli(chn, "sampling_frequency", cfg.fs_hz);

        // Configure LO channel
        if (!get_lo_chan(ctx, type, &chn))
		throw runtime_error("Could not find lo channel in setConfig");

	if(cfg.lo_hz != lcfg.lo_hz)
	        wr_ch_lli(chn, "frequency", cfg.lo_hz);

	// Update member variables	
	switch (type)
        {
		case RX: { RXConfig = getConfig(RX); }
		case TX: { TXConfig = getConfig(TX); }
                default: { throw runtime_error("Wrong enum iodev"); }
        }
}

struct stream_cfg IIODevice::getConfig(enum iodev d)
{
        struct iio_channel *chn = NULL;
	struct stream_cfg cfg;

        // Configure phy and lo channels
        if (!get_phy_chan(ctx, d, 0, &chn))
		throw runtime_error("Could not find physical channel in getConfig");

        cfg.rfport = rd_ch_str(chn, "rf_port_select");
        cfg.bw_hz = rd_ch_lli(chn, "rf_bandwidth");
        cfg.fs_hz = rd_ch_lli(chn, "sampling_frequency");

        // Configure LO channel
        if (!get_lo_chan(ctx, d, &chn))
		throw runtime_error("Could not find lo channel in getConfig");

        cfg.lo_hz = rd_ch_lli(chn, "frequency");
	
	switch (d)
        {
		case RX: { RXConfig = cfg; }
		case TX: { TXConfig = cfg; }
                default: { throw runtime_error("Wrong enum iodev"); }
        }

	return cfg;
}

bool IIODevice::isStreaming(enum iodev d)
{
	switch (d)
        {
		case RX: { return rxIsStreaming; }
		case TX: { return txIsStreaming; }
                default: { throw runtime_error("Wrong enum iodev"); }
        }
}

long long IIODevice::getGain(enum iodev d)
{
	// get gain
	long long gain = 0;
        struct iio_channel *chn = NULL;

	if (!get_phy_chan(ctx, d, 0, &chn))
		throw runtime_error("Could not find physical channel in getGain");
        
	gain = rd_ch_lli(chn, "hardwaregain");

	// update the member variable
	switch (d)
        {
		case RX: 
		{
			rxGain = gain;
			return rxGain;
		}
		case TX: 
		{
			txGain = gain;
			return txGain; 
		}
                default: { throw runtime_error("Wrong enum iodev"); }
        }
}

void IIODevice::setGain(enum iodev d, long long gain)
{
	// set the gain
        struct iio_channel *chn = NULL;

	if (!get_phy_chan(ctx, d, 0, &chn))
		throw runtime_error("Could not find physical channel in setGain");
        
	wr_ch_lli(chn, "hardwaregain", gain);

	// update the member variable
	switch (d)
        {
		case RX: 
		{
			rxGain = gain;
			break;
		}
		case TX: 
		{ 
			txGain = gain;
			break;
		}
                default: { throw runtime_error("Wrong enum iodev"); }
        }
}

void IIODevice::enableChannels(enum iodev d)
{
	switch (d)
	{
        	case RX:
		{
			if(rxIsStreaming)
				throw runtime_error("Enabling rx channel again, disable first");

			printf("Creating RX buffers and channels\n");
        		rx0_i = get_ad9361_stream_ch(ctx, RX, rx, 0);
		        rx0_q = get_ad9361_stream_ch(ctx, RX, rx, 1);
			iio_channel_enable(rx0_i);
        		iio_channel_enable(rx0_q);
		        rxbuf = iio_device_create_buffer(rx, rxBufferSizeSample, false);
			
			rxIsStreaming = true;
			
			break;
		}
	        case TX:
		{
			if(txIsStreaming)
				throw runtime_error("Enabling tx channel again, disable first");
			
			printf("Creating TX buffers and channels\n");
        		tx0_i = get_ad9361_stream_ch(ctx, TX, tx, 0);
		        tx0_q = get_ad9361_stream_ch(ctx, TX, tx, 1);
			iio_channel_enable(tx0_i);
        		iio_channel_enable(tx0_q);
		        txbuf = iio_device_create_buffer(tx, txBufferSizeSample, false);

			txIsStreaming = true;
			
			break;
		}
                default: { throw runtime_error("Wrong enum iodev"); }
        }

}

void IIODevice::disableChannels(enum iodev d)
{
	switch (d)
	{
        	case RX:
		{
			printf("Destroying RX buffers and channels\n");
			if (rxbuf) { iio_buffer_destroy(rxbuf); rxbuf = NULL; }
			if (rx0_i) { iio_channel_disable(rx0_i); rx0_i = NULL; }
			if (rx0_q) { iio_channel_disable(rx0_q); rx0_q = NULL; }
			rxIsStreaming = false;

			break;
		}
	        case TX:
		{
			printf("Destroying TX buffers and channels\n");
			if (txbuf) { iio_buffer_destroy(txbuf); txbuf = NULL; }
			if (tx0_i) { iio_channel_disable(tx0_i); tx0_i = NULL; }
			if (tx0_q) { iio_channel_disable(tx0_q); tx0_q = NULL; }
			txIsStreaming = false;

			break;
		}
                default: { throw runtime_error("Wrong enum iodev"); }
        }

}



