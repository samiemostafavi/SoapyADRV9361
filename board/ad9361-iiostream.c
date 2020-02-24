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

#define TX_BUFF_SIZE 15*1024
#define RX_BUFF_SIZE 15*1024

#define TX_BUFF_SIZE_BYTE TX_BUFF_SIZE*4
#define RX_BUFF_SIZE_BYTE RX_BUFF_SIZE*4

#define SERV_PORT 50707
#define SERV_IP "10.0.9.202"

void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

typedef struct
{
        int receiving_active;
        int sending_active;
	int timedif_active;
        long long sent_count;
        long long recv_count;
        struct sockaddr_in ServAddr;
        struct sockaddr_in ClntAddr;
	int sock;
	pthread_t recv_thread;
	pthread_t tdif_thread;
	bool recv_first;
} handler;
	
time_t sec_begin, sec_end, sec_elapsed;

static handler* phandler;

//Buffers
char tx_buff[TX_BUFF_SIZE_BYTE];
char rx_buff[RX_BUFF_SIZE_BYTE];

struct timeval tv;

int send_msg()
{
	int sndlen = send(phandler->sock,tx_buff, TX_BUFF_SIZE_BYTE, 0);
	//if(sndlen > 0 &&  phandler->recv_first)
	if(sndlen > 0)
		phandler->sent_count+=sndlen;
   
	//printf("Sent %d kB\n",sndlen/1024);
	return sndlen;	
}

int recv_msg()
{
	int rsplen = recv(phandler->sock,rx_buff,RX_BUFF_SIZE_BYTE, 0);
	if(rsplen>0)
		phandler->recv_count+=rsplen;

	//printf("Received %lld kB\n",recvcount/1024);
	return rsplen;
}

/* RX is input, TX is output */
enum iodev { RX, TX };

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
};
/* static scratch mem for strings */
static char tmpstr[64];

/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *rx0_q = NULL;
static struct iio_channel *tx0_i = NULL;
static struct iio_channel *tx0_q = NULL;
static struct iio_buffer  *rxbuf = NULL;
static struct iio_buffer  *txbuf = NULL;


/* cleanup and exit */
void shutdowniio(int s)
{
	// Stop receive thread
	phandler->receiving_active = 0;
	phandler->sending_active = 0;
	pthread_join(phandler->recv_thread, NULL);
	
	// Timing calculation
	sec_end=time(NULL);
	sec_elapsed=sec_end-sec_begin;
	
	// Stop timedif thread
	phandler->timedif_active = 0;
	pthread_join(phandler->tdif_thread, NULL);
	
	// Print the report
	printf("Elapsed time: %lu s\nUDP bytes received %llu MB, UDP received bandwidth %llu Mbps \n", sec_elapsed,phandler->recv_count/1024/1024,phandler->recv_count*8/sec_elapsed/1024/1024);
	printf("UDP bytes sent %llu MB, UDP sent bandwidth %llu Mbps \n", phandler->sent_count/1024/1024,phandler->sent_count*8/sec_elapsed/1024/1024);

	printf("Destroying the socket\n");
	close(phandler->sock);

	printf("Destroying buffers\n");
	if (rxbuf) { iio_buffer_destroy(rxbuf); }
	if (txbuf) { iio_buffer_destroy(txbuf); }

	printf("Disabling streaming channels\n");
	if (rx0_i) { iio_channel_disable(rx0_i); }
	if (rx0_q) { iio_channel_disable(rx0_q); }
	if (tx0_i) { iio_channel_disable(tx0_i); }
	if (tx0_q) { iio_channel_disable(tx0_q); }


	printf("Destroying context\n");
	if (ctx) 
		iio_context_destroy(ctx); 
	
	free(phandler);
	exit(0);
}

/* check return value of attr_write function */
static void errchk(int v, const char* what) {
	 if (v < 0) { fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what); shutdowniio(0); }
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	errchk(iio_channel_attr_write(chn, what, str), what);
}

/* helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
	snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
	return tmpstr;
}

/* returns ad9361 phy device */
static struct iio_device* get_ad9361_phy(struct iio_context *ctx)
{
	struct iio_device *dev =  iio_context_find_device(ctx, "ad9361-phy");
	assert(dev && "No ad9361-phy found");
	return dev;
}

/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
	switch (d) {
	case TX: *dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc"); return *dev != NULL;
	case RX: *dev = iio_context_find_device(ctx, "cf-ad9361-lpc");  return *dev != NULL;
	default: assert(0); return false;
	}
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
	if (!*chn)
		*chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
	switch (d) {
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), true);  return *chn != NULL;
	default: assert(0); return false;
	}
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn)
{
	switch (d) {
	 // LO chan is always output, i.e. true
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 0), true); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 1), true); return *chn != NULL;
	default: assert(0); return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad9361_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	// Configure phy and lo channels
//	printf("* Acquiring AD9361 phy channel %d\n", chid);
	if (!get_phy_chan(ctx, type, chid, &chn)) {	return false; }
	wr_ch_str(chn, "rf_port_select",     cfg->rfport);
	wr_ch_lli(chn, "rf_bandwidth",       cfg->bw_hz);
	wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);

	// Configure LO channel
//	printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
	if (!get_lo_chan(ctx, type, &chn)) { return false; }
	wr_ch_lli(chn, "frequency", cfg->lo_hz);
	return true;
}

// receive thread
void* receive_from_server(void* arg)
{
	// Listen to ctrl+c and assert
	signal(SIGINT, shutdowniio);
	
	struct iio_device *tx;		// Streaming devices
	struct stream_cfg txcfg;	// Stream configurations

	// TX stream config
	txcfg.bw_hz = MHZ(4.373); 		// 1.5 MHz rf bandwidth
	txcfg.fs_hz = MHZ(1.92);  	// 2 MS/s tx sample rate
	txcfg.lo_hz = GHZ(2.560005); 	// 2.5 GHz rf frequency
	txcfg.rfport = "A"; 		// port A (select for rf freq.)
	
	assert((ctx = iio_create_default_context()) && "No context");
	assert(iio_context_get_devices_count(ctx) > 0 && "No devices");
	assert(get_ad9361_stream_dev(ctx, TX, &tx) && "No tx dev found");
	assert(cfg_ad9361_streaming_ch(ctx, &txcfg, TX, 0) && "TX port 0 not found");
	assert(get_ad9361_stream_ch(ctx, TX, tx, 0, &tx0_i) && "TX chan i not found");
	assert(get_ad9361_stream_ch(ctx, TX, tx, 1, &tx0_q) && "TX chan q not found");
	iio_channel_enable(tx0_i);
	iio_channel_enable(tx0_q);
	txbuf = iio_device_create_buffer(tx, RX_BUFF_SIZE, false);
	
	ssize_t nbytes_tx=0;
	void  *t_dat;

	printf("Receiving from server...\n");
	while(phandler->receiving_active)
      	{
		// Fill rx_buff from network
		if(recv_msg()>0)
		{
			// Advertise the connection
			phandler->recv_first = true;

			// Send rx_buff to ad9361
			t_dat = iio_buffer_first(txbuf,tx0_i);
			memcpy(t_dat,rx_buff,RX_BUFF_SIZE_BYTE);
			nbytes_tx = iio_buffer_push(txbuf);
			if (nbytes_tx < 0) 
			{ 
				printf("Error pushing the buffer to ad9361 %d\n", (int) nbytes_tx); 
				shutdowniio(0); 
			}
		}
		
	}
	return NULL;
}

/* simple configuration and streaming */
int main (int argc, char **argv)
{

	// Streaming devices
	struct iio_device *rx;
	// Stream configurations
	struct stream_cfg rxcfg;

	// Listen to ctrl+c and assert
	signal(SIGINT, shutdowniio);

	// Allocate handler
	phandler = malloc(sizeof(handler));
        phandler->receiving_active = 1;
        phandler->sending_active = 1;
        phandler->timedif_active = 1;
        phandler->recv_count = 0;
        phandler->sent_count = 0;
	phandler->recv_first = false;

	// RX stream config
	rxcfg.bw_hz = MHZ(4.695);   // 2 MHz rf bandwidth		//200KHz-56MHz Channel bandwidths 
	rxcfg.fs_hz = MHZ(1.96);   // 2 MS/s rx sample rate	//	61.44
	rxcfg.lo_hz = GHZ(2.680005); // 2.5 GHz rf frequency		//70MHz -6 GHz Local-oscilator
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

	// IIO stuff
	assert((ctx = iio_create_default_context()) && "No context");
	assert(iio_context_get_devices_count(ctx) > 0 && "No devices");
	assert(get_ad9361_stream_dev(ctx, RX, &rx) && "No rx dev found");
	assert(cfg_ad9361_streaming_ch(ctx, &rxcfg, RX, 0) && "RX port 0 not found");
	assert(get_ad9361_stream_ch(ctx, RX, rx, 0, &rx0_i) && "RX chan i not found");
	assert(get_ad9361_stream_ch(ctx, RX, rx, 1, &rx0_q) && "RX chan q not found");
	iio_channel_enable(rx0_i);
	iio_channel_enable(rx0_q);
	rxbuf = iio_device_create_buffer(rx, TX_BUFF_SIZE, false);

	/* Construct the server address structure */
	struct sockaddr_in serv; 				/* Echo server address */
	memset(&serv, 0, sizeof(serv));    			/* Zero out structure */
	serv.sin_family = AF_INET;                 		/* Internet addr family */
	serv.sin_addr.s_addr = inet_addr(SERV_IP);  	/* Server IP address */
	serv.sin_port = htons(SERV_PORT);			/* Server port */

    	/* Create a datagram/UDP socket */
	if ((phandler->sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		DieWithError("socket creation failed");

	/*Set waiting limit*/
	tv.tv_sec = 2;
	if (setsockopt(phandler->sock, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
		DieWithError("Error");

	/* Connecting to the server */
	if (connect(phandler->sock,(struct sockaddr*) &serv, sizeof(serv)) < 0)
		DieWithError("socket connection failed");
	
	// Start the receiving thread
	if(pthread_create(&phandler->recv_thread, NULL, &receive_from_server,NULL)) 
		DieWithError("starting reception thread failed");
	

	printf("Sending to server...\n");

	ssize_t nbytes_rx=0;
	void *p_dat;

	sec_begin=time(NULL);
	//while(sec_elapsed<40 && phandler->sending_active)
	while(phandler->sending_active)
	{
		// Timings
		sec_end=time(NULL);
		sec_elapsed=sec_end-sec_begin;
	
		// Fill buffer from antena
		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) 
		{ 
			printf("Error refilling buf from ad9361 %d\n",(int) nbytes_rx); 
			shutdowniio(0); 
		}

		// Send the buffer to the UDP socket
		p_dat = iio_buffer_first(rxbuf,rx0_i);
		memcpy (tx_buff, p_dat, TX_BUFF_SIZE_BYTE);
		send_msg();
	}

	shutdowniio(0);	
	return 0;
}




