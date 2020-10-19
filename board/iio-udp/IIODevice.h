#ifndef IIO_DEVICE_H
#define IIO_DEVICE_H

#include <iostream>
#include <vector>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdexcept>

#include <iio.h>
//#include <ad9361.h>

/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))


using namespace std;

// RX is input, TX is output
enum iodev { RX, TX };

// common RX and TX streaming params
struct stream_cfg 
{
        long long bw_hz; 	// Analog banwidth in Hz
        long long lo_hz; 	// Local oscillator frequency in Hz
        string rfport; 		// Port name
};


class IIODevice
{
	public:
		IIODevice(int _rxBufferSizeSample,int _txBufferSizeSample,struct stream_cfg _RXConfig, struct stream_cfg _TXConfig,long long rxsf, long long txsf);
		IIODevice(int _rxBufferSizeSample,int _txBufferSizeSample);
		~IIODevice();
		struct iio_buffer* enableChannels(enum iodev d);
		void disableChannels(enum iodev d);
		void setConfig(struct stream_cfg _rxConfig,enum iodev type);
		struct stream_cfg getConfig(enum iodev d);
		bool isStreaming(enum iodev d);
		void setGain(enum iodev d, long long gain);
		long long getGain(enum iodev d);
		void setSamplingFrequency(enum iodev d, long long sf);
                long long getSamplingFrequency(enum iodev d);
		string getGainMode(enum iodev d);
		void setGainMode(enum iodev d, string gainMode);
		char* receiveBuffer();
		void sendBuffer(char* buffer);
		char* getTXBufferPointer();
		void sendBufferFast();
		int getRXBufferSizeSample() {return rxBufferSizeSample; }
		int getTXBufferSizeSample() {return txBufferSizeSample; }
		struct iio_channel* getTx0_i() { return tx0_i; }
		struct iio_channel* getRx0_i() { return rx0_i; }
		void setBufferSizeSample(enum iodev d,int size);
	private:
		int rxBufferSizeSample;
		int txBufferSizeSample;
		
		struct stream_cfg RXConfig; 
		struct stream_cfg TXConfig;
	
		float rxGain;
		float txGain;

		long long rxSamplingFrequency;
		long long txSamplingFrequency;

		struct iio_context *ctx;
		struct iio_device *rx;
		struct iio_device *tx;
		struct iio_channel *rx0_i;
		struct iio_channel *rx0_q;
		struct iio_channel *tx0_i;
		struct iio_channel *tx0_q;
		struct iio_buffer  *rxbuf;
		struct iio_buffer  *txbuf;

		struct iio_device *phy;

		int sent_count;
		int recv_count;

		bool rxIsStreaming;
		bool txIsStreaming;
};

// check return value of attr_write function
void errchk(int v, const char* what); 

// write attribute: long long int
void wr_ch_lli(struct iio_channel *chn, const char* what, long long val);

// write attribute: string
void wr_ch_str(struct iio_channel *chn, const char* what, const char* str);

// read attribute: long long
long long rd_ch_lli(struct iio_channel *chn, const char* what);

// read attribute: string
string rd_ch_str(struct iio_channel *chn, const char* what);

// helper function generating channel names
char* get_ch_name(const char* type, int id);

// returns ad9361 phy device
struct iio_device* get_ad9361_phy(struct iio_context *ctx);

// finds AD9361 streaming IIO devices
struct iio_device* get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d);

// finds AD9361 streaming IIO channels
struct iio_channel* get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid);

// finds AD9361 phy IIO configuration channel with id chid
bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn);

// finds AD9361 local oscillator IIO configuration channels
bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn);

#endif
