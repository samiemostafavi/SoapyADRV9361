#ifndef IIO_DEVICE_H
#define IIO_DEVICE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <iio.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

/* helper macros */
#define MHZ(x) ((long long)(x*1000000.0 + .5))
#define GHZ(x) ((long long)(x*1000000000.0 + .5))

// RX is input, TX is output
enum iodev { RX, TX };

struct IIODevice
{
	int rxBufferSizeSample;
	int txBufferSizeSample;
		
	struct stream_cfg* RXConfig; 
	struct stream_cfg* TXConfig;
	
	float rxGain;
	float txGain;

	struct iio_context *ctx;
	struct iio_device *rx;
	struct iio_device *tx;
	struct iio_channel *rx0_i;
	struct iio_channel *rx0_q;
	struct iio_channel *tx0_i;
	struct iio_channel *tx0_q;
	struct iio_buffer  *rxbuf;
	struct iio_buffer  *txbuf;

	int sent_count;
	int recv_count;
	
	bool rxIsStreaming;
	bool txIsStreaming;
};

int errchk(int v, const char* what);
int wr_ch_lli(struct iio_channel *chn, const char* what, long long val);
int wr_ch_str(struct iio_channel *chn, const char* what, const char* str);
int rd_ch_lli(struct iio_channel *chn, const char* what, long long* val);
int rd_ch_str(struct iio_channel *chn, const char* what, char* res);
char* get_ch_name(const char* type, int id);
struct iio_device* get_ad9361_phy(struct iio_context *ctx);
struct iio_device* get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d);
struct iio_channel* get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid);
bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn);
bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn);

int IIODeviceInit(int _rxBufferSizeSample,int _txBufferSizeSample,char* _rfportRX,long long _bwRX,long long _fsRX,long long _loRX, char* _rfportTX, long long _bwTX, long long _fsTX, long long _loTX);
void IIODeviceDelete();

char* receiveBuffer();
int sendBuffer(char* buffer);
int sendBufferFast();
char* getTXBufferPointer();

int setRFPort(const char* _rfport, enum iodev type);
int setBW(long long _bw_hz, enum iodev type);
int setFS(long long _fs_hz, enum iodev type);
int setLO(long long _lo_hz, enum iodev type);
int setGain(enum iodev d, long long gain);

int getRFPort(enum iodev d, char* res);
int getBW(enum iodev d, long long* res);
int getFS(enum iodev d,long long* res);
int getLO(enum iodev d, long long* res);
int getGain(enum iodev d, long long* res);

bool isStreaming(enum iodev d);

int enableChannels(enum iodev d);
void disableChannels(enum iodev d);

#endif
