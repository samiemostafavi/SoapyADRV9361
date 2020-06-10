#include "IIODevice.h"

struct IIODevice* io = NULL;
char tmpstr[64];

/* check return value of attr_write function */
int errchk(int v, const char* what) 
{
        if(v < 0)
	{
		char buffer [100];
		printf(buffer,"Error %d writing/reading to/from channel %s \n", v, what);
	}
	return v;
}

/* write attribute: long long int */
int wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
        return errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: char* */
int wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
        return errchk(iio_channel_attr_write(chn, what, str), what);
}

/* read attribute: long long int */
int rd_ch_lli(struct iio_channel *chn, const char* what, long long* val)
{
        return errchk(iio_channel_attr_read_longlong(chn, what, val), what);
}

/* read attribute: char* */
int rd_ch_str(struct iio_channel *chn, const char* what, char* res)
{
        return errchk(iio_channel_attr_read(chn, what, res,sizeof(res)), what);
}
	

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
		printf("No ad9361-phy found\n");

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
			{
				printf("No ad9361 TX device found\n");
				return NULL;
			}
		}
        	case RX:
		{	
			dev = iio_context_find_device(ctx, "cf-ad9361-lpc"); 
			if(dev == NULL)
			{
				printf("No ad9361 RX device found\n");
				return NULL;
			}
		}
        	default:
		{
			printf("Wrong enum iodev\n");
			return NULL;
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
		{
			printf("No ad9361 stream channel found\n");
			return NULL;
		}
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


int IIODeviceInit(int _rxBufferSizeSample,int _txBufferSizeSample,char* _rfportRX,long long _bwRX,long long _fsRX,long long _loRX, char* _rfportTX, long long _bwTX, long long _fsTX, long long _loTX)
{
	io = (struct IIODevice*) malloc(sizeof(struct IIODevice));

	io->rxBufferSizeSample = _rxBufferSizeSample;
       	io->txBufferSizeSample = _txBufferSizeSample;

	io->rxIsStreaming = false;
	io->txIsStreaming = false;
	
	io->rxbuf = NULL;
        io->rx0_i = NULL;
        io->rx0_q = NULL;
        io->txbuf = NULL;
        io->tx0_i = NULL;
        io->tx0_q = NULL;

        if((io->ctx = iio_create_default_context()) == NULL)
	{
		printf("No context\n"); 
		return -1;
	}

        if(iio_context_get_devices_count(io->ctx) == 0);
	{
		printf("No IIO device found\n");
		return -1;
	}

        if((io->rx = get_ad9361_stream_dev(io->ctx, RX)) == NULL)
		return -1;

        if((io->tx = get_ad9361_stream_dev(io->ctx, TX)) == NULL)
		return -1;

	setRFPort(_rfportRX,RX); setBW(_bwRX,RX); setFS(_fsRX,RX); setLO(_loRX,RX);
	setRFPort(_rfportTX,TX); setBW(_bwTX,TX); setFS(_fsTX,TX); setLO(_loTX,TX);

	return 0;
}

void IIODeviceDelete()
{
	disableChannels(RX);
	disableChannels(TX);

	printf("Destroying context\n");
        if (io->ctx)
                iio_context_destroy(io->ctx);

	free(io->RXConfig);
	free(io->TXConfig);
	free(io);
	io = NULL;
}

char* receiveBuffer()
{
	if(io->rxbuf != NULL)
	{
		// Receive a buffer
		int nbytes_rx = iio_buffer_refill(io->rxbuf);
		if (nbytes_rx < 0)
		{
			printf("Error refilling buf from ad9361\n");
			return NULL;
		}

		// Send the buffer pointer out
		char* p_dat = (char*)iio_buffer_first(io->rxbuf,io->rx0_i);
		return p_dat;
	}
	else
	{
		printf("Buffer refill error, rxbuf does not exist\n");
		return NULL;
	}
}

int sendBuffer(char* buffer)
{
	if(io->txbuf != NULL && io->tx0_i != NULL)
	{
		// Get IIO buffer pointer, copy the data
		char* t_dat = (char*) iio_buffer_first(io->txbuf,io->tx0_i);
		memcpy(t_dat,buffer,io->txBufferSizeSample*4); // Since each sample is 4 bytes

		// Send the buffer
		int nbytes_tx = iio_buffer_push(io->txbuf);
		if (nbytes_tx < 0)
		{
			printf("Error pushing the buffer to ad9361\n");
			return -1;
		}
	}
	else
	{
		printf("Buffer refill error, txbuf does not exist\n");
		return -1;
	}
	return 0;
}

int sendBufferFast()
{
	if(io->txbuf != NULL)
	{
		// Push the buffer only
		int nbytes_tx = iio_buffer_push(io->txbuf);
		if (nbytes_tx < 0)
		{
			printf("Error pushing the buffer to ad9361\n");
			return -1;
		}
	}
	else
	{
		printf("Buffer refill error, txbuf does not exist\n");
		return -1;
	}
	return 0;
}

char* getTXBufferPointer()
{
	if(io->txbuf != NULL && io->tx0_i != NULL)
	{
		// Get IIO buffer pointer, copy the data
		char* t_dat = (char*) iio_buffer_first(io->txbuf,io->tx0_i);
		return t_dat;
	}
	else
	{
		printf("Buffer refill error, txbuf does not exist\n");
		return NULL;
	}
}

// applies streaming configuration through IIO
int setRFPort(const char* _rfport, enum iodev type)
{
	// check if the streaming is stopped
        if(isStreaming(type))
	{
		printf("set config %s while streaming\n",(type==RX)?("RX"):("TX"));
		return -1;
	}

        struct iio_channel *chn = NULL;

        // Get phy channel
        if (!get_phy_chan(io->ctx, type, 0, &chn))
	{
		printf("Could not find physical channel in setConfig\n");
		return -1;
	}

	return wr_ch_str(chn, "rf_port_select",_rfport);
}

// applies streaming configuration through IIO
int setBW(long long _bw_hz, enum iodev type)
{
	// check if the streaming is stopped
        if(isStreaming(type))
	{
		printf("set config %s while streaming\n",(type==RX)?("RX"):("TX"));
		return -1;
	}

        struct iio_channel *chn = NULL;

        // Get phy channel
        if (!get_phy_chan(io->ctx, type, 0, &chn))
	{
		printf("Could not find physical channel in setConfig\n");
		return -1;
	}

	return wr_ch_lli(chn, "rf_bandwidth",_bw_hz);
}

// applies streaming configuration through IIO
int setFS(long long _fs_hz, enum iodev type)
{
	// check if the streaming is stopped
        if(isStreaming(type))
	{
		printf("set config %s while streaming\n",(type==RX)?("RX"):("TX"));
		return -1;
	}

        struct iio_channel *chn = NULL;

        // Get phy channel
        if (!get_phy_chan(io->ctx, type, 0, &chn))
	{
		printf("Could not find physical channel in setConfig\n");
		return -1;
	}

	return wr_ch_lli(chn, "sampling_frequency", _fs_hz);
}

// applies streaming configuration through IIO
int setLO(long long _lo_hz, enum iodev type)
{
	// check if the streaming is stopped
        if(isStreaming(type))
	{
		printf("set config %s while streaming\n",(type==RX)?("RX"):("TX"));
		return -1;
	}

        struct iio_channel *chn = NULL;

        // Get LO channel
        if (!get_lo_chan(io->ctx, type, &chn))
	{
		printf("Could not find lo channel in setConfig\n");
		return -1;
	}

	return wr_ch_lli(chn, "frequency", _lo_hz);
}

int getRFPort(enum iodev d, char* res)
{
        struct iio_channel *chn = NULL;

        // Get phy channel
        if (!get_phy_chan(io->ctx, d, 0, &chn))
	{
		printf("Could not find physical channel in getConfig\n");
		return -1;
	}
	
	// Write it
	return rd_ch_str(chn, "rf_port_select", res);
}

int getBW(enum iodev d, long long* res)
{
        struct iio_channel *chn = NULL;
        
	// Get phy channel
        if (!get_phy_chan(io->ctx, d, 0, &chn))
	{
		printf("Could not find physical channel in getConfig\n");
		return -1;
	}

        return rd_ch_lli(chn, "rf_bandwidth", res);
}

int getFS(enum iodev d,long long* res)
{
        struct iio_channel *chn = NULL;

	// Get phy channel
        if (!get_phy_chan(io->ctx, d, 0, &chn))
	{
		printf("Could not find physical channel in getConfig\n");
		return -1;
	}

        return rd_ch_lli(chn, "sampling_frequency",res);
}

int getLO(enum iodev d, long long* res)
{
        struct iio_channel *chn = NULL;

        // Get LO channel
        if (!get_lo_chan(io->ctx, d, &chn))
	{
		printf("Could not find lo channel in getConfig\n");
		return -1;
	}

        return rd_ch_lli(chn, "frequency", res);
}

bool isStreaming(enum iodev d)
{
	switch (d)
        {
		case RX: { return io->rxIsStreaming; }
		case TX: { return io->txIsStreaming; }
        }
}

int getGain(enum iodev d, long long* res)
{
	// get gain
        struct iio_channel *chn = NULL;

	if (!get_phy_chan(io->ctx, d, 0, &chn))
	{
		printf("Could not find physical channel in getGain\n");
		return -1;
	}
        
	return rd_ch_lli(chn, "hardwaregain",res);
}

int setGain(enum iodev d, long long gain)
{
	// set the gain
        struct iio_channel *chn = NULL;

	if (!get_phy_chan(io->ctx, d, 0, &chn))
	{
		printf("Could not find physical channel in setGain\n");
		return -1;
	}
        
	return wr_ch_lli(chn, "hardwaregain", gain);
}

int enableChannels(enum iodev d)
{
	switch (d)
	{
        	case RX:
		{
			if(io->rxIsStreaming)
			{
				printf("Enabling rx channel again, disable first\n");
				return -1;
			}

			printf("Creating RX buffers and channels\n");
        		io->rx0_i = get_ad9361_stream_ch(io->ctx, RX, io->rx, 0);
		        io->rx0_q = get_ad9361_stream_ch(io->ctx, RX, io->rx, 1);
			iio_channel_enable(io->rx0_i);
        		iio_channel_enable(io->rx0_q);
		        io->rxbuf = iio_device_create_buffer(io->rx, io->rxBufferSizeSample, false);
			if(io->rxbuf == NULL)
			{
				printf("Could not create rx buffer\n");
				return -1;
			}
			
			io->rxIsStreaming = true;
			
			break;
		}
	        case TX:
		{
			if(io->txIsStreaming)
			{
				printf("Enabling tx channel again, disable first\n");
				return -1;
			}
			
			printf("Creating TX buffers and channels\n");
        		io->tx0_i = get_ad9361_stream_ch(io->ctx, TX, io->tx, 0);
		        io->tx0_q = get_ad9361_stream_ch(io->ctx, TX, io->tx, 1);
			iio_channel_enable(io->tx0_i);
        		iio_channel_enable(io->tx0_q);
		        io->txbuf = iio_device_create_buffer(io->tx, io->txBufferSizeSample, false);
			if(io->txbuf == NULL)
			{
				printf("Could not create tx buffer\n");
				return -1;
			}

			io->txIsStreaming = true;
			
			break;
		}
        }

}

void disableChannels(enum iodev d)
{
	switch (d)
	{
        	case RX:
		{
			printf("Destroying RX buffers and channels\n");
			if (io->rxbuf) { iio_buffer_destroy(io->rxbuf); io->rxbuf = NULL; }
			if (io->rx0_i) { iio_channel_disable(io->rx0_i); io->rx0_i = NULL; }
			if (io->rx0_q) { iio_channel_disable(io->rx0_q); io->rx0_q = NULL; }
			io->rxIsStreaming = false;

			break;
		}
	        case TX:
		{
			printf("Destroying TX buffers and channels\n");
			if (io->txbuf) { iio_buffer_destroy(io->txbuf); io->txbuf = NULL; }
			if (io->tx0_i) { iio_channel_disable(io->tx0_i); io->tx0_i = NULL; }
			if (io->tx0_q) { iio_channel_disable(io->tx0_q); io->tx0_q = NULL; }
			io->txIsStreaming = false;

			break;
		}
        }

}



