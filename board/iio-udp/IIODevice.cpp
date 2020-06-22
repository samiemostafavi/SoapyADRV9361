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
			break;
		}
        	case RX:
		{	
			dev = iio_context_find_device(ctx, "cf-ad9361-lpc"); 
			if(dev == NULL)
				throw runtime_error("No ad9361 RX device found");
			break;
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


int ad9361_set_trx_fir_enable(struct iio_device *dev, int enable)
{
	int ret = iio_device_attr_write_bool(dev,"in_out_voltage_filter_fir_en", !!enable);
	if (ret < 0)
		ret = iio_channel_attr_write_bool(iio_device_find_channel(dev, "out", false),"voltage_filter_fir_en", !!enable);
	
	return ret;
}

int ad9361_get_trx_fir_enable(struct iio_device *dev, int *enable)
{
	bool value;

	int ret = iio_device_attr_read_bool(dev, "in_out_voltage_filter_fir_en", &value);

	if (ret < 0)
		ret = iio_channel_attr_read_bool(iio_device_find_channel(dev, "out", false),
						 "voltage_filter_fir_en", &value);
	if (!ret)
		*enable	= value;

	return ret;
}

static int16_t fir_128_4[] = {
	-15,-27,-23,-6,17,33,31,9,-23,-47,-45,-13,34,69,67,21,-49,-102,-99,-32,69,146,143,48,-96,-204,-200,-69,129,278,275,97,-170,
	-372,-371,-135,222,494,497,187,-288,-654,-665,-258,376,875,902,363,-500,-1201,-1265,-530,699,1748,1906,845,-1089,-2922,-3424,
	-1697,2326,7714,12821,15921,15921,12821,7714,2326,-1697,-3424,-2922,-1089,845,1906,1748,699,-530,-1265,-1201,-500,363,902,875,
	376,-258,-665,-654,-288,187,497,494,222,-135,-371,-372,-170,97,275,278,129,-69,-200,-204,-96,48,143,146,69,-32,-99,-102,-49,21,
	67,69,34,-13,-45,-47,-23,9,31,33,17,-6,-23,-27,-15};

static int16_t fir_128_2[] = {
	-0,0,1,-0,-2,0,3,-0,-5,0,8,-0,-11,0,17,-0,-24,0,33,-0,-45,0,61,-0,-80,0,104,-0,-134,0,169,-0,
	-213,0,264,-0,-327,0,401,-0,-489,0,595,-0,-724,0,880,-0,-1075,0,1323,-0,-1652,0,2114,-0,-2819,0,4056,-0,-6883,0,20837,32767,
	20837,0,-6883,-0,4056,0,-2819,-0,2114,0,-1652,-0,1323,0,-1075,-0,880,0,-724,-0,595,0,-489,-0,401,0,-327,-0,264,0,-213,-0,
	169,0,-134,-0,104,0,-80,-0,61,0,-45,-0,33,0,-24,-0,17,0,-11,-0,8,0,-5,-0,3,0,-2,-0,1,0,-0, 0 };

static int16_t fir_96_2[] = {
	-4,0,8,-0,-14,0,23,-0,-36,0,52,-0,-75,0,104,-0,-140,0,186,-0,-243,0,314,-0,-400,0,505,-0,-634,0,793,-0,
	-993,0,1247,-0,-1585,0,2056,-0,-2773,0,4022,-0,-6862,0,20830,32767,20830,0,-6862,-0,4022,0,-2773,-0,2056,0,-1585,-0,1247,0,-993,-0,
	793,0,-634,-0,505,0,-400,-0,314,0,-243,-0,186,0,-140,-0,104,0,-75,-0,52,0,-36,-0,23,0,-14,-0,8,0,-4,0};

static int16_t fir_64_2[] = {
	-58,0,83,-0,-127,0,185,-0,-262,0,361,-0,-488,0,648,-0,-853,0,1117,-0,-1466,0,1954,-0,-2689,0,3960,-0,-6825,0,20818,32767,
	20818,0,-6825,-0,3960,0,-2689,-0,1954,0,-1466,-0,1117,0,-853,-0,648,0,-488,-0,361,0,-262,-0,185,0,-127,-0,83,0,-58,0};

#define FIR_BUF_SIZE 8192

int ad9361_set_bb_rate(struct iio_device *dev, unsigned long rate)
{
	struct iio_channel *chan;
	long long current_rate;
	int dec, taps, ret, i, enable, len = 0;
	int16_t *fir;
	char *buf;

	if (rate <= 20000000UL) {
		dec = 4;
		taps = 128;
		fir = fir_128_4;
	} else if (rate <= 40000000UL) {
		dec = 2;
		fir = fir_128_2;
		taps = 128;
	} else if (rate <= 53333333UL) {
		dec = 2;
		fir = fir_96_2;
		taps = 96;
	} else {
		dec = 2;
		fir = fir_64_2;
		taps = 64;
	}

	chan = iio_device_find_channel(dev, "voltage0", true);
	if (chan == NULL)
		return -ENODEV;

	ret = iio_channel_attr_read_longlong(chan, "sampling_frequency", &current_rate);
	if (ret < 0)
		return ret;

	ret = ad9361_get_trx_fir_enable(dev, &enable);
	if (ret < 0)
		return ret;

	if (enable) {
		if (current_rate <= (25000000 / 12))
			iio_channel_attr_write_longlong(chan, "sampling_frequency", 3000000);

		ret = ad9361_set_trx_fir_enable(dev, false);
		if (ret < 0)
			return ret;
	}

	buf = (char*)malloc(FIR_BUF_SIZE);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, FIR_BUF_SIZE - len, "RX 3 GAIN -6 DEC %d\n", dec);
	len += snprintf(buf + len, FIR_BUF_SIZE - len, "TX 3 GAIN 0 INT %d\n", dec);

	for (i = 0; i < taps; i++)
		len += snprintf(buf + len, FIR_BUF_SIZE - len, "%d,%d\n", fir[i], fir[i]);

	len += snprintf(buf + len, FIR_BUF_SIZE - len, "\n");

	ret = iio_device_attr_write_raw(dev, "filter_fir_config", buf, len);
	free (buf);


	if (ret < 0)
		return ret;

	if (rate <= (25000000 / 12))  {
		int dacrate, txrate, max;
		char readbuf[100];

		ret = iio_device_attr_read(dev, "tx_path_rates", readbuf, sizeof(readbuf));
		if (ret < 0)
			return ret;
		ret = sscanf(readbuf, "BBPLL:%*d DAC:%d T2:%*d T1:%*d TF:%*d TXSAMP:%d", &dacrate, &txrate);
		if (ret != 2)
			return -EFAULT;

		if (txrate == 0)
			return -EINVAL;

		max = (dacrate / txrate) * 16;
		if (max < taps)
			iio_channel_attr_write_longlong(chan, "sampling_frequency", 3000000);

		ret = ad9361_set_trx_fir_enable(dev, true);
		if (ret < 0)
			return ret;
		ret = iio_channel_attr_write_longlong(chan, "sampling_frequency", rate);
		if (ret < 0)
			return ret;
	} else {
		ret = iio_channel_attr_write_longlong(chan, "sampling_frequency", rate);
		if (ret < 0)
			return ret;
		ret = ad9361_set_trx_fir_enable(dev, true);
		if (ret < 0)
			return ret;
	}

	return 0;
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

        /*if(iio_context_get_devices_count(ctx) == 0);
		throw runtime_error("No IIO device found");*/

        // rx = get_ad9361_stream_dev(ctx, RX);
        // tx = get_ad9361_stream_dev(ctx, TX);

	phy = get_ad9361_phy(ctx);

	setConfig(_RXConfig, RX);
	setConfig(_TXConfig, TX);

	getGain(RX);
	getGain(TX);
}

IIODevice::IIODevice(int _rxBufferSizeSample,int _txBufferSizeSample) :
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

        /*if(iio_context_get_devices_count(ctx) == 0);
		throw runtime_error("No IIO device found");*/

        // rx = get_ad9361_stream_dev(ctx, RX);
        // tx = get_ad9361_stream_dev(ctx, TX);
	
	phy = get_ad9361_phy(ctx);

	RXConfig = getConfig(RX);
        TXConfig = getConfig(TX);
	
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

void IIODevice::setBufferSizeSample(enum iodev d,int size)
{
	// Update member variables      
        switch (d)
        {
                case RX: { rxBufferSizeSample = size; break; }
                case TX: { txBufferSizeSample = size; break; }
                default: { throw runtime_error("Wrong enum iodev"); }
        }
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
	{
		wr_ch_lli(chn, "sampling_frequency", cfg.fs_hz);

		if(ad9361_set_bb_rate(phy,(unsigned long)cfg.fs_hz))
			throw runtime_error("Unable to set BB rate.");
	}

        // Configure LO channel
        if (!get_lo_chan(ctx, type, &chn))
		throw runtime_error("Could not find lo channel in setConfig");

	if(cfg.lo_hz != lcfg.lo_hz)
	        wr_ch_lli(chn, "frequency", cfg.lo_hz);

	// Update member variables	
	switch (type)
        {
		case RX: { RXConfig = getConfig(RX); break; }
		case TX: { TXConfig = getConfig(TX); break; }
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
		case RX: { RXConfig = cfg; break; }
		case TX: { TXConfig = cfg; break; }
                default: { throw runtime_error("Wrong enum iodev"); }
        }

	return cfg;
}

bool IIODevice::isStreaming(enum iodev d)
{
	switch (d)
        {
		case RX: { return rxIsStreaming; break; }
		case TX: { return txIsStreaming; break; }
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

string IIODevice::getGainMode(enum iodev d)
{
        struct iio_channel *chn = NULL;
	
	if (!get_phy_chan(ctx, d, 0, &chn))
		throw runtime_error("Could not find physical channel in getGainMode");
        
	return rd_ch_str(chn, "gain_control_mode");
}

void IIODevice::setGainMode(enum iodev d, string gainMode)
{
        struct iio_channel *chn = NULL;
	
	if (!get_phy_chan(ctx, d, 0, &chn))
		throw runtime_error("Could not find physical channel in getGainMode");
        
	wr_ch_str(chn, "gain_control_mode", gainMode.c_str());
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
        		rx = get_ad9361_stream_dev(ctx, RX);
			if(rx == NULL)
				throw runtime_error("Unable to get the rx stream dev");

        		rx0_i = get_ad9361_stream_ch(ctx, RX, rx, 0);
			if(rx0_i == NULL)
				throw runtime_error("Unable to get the rx stream channel i");
		        
			rx0_q = get_ad9361_stream_ch(ctx, RX, rx, 1);
			if(rx0_q == NULL)
				throw runtime_error("Unable to get the rx stream channel q");
			
			iio_channel_enable(rx0_i);
        		iio_channel_enable(rx0_q);
		        
			cout << "rxBufferSizeSample (IIODevice): " << rxBufferSizeSample << endl;

			if(rxbuf == NULL)
				rxbuf = iio_device_create_buffer(rx, rxBufferSizeSample, false);
			if(rxbuf == NULL)
				throw runtime_error("Unable to create the rx buffer");
							
			rxIsStreaming = true;
			
			break;
		}
	        case TX:
		{
			if(txIsStreaming)
				throw runtime_error("Enabling tx channel again, disable first");
			
			printf("Creating TX buffers and channels\n");
        		tx = get_ad9361_stream_dev(ctx, TX);
			if(tx == NULL)
				throw runtime_error("Unable to get the tx stream dev");

        		tx0_i = get_ad9361_stream_ch(ctx, TX, tx, 0);
			if(tx0_i == NULL)
				throw runtime_error("Unable to get the tx stream channel i");

		        tx0_q = get_ad9361_stream_ch(ctx, TX, tx, 1);
			if(tx0_q == NULL)
				throw runtime_error("Unable to get the tx stream channel q");

			iio_channel_enable(tx0_i);
        		iio_channel_enable(tx0_q);
			
			cout << "txBufferSizeSample (IIODevice): " << txBufferSizeSample << endl;

			if(txbuf == NULL)
		        	txbuf = iio_device_create_buffer(tx, txBufferSizeSample, false);
			if(txbuf == NULL)
				throw runtime_error(string("Unable to create the tx buffer: ") + string(strerror(errno)));

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
			if (rxbuf) 
			{
				printf("Destroying RX buffer\n");
				iio_buffer_cancel(rxbuf); 
				iio_buffer_destroy(rxbuf); 
				rxbuf = NULL;
			}
			if (rx0_i) 
			{ 
				printf("Disabling RX channel i\n");
				iio_channel_disable(rx0_i); 
				rx0_i = NULL; 
			}
			if (rx0_q) 
			{ 
				printf("Disabling RX channel q\n");
				iio_channel_disable(rx0_q); 
				rx0_q = NULL; 
			}
			rxIsStreaming = false;
			break;
		}
	        case TX:
		{
			if (txbuf) 
			{
				printf("Destroying TX buffer\n");
				iio_buffer_cancel(txbuf); 
				iio_buffer_destroy(txbuf); 
				txbuf = NULL;
			}
			if (tx0_i) 
			{ 
				printf("Disabling TX channel i\n");
				iio_channel_disable(tx0_i); 
				tx0_i = NULL; 
			}
			if (tx0_q) 
			{ 
				printf("Disabling TX channel q\n");
				iio_channel_disable(tx0_q); 
				tx0_q = NULL;
			}
			txIsStreaming = false;
			break;
		}
                default: { throw runtime_error("Wrong enum iodev"); }
        }
        


}

