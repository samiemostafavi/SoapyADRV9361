#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <algorithm>
#include <math.h>

#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Time.h>
#include <SoapySDR/Version.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <mutex>


using namespace std::literals;
using clock_type = std::chrono::high_resolution_clock;

#define HAVE_ASYNC_THREAD 1

#define PRINT_RX_STATS 0
#define PRINT_TX_STATS 0

using namespace std;

class TimeKeeper
{
	public:
	TimeKeeper();
	void setTimeNs(uint64_t inp)
	{
		mtx.lock();
		timeNs = inp;
  		mtx.unlock();
	}
	uint64_t getTimeNs()
	{ 
		mtx.lock();
		uint64_t res = timeNs;
  		mtx.unlock();
		return res;
	}
	private:
	uint64_t timeNs;
	mutex mtx;
};

typedef struct {
  char*            devname;
  SoapySDRKwargs   args;
  SoapySDRDevice*  device;
  SoapySDRRange*   ranges;
  SoapySDRStream*  rxStream;
  SoapySDRStream*  txStream;
  bool             tx_stream_active;
  bool             rx_stream_active;
  double           tx_rate;
  size_t           rx_mtu, tx_mtu;
  size_t           num_rx_channels;
  size_t           num_tx_channels;

  void*            soapy_error_handler_arg;

  TimeKeeper*	   rxTimeKeeper;

  bool      async_thread_running;
  pthread_t async_thread;
  
  bool      rx_thread_running;
  pthread_t rx_thread;
  
  bool      tx_thread_running;
  pthread_t tx_thread;

  uint32_t num_time_errors;
  uint32_t num_lates;
  uint32_t num_overflows;
  uint32_t num_underflows;
  uint32_t num_other_errors;
  uint32_t num_stream_curruption;
} rf_soapy_handler_t;

typedef _Complex float cf_t;
cf_t zero_mem[64 * 1024];

#if HAVE_ASYNC_THREAD
static void* async_thread(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;

  while (handler->async_thread_running) {
    int        ret       = 0;
    size_t     chanMask  = 0;
    int        flags     = 0;
    const long timeoutUs = 400000; // arbitrarily chosen
    long long  timeNs;

    ret = SoapySDRDevice_readStreamStatus(handler->device, handler->txStream, &chanMask, &flags, &timeNs, timeoutUs);
    /*if (ret == SOAPY_SDR_TIME_ERROR) {
      // this is a late
      log_late(handler, false);
    } else if (ret == SOAPY_SDR_UNDERFLOW) {
      log_underflow(handler);
    } else if (ret == SOAPY_SDR_OVERFLOW) {
      log_overflow(handler);
    } else if (ret == SOAPY_SDR_TIMEOUT) {
      // this is a timeout of the readStreamStatus call, ignoring it ..
    } else if (ret == SOAPY_SDR_NOT_SUPPORTED) {
      // stopping async thread
      ERROR("Receiving async metadata not supported by device. Exiting thread.\n");
      handler->async_thread_running = false;
    } else {
      ERROR("Error while receiving aync metadata: %s (%d), flags=%d, channel=%zu, timeNs=%lld\n",
            SoapySDR_errToStr(ret),
            ret,
            flags,
            chanMask,
            timeNs);
      handler->async_thread_running = false;
    }*/
  }
  return NULL;
}
#endif



double rf_soapy_set_rx_srate(void* h, double rate)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;

  for (uint32_t i = 0; i < handler->num_rx_channels; i++) {
    if (SoapySDRDevice_setSampleRate(handler->device, SOAPY_SDR_RX, i, rate) != 0) {
      printf("setSampleRate Rx fail: %s\n", SoapySDRDevice_lastError());
      return -1;
    }

#if SET_RF_BW
    // Set bandwidth close to current rate
    size_t         bw_length;
    SoapySDRRange* bw_range = SoapySDRDevice_getBandwidthRange(handler->device, SOAPY_SDR_RX, 0, &bw_length);
    for (int k = 0; k < bw_length; ++k) {
      double bw = rate * 0.75;
      bw        = SRSLTE_MIN(bw, bw_range[k].maximum);
      bw        = SRSLTE_MAX(bw, bw_range[k].minimum);
      if (SoapySDRDevice_setBandwidth(handler->device, SOAPY_SDR_RX, i, bw) != 0) {
        printf("setBandwidth fail: %s\n", SoapySDRDevice_lastError());
        return -1;
      }
      printf("Set Rx bandwidth to %.2f MHz\n", SoapySDRDevice_getBandwidth(handler->device, SOAPY_SDR_RX, i) / 1e6);
    }
#endif // SET_RF_BW
  }

  // retrun sample rate of first channel
  return SoapySDRDevice_getSampleRate(handler->device, SOAPY_SDR_RX, 0);
}


double rf_soapy_set_tx_srate(void* h, double rate)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;

  for (uint32_t i = 0; i < handler->num_tx_channels; i++) {
    if (SoapySDRDevice_setSampleRate(handler->device, SOAPY_SDR_TX, i, rate) != 0) {
      printf("setSampleRate Tx fail: %s\n", SoapySDRDevice_lastError());
      return -1;
    }

#if SET_RF_BW
    size_t         bw_length;
    SoapySDRRange* bw_range = SoapySDRDevice_getBandwidthRange(handler->device, SOAPY_SDR_TX, i, &bw_length);
    for (int k = 0; k < bw_length; ++k) {
      // try to set the BW a bit narrower than sampling rate to prevent aliasing but make sure to stay within device
      // boundaries
      double bw = rate * 0.75;
      bw        = SRSLTE_MAX(bw, bw_range[k].minimum);
      bw        = SRSLTE_MIN(bw, bw_range[k].maximum);
      if (SoapySDRDevice_setBandwidth(handler->device, SOAPY_SDR_TX, i, bw) != 0) {
        printf("setBandwidth fail: %s\n", SoapySDRDevice_lastError());
        return -1;
      }
      printf("Set Tx bandwidth to %.2f MHz\n", SoapySDRDevice_getBandwidth(handler->device, SOAPY_SDR_TX, i) / 1e6);
    }
#endif // SET_RF_BW
  }

  handler->tx_rate = SoapySDRDevice_getSampleRate(handler->device, SOAPY_SDR_TX, 0);

  return handler->tx_rate;
}


int rf_soapy_start_rx_stream(void* h, bool now)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  if (handler->rx_stream_active == false) {
    if (SoapySDRDevice_activateStream(handler->device, handler->rxStream, 0, 0, 0) != 0) {
      printf("Error starting Rx streaming.\n");
      return -1;
    }

    handler->rx_stream_active = true;
  }
  return 0;
}

int rf_soapy_start_tx_stream(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  if (handler->tx_stream_active == false) {
    if (SoapySDRDevice_activateStream(handler->device, handler->txStream, 0, 0, 0) != 0) {
      printf("Error starting Tx streaming.\n");
      return -1;
    }
    handler->tx_stream_active = true;
  }
  return 0;
}

int rf_soapy_stop_rx_stream(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  if (SoapySDRDevice_deactivateStream(handler->device, handler->rxStream, 0, 0) != 0) {
    printf("Error deactivating Rx streaming.\n");
    return -1;
  }

  handler->rx_stream_active = false;
  return 0;
}

int rf_soapy_stop_tx_stream(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  if (SoapySDRDevice_deactivateStream(handler->device, handler->txStream, 0, 0) != 0) {
    printf("Error deactivating Tx streaming.\n");
    return -1;
  }

  handler->tx_stream_active = false;
  return 0;
}


int rf_soapy_open_multi(char* args, void** h, uint32_t num_requested_channels)
{
  size_t          length;
  SoapySDRKwargs* soapy_args = SoapySDRDevice_enumerate(NULL, &length);

  if (length == 0) {
    printf("No Soapy devices found.\n");
    SoapySDRKwargsList_clear(soapy_args, length);
    return -1;
  }
  char* devname;
  for (size_t i = 0; i < length; i++) {
    printf("Soapy has found device #%d: ", (int)i);
    for (size_t j = 0; j < soapy_args[i].size; j++) {
      printf("%s=%s, ", soapy_args[i].keys[j], soapy_args[i].vals[j]);
    }
    printf("\n");
  }

  // Select Soapy device by id
  int dev_id = 0;
  
  // select last device if dev_id exceeds available devices
  dev_id = min((int)dev_id,(int)length - 1);
  printf("Selecting Soapy device: %d\n", dev_id);

  SoapySDRDevice* sdr = SoapySDRDevice_make(&(soapy_args[dev_id]));
  if (sdr == NULL) {
    printf("Failed to create Soapy object\n");
    return -1;
  }
  SoapySDRKwargsList_clear(soapy_args, length);

  // create handler
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)malloc(sizeof(rf_soapy_handler_t));
  bzero(handler, sizeof(rf_soapy_handler_t));
  *h                        = handler;
  handler->rxTimeKeeper = (TimeKeeper*)malloc(sizeof(TimeKeeper));
  handler->device           = sdr;
  handler->tx_stream_active = false;
  handler->rx_stream_active = false;
  handler->devname          = devname;

  // create stream args from device args
  SoapySDRKwargs stream_args = {};
#if SOAPY_SDR_API_VERSION >= 0x00060000
  stream_args = SoapySDRKwargs_fromString(args);
#endif

  // Setup Rx streamer
  size_t num_available_channels = SoapySDRDevice_getNumChannels(handler->device, SOAPY_SDR_RX);
  if ((num_available_channels > 0) && (num_requested_channels > 0)) {
    handler->num_rx_channels = min((int)num_available_channels, (int)num_requested_channels);
    size_t rx_channels[handler->num_rx_channels];
    for (int i = 0; i < handler->num_rx_channels; i++) {
      rx_channels[i] = i;
    }
    printf("Setting up Rx stream with %zd channel(s)\n", handler->num_rx_channels);
#if SOAPY_SDR_API_VERSION < 0x00080000
    if (SoapySDRDevice_setupStream(handler->device,
                                   &handler->rxStream,
                                   SOAPY_SDR_RX,
                                   SOAPY_SDR_CF32,
                                   rx_channels,
                                   handler->num_rx_channels,
                                   &stream_args) != 0) {
#else
    handler->rxStream = SoapySDRDevice_setupStream(
        handler->device, SOAPY_SDR_RX, SOAPY_SDR_CF32, rx_channels, handler->num_rx_channels, &stream_args);
    if (handler->rxStream == NULL) {
#endif
      printf("Rx setupStream fail: %s\n", SoapySDRDevice_lastError());
      return -1;
    }
    handler->rx_mtu = SoapySDRDevice_getStreamMTU(handler->device, handler->rxStream);
  }

  // Setup Tx streamer
  num_available_channels = SoapySDRDevice_getNumChannels(handler->device, SOAPY_SDR_TX);
  if ((num_available_channels > 0) && (num_requested_channels > 0)) {
    handler->num_tx_channels = min((int)num_available_channels, (int)num_requested_channels);
    size_t tx_channels[handler->num_tx_channels];
    for (int i = 0; i < handler->num_tx_channels; i++) {
      tx_channels[i] = i;
    }
    printf("Setting up Tx stream with %zd channel(s)\n", handler->num_tx_channels);
#if SOAPY_SDR_API_VERSION < 0x00080000
    if (SoapySDRDevice_setupStream(handler->device,
                                   &handler->txStream,
                                   SOAPY_SDR_TX,
                                   SOAPY_SDR_CF32,
                                   tx_channels,
                                   handler->num_tx_channels,
                                   &stream_args) != 0) {
#else
    handler->txStream = SoapySDRDevice_setupStream(
        handler->device, SOAPY_SDR_TX, SOAPY_SDR_CF32, tx_channels, handler->num_tx_channels, &stream_args);
    if (handler->txStream == NULL) {
#endif
      printf("Tx setupStream fail: %s\n", SoapySDRDevice_lastError());
      return -1;
    }
    handler->tx_mtu = SoapySDRDevice_getStreamMTU(handler->device, handler->txStream);
  }

  // init rx/tx rate to lowest LTE rate to avoid decimation warnings
  rf_soapy_set_rx_srate(handler, 1.92e6);
  rf_soapy_set_tx_srate(handler, 1.92e6);
  
  // list device sensors
  size_t list_length;
  char** list;

  // list channel sensors
  for (uint32_t i = 0; i < handler->num_rx_channels; ++i) {
    list = SoapySDRDevice_listChannelSensors(handler->device, SOAPY_SDR_RX, i, &list_length);
    printf("Available sensors for Rx channel %d: \n", i);
    for (int j = 0; j < list_length; j++) {
      printf(" - %s\n", list[j]);
    }
  }
  // Set static radio info
  SoapySDRRange tx_range    = SoapySDRDevice_getGainRange(handler->device, SOAPY_SDR_TX, 0);
  SoapySDRRange rx_range    = SoapySDRDevice_getGainRange(handler->device, SOAPY_SDR_RX, 0);

#if HAVE_ASYNC_THREAD
  bool start_async_thread = true;
  if (args) {
    if (strstr(args, "silent")) {
      //REMOVE_SUBSTRING_WITHCOMAS(args, "silent");
      start_async_thread = false;
    }
  }
#endif


  // list gains and AGC mode
  bool has_agc = SoapySDRDevice_hasGainMode(handler->device, SOAPY_SDR_RX, 0);
  list         = SoapySDRDevice_listGains(handler->device, SOAPY_SDR_RX, 0, &list_length);
  printf("State of gain elements for Rx channel 0 (AGC %s):\n", has_agc ? "supported" : "not supported");
  for (int i = 0; i < list_length; i++) {
    printf(" - %s: %.2f dB\n", list[i], SoapySDRDevice_getGainElement(handler->device, SOAPY_SDR_RX, 0, list[i]));
  }

  has_agc = SoapySDRDevice_hasGainMode(handler->device, SOAPY_SDR_TX, 0);
  list    = SoapySDRDevice_listGains(handler->device, SOAPY_SDR_TX, 0, &list_length);
  printf("State of gain elements for Tx channel 0 (AGC %s):\n", has_agc ? "supported" : "not supported");
  for (int i = 0; i < list_length; i++) {
    printf(" - %s: %.2f dB\n", list[i], SoapySDRDevice_getGainElement(handler->device, SOAPY_SDR_TX, 0, list[i]));
  }

  // print actual antenna configuration
  char* ant = SoapySDRDevice_getAntenna(handler->device, SOAPY_SDR_RX, 0);
  printf("Rx antenna set to %s\n", ant);

  ant = SoapySDRDevice_getAntenna(handler->device, SOAPY_SDR_TX, 0);
  printf("Tx antenna set to %s\n", ant);

#if HAVE_ASYNC_THREAD
  if (start_async_thread) {
    // Start low priority thread to receive async commands
    handler->async_thread_running = true;
    if (pthread_create(&handler->async_thread, NULL, async_thread, handler)) {
      perror("pthread_create");
      return -1;
    }
  }
#endif

  return 0;
}

int rf_soapy_open(char* args, void** h)
{
  return rf_soapy_open_multi(args, h, 1);
}

int rf_soapy_close(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;

#if HAVE_ASYNC_THREAD
  if (handler->async_thread_running) {
    handler->async_thread_running = false;
    pthread_join(handler->async_thread, NULL);
  }
#endif

  if (handler->tx_stream_active) {
    rf_soapy_stop_tx_stream(handler);
    SoapySDRDevice_closeStream(handler->device, handler->txStream);
  }
  
  if (handler->rx_stream_active) {
    rf_soapy_stop_rx_stream(handler);
    SoapySDRDevice_closeStream(handler->device, handler->rxStream);
  }

  SoapySDRDevice_unmake(handler->device);

  // print statistics
  if (handler->num_lates)
    printf("#lates=%d\n", handler->num_lates);
  if (handler->num_overflows)
    printf("#overflows=%d\n", handler->num_overflows);
  if (handler->num_underflows)
    printf("#underflows=%d\n", handler->num_underflows);
  if (handler->num_time_errors)
    printf("#time_errors=%d\n", handler->num_time_errors);
  if (handler->num_other_errors)
    printf("#other_errors=%d\n", handler->num_other_errors);

  free(handler);

  return 0;
}

#define MAX_PORTS 4

uint64_t timeNsOld = 0;

int rf_soapy_recv_with_time_multi(void*    h,
                                  void*    data[MAX_PORTS],
                                  uint32_t nsamples,
                                  bool     blocking,
                                  time_t*  secs,
                                  double*  frac_secs)
{
  rf_soapy_handler_t* handler   = (rf_soapy_handler_t*)h;
  int                 flags     = 0;      // flags set by receive operation
  const long          timeoutUs = 400000; // arbitrarily chosen

  int       trials = 0;
  int       ret    = 0;
  long long timeNs; // timestamp for receive buffer
  int       n = 0;

#if PRINT_RX_STATS
  printf("rx: nsamples=%d rx_mtu=%zd\n", nsamples, handler->rx_mtu);
#endif

  do {
    size_t rx_samples = min((int)nsamples - n, (int)handler->rx_mtu);
#if PRINT_RX_STATS
    printf(" - rx_samples=%zd\n", rx_samples);
#endif

    void* buffs_ptr[MAX_PORTS] = {};
    for (int i = 0; i < handler->num_rx_channels; i++) {
      cf_t* data_c = (cf_t*)data[i];
      buffs_ptr[i] = &data_c[n];
    }
    ret = SoapySDRDevice_readStream(
        handler->device, handler->rxStream, buffs_ptr, rx_samples, &flags, &timeNs, timeoutUs);
  
    if (ret == SOAPY_SDR_OVERFLOW || (ret > 0 && (flags & SOAPY_SDR_END_ABRUPT) != 0)) {
      //log_overflow(handler);
      continue;
    } else if (ret == SOAPY_SDR_TIMEOUT) {
      //log_late(handler, true);
      continue;
    } else if (ret < 0) {
      // unspecific error
      printf("SoapySDRDevice_readStream returned %d: %s\n", ret, SoapySDR_errToStr(ret));
      handler->num_other_errors++;
    }

    // update rx time only for first segment
    if (secs != NULL && frac_secs != NULL && n == 0) {
      *secs      = floor(timeNs / 1e9);
      *frac_secs = (timeNs % 1000000000) / 1e9;
      //printf("rx_time: secs=%ld, frac_secs=%lf timeNs=%llu\n", *secs, *frac_secs, timeNs);

    }

#if PRINT_RX_STATS
    printf(" - rx: %d/%zd\n", ret, rx_samples);
#endif

    n += ret;
    trials++;
  } while (n < nsamples && trials < 100);

  return n;
}

int rf_soapy_recv_with_time(void* h, void* data, uint32_t nsamples, bool blocking, time_t* secs, double* frac_secs)
{
  return rf_soapy_recv_with_time_multi(h, &data, nsamples, blocking, secs, frac_secs);
}

double prev_ts = 0;
int prev_size = 0;
bool print_next = false;
unsigned long prev_time = 0;

// Todo: Check correct handling of flags, use RF metrics API, fix timed transmissions
int rf_soapy_send_timed_multi(void*  h,
                              void*  data[MAX_PORTS],
                              int    nsamples,
                              time_t secs,
                              double frac_secs,
                              bool   has_time_spec,
                              bool   blocking,
                              bool   is_start_of_burst,
                              bool   is_end_of_burst)
{
  rf_soapy_handler_t* handler   = (rf_soapy_handler_t*)h;
  int                 flags     = 0;
  const long          timeoutUs = 100000; // arbitrarily chosen
  long long           timeNs    = 0;
  int                 trials    = 0;
  int                 ret       = 0;
  int                 n         = 0;

  // Uncomment this to see exactly how the framse get transmitted 
  /*struct timeval tv;
  gettimeofday(&tv,NULL);
  unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;
  if((nsamples != 1920) || print_next)
  {
    printf("rx: nsamples=%d timens=%f, curr_time_us:%lu, prev_nsamples=%d, prev_timens=%f, prev_time_us=%lu\n", nsamples, frac_secs, time_in_micros, prev_size ,prev_ts, prev_time);

    if(nsamples != 1920)
	    print_next = true;
    else
	    print_next = false;
  }

  prev_ts = frac_secs;
  prev_size = nsamples;
  prev_time = time_in_micros;*/

#if PRINT_TX_STATS
  printf("tx: namples=%d, mtu=%zd\n", nsamples, handler->tx_mtu);
#endif

  if (!handler->tx_stream_active) {
    rf_soapy_start_tx_stream(h);
  }

  // Convert initial tx time
  if (has_time_spec) {
    timeNs = (long long)secs * 1000000000;
    timeNs = timeNs + (frac_secs * 1000000000);
  }
  
  if(timeNs==timeNsOld)
  {
    //cout << timeNs - timeNsOld << endl;
    timeNs = timeNsOld + 1000000;
  }
  
  if( (timeNs < timeNsOld) && (timeNsOld != 0) && (timeNsOld-timeNs < 100000000))
  {
    //cout << timeNs << " " << timeNsOld << " " << (int64_t)(timeNs - timeNsOld) << endl;
    timeNs = timeNsOld + 1000000;
  }
  timeNsOld = timeNs;


  do {
#if USE_TX_MTU
    size_t tx_samples = min(nsamples - n, handler->tx_mtu);
#else
    size_t tx_samples = nsamples;
    if (tx_samples > nsamples - n) {
      tx_samples = nsamples - n;
    }
#endif

    // (re-)set stream flags
    flags = 0;
    if (is_start_of_burst && is_end_of_burst) {
      flags |= SOAPY_SDR_ONE_PACKET;
    }

    if (is_end_of_burst) {
      flags |= SOAPY_SDR_END_BURST;
    }

    // only set time flag for first tx
    if (has_time_spec && n == 0) {
      flags |= SOAPY_SDR_HAS_TIME;
    }

#if PRINT_TX_STATS
    printf(" - tx_samples=%zd at timeNs=%llu flags=%d\n", tx_samples, timeNs, flags);
#endif

    const void* buffs_ptr[MAX_PORTS] = {};
    for (int i = 0; i < handler->num_tx_channels; i++) {
      cf_t* data_c = (cf_t*)data[i] ? (cf_t*)data[i] : zero_mem;
      buffs_ptr[i] = &data_c[n];
    }
      
    ret = SoapySDRDevice_writeStream(
        handler->device, handler->txStream, buffs_ptr, tx_samples, &flags, timeNs, timeoutUs);
    if (ret >= 0) {
      // Tx was ok
#if PRINT_TX_STATS
      printf(" - tx: %d/%zd\n", ret, tx_samples);
#endif
      // Advance tx time
      if (has_time_spec && ret < nsamples) {
        long long adv = SoapySDR_ticksToTimeNs(ret, handler->tx_rate);
#if PRINT_TX_STATS
        printf(" - tx: timeNs_old=%llu, adv=%llu, timeNs_new=%llu, tx_rate=%f\n",
               timeNs,
               adv,
               timeNs + adv,
               handler->tx_rate);
#endif
        timeNs += adv;
      }
      n += ret;
    } else if (ret < 0) {
      // An error has occured
      switch (ret) {
        case SOAPY_SDR_TIMEOUT:
          printf("L");
          break;
        case SOAPY_SDR_STREAM_ERROR:
          handler->num_stream_curruption++;
          printf("E");
          break;
        case SOAPY_SDR_TIME_ERROR:
          handler->num_time_errors++;
          printf("T");
          break;
        case SOAPY_SDR_UNDERFLOW:
          printf("U");
          break;
        default:
          printf("Error during writeStream\n");
          exit(-1);
          return -1;
      }
    }
    trials++;
  } while (n < nsamples && trials < 100);

  if (n != nsamples) {
    printf("Couldn't write all samples after %d trials.\n", trials);
  }

  return n;
}

int rf_soapy_send_timed(void*  h,
                        void*  data,
                        int    nsamples,
                        time_t secs,
                        double frac_secs,
                        bool   has_time_spec,
                        bool   blocking,
                        bool   is_start_of_burst,
                        bool   is_end_of_burst)
{
  void* _data[MAX_PORTS] = {data, zero_mem, zero_mem, zero_mem};
  return rf_soapy_send_timed_multi(
      h, _data, nsamples, secs, frac_secs, has_time_spec, blocking, is_start_of_burst, is_end_of_burst);
}

static uint64_t convert_time_to_int(time_t secs,double frac_secs)
{
  uint64_t timeNs;
  timeNs = (long long)secs * 1000000000;
  timeNs = timeNs + (frac_secs * 1000000000);
  return timeNs;
}

static int convert_int_to_time(uint64_t timeNs, time_t* secs,double* frac_secs)
{
  // update rx time only for first segment
  if (secs != NULL && frac_secs != NULL) 
  {
    *secs      = floor(timeNs / 1e9);
    *frac_secs = (timeNs % 1000000000) / 1e9;
    //printf("rx_time: secs=%ld, frac_secs=%lf timeNs=%llu\n", *secs, *frac_secs, timeNs);
    return 0;
  }
  else
    return -1;
}

static void* rx_thread(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  cf_t buffer[1920*8];
  uint32_t nsamples = 1920;
  time_t secs;
  double frac_secs;
  bool blocking = true;

  auto when_started = clock_type::now();
  while (handler->rx_thread_running)
  {
	int ret = rf_soapy_recv_with_time(h, (void*) buffer, nsamples, blocking, &secs, &frac_secs);
	uint64_t rxTimeNs = convert_time_to_int(secs,frac_secs);
	handler->rxTimeKeeper->setTimeNs(rxTimeNs);
  }
  auto now = clock_type::now();
  cout << "Rx stream took: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - when_started).count() << " ms\n";
  return NULL;
}

static void* tx_thread(void* h)
{
  rf_soapy_handler_t* handler = (rf_soapy_handler_t*)h;
  cf_t buffer[1920*8];

  uint32_t nsamples = 1920;
  uint64_t txTimeNs = 0;
  time_t secs;
  double frac_secs;
  bool has_time_spec = true;
  bool blocking = true;
  bool is_start_of_burst = false;
  bool is_end_of_burst = false;
  uint64_t rx_tx_offset = 4000000;

  auto when_started = clock_type::now();
  auto target_time = when_started + 10ms;
  while (handler->tx_thread_running)
  {
	txTimeNs = handler->rxTimeKeeper->getTimeNs() + rx_tx_offset;
	convert_int_to_time(txTimeNs, &secs, &frac_secs);
	int ret = rf_soapy_send_timed(h,(void*)buffer,nsamples,secs,frac_secs,has_time_spec,blocking,is_start_of_burst,is_end_of_burst);
        std::this_thread::sleep_until(target_time);
        target_time += 1ms;
  }
  auto now = clock_type::now();
  cout << "Tx stream took: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - when_started - 10ms).count() << " ms\n";

  return NULL;
}

int main()
{
	rf_soapy_handler_t* handler;
	char* args = NULL;

	rf_soapy_open(args, (void**) &handler);
  
        rf_soapy_start_rx_stream(handler, true);


	handler->rx_thread_running = true;
	if (pthread_create(&handler->rx_thread, NULL, rx_thread, handler))
	{
     		perror("pthread_create");
      		return -1;
    	}

	cout << "rx thread is running..." << endl;
	
	handler->tx_thread_running = true;
	if (pthread_create(&handler->tx_thread, NULL, tx_thread, handler))
	{
     		perror("pthread_create");
      		return -1;
    	}
	
	cout << "tx thread is running..." << endl;

	usleep(600e6);

  	if (handler->rx_thread_running) 
	{
    		handler->rx_thread_running = false;
    		pthread_join(handler->rx_thread, NULL);
  	}
	rf_soapy_stop_rx_stream(handler);
	cout << "rx thread is stopped." << endl;
	
  	if (handler->tx_thread_running) 
	{
    		handler->tx_thread_running = false;
    		pthread_join(handler->tx_thread, NULL);
  	}
	rf_soapy_stop_tx_stream(handler);
	cout << "tx thread is stopped." << endl;
	
	rf_soapy_close(handler);

	return 0;
}

