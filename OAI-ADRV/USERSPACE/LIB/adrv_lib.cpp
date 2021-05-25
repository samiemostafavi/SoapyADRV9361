
/** adrv_lib.cpp
 *
 * \authors: Seyed Samie Mostafavi : ssmos@kth.se
 *
 * Based on IRIS sdr driver for OAI
 *
 */

#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Time.hpp>
#include <SoapySDR/Logger.hpp>

//#include <boost/format.hpp>
#include <iostream>
#include <complex>
#include <fstream>
#include <cmath>
#include <time.h>
#include <limits>
#include "common/utils/LOG/log_extern.h"
#include "common_lib.h"
#include <chrono>
#include <vector>

#ifdef __SSE4_1__
#  include <smmintrin.h>
#endif

#ifdef __AVX2__
#  include <immintrin.h>
#endif

#define MOVE_DC
#define SAMPLE_RATE_DOWN 1

using namespace std;

/*! \brief adrv Configuration */
extern "C" 
{

  typedef struct 
  {

    // --------------------------------
    // variables for adrv configuration
    // --------------------------------
    //! adrv device pointer
    vector<SoapySDR::Device*> adrv;
    int device_num;
    int rx_num_channels;
    int tx_num_channels;
    //create a send streamer and a receive streamer
    //! adrv TX Stream
    vector<SoapySDR::Stream*> txStream;
    //! adrv RX Stream
    vector<SoapySDR::Stream*> rxStream;

    //! Sampling rate
    double sample_rate;

    //! time offset between transmiter timestamp and receiver timestamp;
    double tdiff;

    //! TX forward samples.
    int tx_forward_nsamps; //166 for 20Mhz


    // --------------------------------
    // Debug and output control
    // --------------------------------
    //! Number of underflows
    int num_underflows;
    //! Number of overflows
    int num_overflows;

    //! Number of sequential errors
    int num_seq_errors;
    //! tx count
    int64_t tx_count;
    //! rx count
    int64_t rx_count;
    //! timestamp of RX packet
    openair0_timestamp rx_timestamp;

  } adrv_state_t;
}

/*! \brief Called to start the Adrv transceiver. Return 0 if OK, < 0 if error
    @param device pointer to the device structure specific to the RF hardware target
*/
static int trx_adrv_start(openair0_device *device) 
{
    adrv_state_t *s = (adrv_state_t *) device->priv;

    long long timeNs = s->adrv[0]->getHardwareTime("") + 500000;
    int flags = 0;
    //flags |= SOAPY_SDR_HAS_TIME;
    int r;
    for (r = 0; r < s->device_num; r++) 
    {
        int ret = s->adrv[r]->activateStream(s->rxStream[r], flags, timeNs, 0);
        int ret2 = s->adrv[r]->activateStream(s->txStream[r]);
        if (ret < 0 | ret2 < 0)
            return -1;
    }
    return 0;
}

/*! \brief Stop adrv
 * \param card refers to the hardware index to use
 */
int trx_adrv_stop(openair0_device *device) 
{
    adrv_state_t *s = (adrv_state_t *) device->priv;
    int r;
    for (r = 0; r < s->device_num; r++) 
    {
        s->adrv[r]->deactivateStream(s->txStream[r]);
        s->adrv[r]->deactivateStream(s->rxStream[r]);
    }
    return (0);
}

/*! \brief Terminate operation of the adrv transceiver -- free all associated resources
 * \param device the hardware to use
 */
static void trx_adrv_end(openair0_device *device) 
{
    LOG_I(HW, "Closing adrv device.\n");
    trx_adrv_stop(device);
    adrv_state_t *s = (adrv_state_t *) device->priv;
    int r;
    for (r = 0; r < s->device_num; r++) 
    {
        s->adrv[r]->closeStream(s->txStream[r]);
        s->adrv[r]->closeStream(s->rxStream[r]);
	SoapySDR::Device::unmake(s->adrv[r]);
    }
}
	
/*! \brief Called to send samples to the adrv RF target
      @param device pointer to the device structure specific to the RF hardware target
      @param timestamp The timestamp at whicch the first sample MUST be sent
      @param buff Buffer which holds the samples
      @param nsamps number of samples to be sent
      @param antenna_id index of the antenna if the device has multiple anteannas
      @param flags flags must be set to TRUE if timestamp parameter needs to be applied
*/
static int trx_adrv_write(openair0_device *device, openair0_timestamp timestamp, void **buff, int nsamps, int cc, int flags) 
{
    using namespace std::chrono;
    static long long int loop = 0;
    static long time_min = 0, time_max = 0, time_avg = 0;
    struct timespec tp_start, tp_end;
    long time_diff;

    int ret = 0, ret_i = 0;
    int flag = 0;

    adrv_state_t *s = (adrv_state_t *) device->priv;
    int nsamps2;  // aligned to upper 32 or 16 byte boundary
#if defined(__x86_64) || defined(__i386__)
  #ifdef __AVX2__
    nsamps2 = (nsamps+7)>>3;
    __m256i buff_tx[2][nsamps2];
  #else
    nsamps2 = (nsamps+3)>>2;
    __m128i buff_tx[2][nsamps2];
  #endif
#else
  #error unsupported CPU architecture, adrv device cannot be built
#endif

    // bring RX data into 12 LSBs for softmodem RX
    for (int i=0; i<cc; i++) 
    {
      for (int j=0; j<nsamps2; j++) 
      {
#if defined(__x86_64__) || defined(__i386__)
#ifdef __AVX2__
        buff_tx[i][j] = _mm256_slli_epi16(((__m256i *)buff[i])[j],4);
#else
        buff_tx[i][j] = _mm_slli_epi16(((__m128i *)buff[i])[j],4);
#endif
#endif
      }
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp_start);

    // This hack was added by cws to help keep packets flowing

    if (flags)
        flag |= SOAPY_SDR_HAS_TIME;
    else 
    {
        long long tempHack = s->adrv[0]->getHardwareTime("");
        return nsamps;
    }

    if (flags == 2 || flags == 1) 
    {
        // start of burst
    } 
    else if (flags == 3 | flags == 4) 
    {
        flag |= SOAPY_SDR_END_BURST;
    }



    long long timeNs = SoapySDR::ticksToTimeNs(timestamp, s->sample_rate);
    uint32_t *samps[2]; //= (uint32_t **)buff;
    int r;
    int m = s->tx_num_channels;
    for (r = 0; r < s->device_num; r++) 
    {
        int samples_sent = 0;
        samps[0] = (uint32_t *) buff_tx[m * r];

        if (cc % 2 == 0)
            samps[1] = (uint32_t *) buff_tx[m * r + 1]; //cws: it seems another thread can clobber these, so we need to save them locally.
#ifdef ADRV_DEBUG
        int i;
        for (i = 200; i < 216; i++)
            printf("%d, ",((int16_t)(samps[0][i]>>16))>>4);
        printf("\n");
        //printf("\nHardware time before write: %lld, tx_time_stamp: %lld\n", s->adrv[0]->getHardwareTime(""), timeNs);
#endif
	ret = s->adrv[r]->writeStream(s->txStream[r], (void **) samps, (size_t)(nsamps), flag, timeNs, 1000000);
	//ret = nsamps;

        if (ret < 0)
            printf("Unable to write stream!\n");
        else
            samples_sent = ret;


        if (samples_sent != nsamps)
            printf("[xmit] tx samples %d != %d\n", samples_sent, nsamps);

    }

    return nsamps;
}

/*! \brief Receive samples from hardware.
 * Read \ref nsamps samples from each channel to buffers. buff[0] is the array for
 * the first channel. *ptimestamp is the time at which the first sample
 * was received.
 * \param device the hardware to use
 * \param[out] ptimestamp the time at which the first sample was received.
 * \param[out] buff An array of pointers to buffers for received samples. The buffers must be large enough to hold the number of samples \ref nsamps.
 * \param nsamps Number of samples. One sample is 2 byte I + 2 byte Q => 4 byte.
 * \param antenna_id Index of antenna for which to receive samples
 * \returns the number of sample read
*/
static int trx_adrv_read(openair0_device *device, openair0_timestamp *ptimestamp, void **buff, int nsamps, int cc) 
{
    //if(nsamps>7680)
    //	printf("driver read adrv nsamps: %d\n",nsamps);

    int ret = 0;
    static long long nextTime;
    static bool nextTimeValid = false;
    adrv_state_t *s = (adrv_state_t *) device->priv;
    bool time_set = false;
    long long timeNs = 0;
    int flags;
    int samples_received;

    int r;
    int m = s->rx_num_channels;
    int nsamps2;  // aligned to upper 32 or 16 byte boundary
#if defined(__x86_64) || defined(__i386__)
#ifdef __AVX2__
    nsamps2 = (nsamps+7)>>3;
    __m256i buff_tmp[2][nsamps2];
#else
    nsamps2 = (nsamps+3)>>2;
    __m128i buff_tmp[2][nsamps2];
#endif
#endif

    flags = 0;
    samples_received = 0;

    while(samples_received < nsamps)
    {
	    long long timeNst = 0;
    	    uint32_t* samps = (uint32_t*)buff_tmp[0];
	    uint32_t* sampsn = samps + samples_received;
	    int reqNum = min((int)nsamps-samples_received,(int)s->adrv[r]->getStreamMTU(s->rxStream[r]));
            ret = s->adrv[r]->readStream(s->rxStream[r], (void**)(&sampsn), (size_t)(reqNum), flags,timeNst, 1000000);
            if (ret < 0) 
    	    {
                if (ret == SOAPY_SDR_TIME_ERROR)
                    printf("[recv] Time Error in tx stream!\n");
                else if (ret == SOAPY_SDR_OVERFLOW | (flags & SOAPY_SDR_END_ABRUPT))
                    printf("[recv] Overflow occured!\n");
                else if (ret == SOAPY_SDR_TIMEOUT)
                    printf("[recv] Timeout occured!\n");
                else if (ret == SOAPY_SDR_STREAM_ERROR)
                    printf("[recv] Stream (tx) error occured!\n");
                else if (ret == SOAPY_SDR_CORRUPTION)
                    printf("[recv] Bad packet occured!\n");
                break;
            } 
     	    else
	    {
		if(samples_received == 0)
		{
			timeNs = timeNst;
		}
                samples_received += ret;
	    }
    }
    if (flags & SOAPY_SDR_HAS_TIME)
    {
               s->rx_timestamp = SoapySDR::timeNsToTicks(timeNs, s->sample_rate);
               *ptimestamp = s->rx_timestamp;
    }
    s->rx_count += samples_received;

    // bring RX data into 12 LSBs for softmodem RX
    for (int i=0; i<cc; i++) 
    {
          for (int j=0; j<nsamps2; j++) 
	  {
#if defined(__x86_64__) || defined(__i386__)
#ifdef   __AVX2__
            ((__m256i *)buff[i])[j] = _mm256_srai_epi16(buff_tmp[i][j],4);
#else
            ((__m128i *)buff[i])[j] = _mm_srai_epi16(buff_tmp[i][j],4);
#endif
#endif
          }
    }

    if (samples_received != nsamps)
            printf("[recv] rx samples %d != %d\n", samples_received, nsamps);

    return samples_received;
}

/*! \brief Get current timestamp of adrv
 * \param device the hardware to use
*/
openair0_timestamp get_adrv_time(openair0_device *device)
{
    
    printf("get hw timestamp\n");
    adrv_state_t *s = (adrv_state_t *) device->priv;
    return SoapySDR::timeNsToTicks(s->adrv[0]->getHardwareTime(""), s->sample_rate);
}

/*! \brief Compares two variables within precision
 * \param a first variable
 * \param b second variable
*/
static bool is_equal(double a, double b) 
{
    return std::fabs(a - b) < std::numeric_limits<double>::epsilon();
}

void *set_freq_thread(void *arg) 
{
    openair0_device *device = (openair0_device *) arg;
    adrv_state_t *s = (adrv_state_t *) device->priv;
    int r, i;
    printf("Setting adrv TX Freq %f, RX Freq %f\n", device->openair0_cfg[0].tx_freq[0],device->openair0_cfg[0].rx_freq[0]);
    // add check for the number of channels in the cfg
    for (r = 0; r < s->device_num; r++) 
    {
        for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX); i++) 
	{
            if (i < s->rx_num_channels)
                s->adrv[r]->setFrequency(SOAPY_SDR_RX, i, "RF", device->openair0_cfg[0].rx_freq[i]);
        }
        for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX); i++) 
	{
            if (i < s->tx_num_channels)
                s->adrv[r]->setFrequency(SOAPY_SDR_TX, i, "RF", device->openair0_cfg[0].tx_freq[i]);
        }
    }
}

/*! \brief Set frequencies (TX/RX)
 * \param device the hardware to use
 * \param openair0_cfg RF frontend parameters set by application
 * \param dummy dummy variable not used
 * \returns 0 in success
 */
int trx_adrv_set_freq(openair0_device *device, openair0_config_t *openair0_cfg, int dont_block) 
{
    adrv_state_t *s = (adrv_state_t *) device->priv;
    pthread_t f_thread;
    if (dont_block)
        pthread_create(&f_thread, NULL, set_freq_thread, (void *) device);
    else
    {
        int r, i;
        for (r = 0; r < s->device_num; r++)
	{
            printf("Setting adrv TX Freq %f, RX Freq %f\n", openair0_cfg[0].tx_freq[0], openair0_cfg[0].rx_freq[0]);
            for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX); i++)
	    {
                if (i < s->rx_num_channels)
		{
                    s->adrv[r]->setFrequency(SOAPY_SDR_RX, i, "LO", openair0_cfg[0].rx_freq[i]);
                }
            }
            for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX); i++) 
	    {
                if (i < s->tx_num_channels)
		{
                    s->adrv[r]->setFrequency(SOAPY_SDR_TX, i, "LO", openair0_cfg[0].tx_freq[i]);
                }
            }
        }
    }
    return (0);
}


/*! \brief Set Gains (TX/RX)
 * \param device the hardware to use
 * \param openair0_cfg RF frontend parameters set by application
 * \returns 0 in success
 */
int trx_adrv_set_gains(openair0_device *device,
                       openair0_config_t *openair0_cfg) 
{
    adrv_state_t *s = (adrv_state_t *) device->priv;
    int r;
    for (r = 0; r < s->device_num; r++) {
        s->adrv[r]->setGain(SOAPY_SDR_RX, 0, openair0_cfg[0].rx_gain[0]);
        s->adrv[r]->setGain(SOAPY_SDR_TX, 0, openair0_cfg[0].tx_gain[0]);
        s->adrv[r]->setGain(SOAPY_SDR_RX, 1, openair0_cfg[0].rx_gain[1]);
        s->adrv[r]->setGain(SOAPY_SDR_TX, 1, openair0_cfg[0].tx_gain[1]);
    }
    return (0);
}

/*! \brief adrv RX calibration table */
rx_gain_calib_table_t calib_table_adrv[] = {
        {3500000000.0, 83},
        {2660000000.0, 83},
        {2580000000.0, 83},
        {2300000000.0, 83},
        {1880000000.0, 83},
        {816000000.0,  83},
        {-1,           0}};


/*! \brief Set RX gain offset
 * \param openair0_cfg RF frontend parameters set by application
 * \param chain_index RF chain to apply settings to
 * \returns 0 in success
 */
void set_rx_gain_offset(openair0_config_t *openair0_cfg, int chain_index, int bw_gain_adjust) 
{

    int i = 0;
    // loop through calibration table to find best adjustment factor for RX frequency
    double min_diff = 6e9, diff, gain_adj = 0.0;
    if (bw_gain_adjust == 1) {
        switch ((int) openair0_cfg[0].sample_rate) {
            case 30720000:
                break;
            case 23040000:
                gain_adj = 1.25;
                break;
            case 15360000:
                gain_adj = 3.0;
                break;
            case 7680000:
                gain_adj = 6.0;
                break;
            case 3840000:
                gain_adj = 9.0;
                break;
            case 1920000:
                gain_adj = 12.0;
                break;
            default:
                printf("unknown sampling rate %d\n", (int) openair0_cfg[0].sample_rate);
                exit(-1);
                break;
        }
    }

    while (openair0_cfg->rx_gain_calib_table[i].freq > 0) 
    {
        diff = fabs(openair0_cfg->rx_freq[chain_index] - openair0_cfg->rx_gain_calib_table[i].freq);
        printf("cal %d: freq %f, offset %f, diff %f\n",
               i,
               openair0_cfg->rx_gain_calib_table[i].freq,
               openair0_cfg->rx_gain_calib_table[i].offset, diff);
        if (min_diff > diff) 
	{
            min_diff = diff;
            openair0_cfg->rx_gain_offset[chain_index] = openair0_cfg->rx_gain_calib_table[i].offset + gain_adj;
        }
        i++;
    }

}

/*! \brief print the adrv statistics
* \param device the hardware to use
* \returns  0 on success
*/
int trx_adrv_get_stats(openair0_device *device) 
{
    return (0);
}

/*! \brief Reset the adrv statistics
* \param device the hardware to use
* \returns  0 on success
*/
int trx_adrv_reset_stats(openair0_device *device) 
{
    return (0);
}


extern "C" {
/*! \brief Initialize Openair adrv target. It returns 0 if OK
* \param device the hardware to use
* \param openair0_cfg RF frontend parameters set by application
*/
int device_init(openair0_device *device, openair0_config_t *openair0_cfg) {

    size_t i, r, card;
    int bw_gain_adjust = 0;
    openair0_cfg[0].rx_gain_calib_table = calib_table_adrv;
    adrv_state_t *s = (adrv_state_t *) malloc(sizeof(adrv_state_t));
    memset(s, 0, sizeof(adrv_state_t));

    string devFE("DEV");
    string cbrsFE("CBRS");
    string wireFormat("WIRE");
    string devName;

    SoapySDR::KwargsList soapy_args = SoapySDR::Device::enumerate();

    if(soapy_args.size() == 0)
    {
        printf("No Soapy devices found\n");
        exit(-1);
    }

    printf("%d Soapy devices found\n",soapy_args.size());
    for (size_t i=0; i<soapy_args.size(); i++)
    {
	if(soapy_args[i].find("device")->second == "adrvsdr")
	{
            devName = "adrvsdr";
      	    printf("Soapy has found device ADRV9361 SDR\n");
      	}
    }

    // Initialize adrv device
    device->openair0_cfg = openair0_cfg;
    SoapySDR::Kwargs args;
    args["driver"] = "adrvsdr";
    char *adrv_addrs = device->openair0_cfg[0].sdr_addrs;
    if (adrv_addrs == NULL)
    {
        s->adrv.push_back(SoapySDR::Device::make(args));
    }
    else
    {
        char *serial = strtok(adrv_addrs, ",");
        while (serial != NULL) {
            LOG_I(HW, "Attempting to open adrv device %s\n", serial);
            args["serial"] = serial;
            s->adrv.push_back(SoapySDR::Device::make(args));
            serial = strtok(NULL, ",");
        }
    }

    s->device_num = s->adrv.size();
    device->type = ADRV9361_DEV;

    // Determine the sampling frequency and bandwidth
    switch ((int) openair0_cfg[0].sample_rate) 
    {
        case 30720000:
            //openair0_cfg[0].samples_per_packet    = 1024;
            openair0_cfg[0].tx_sample_advance = 115;
            openair0_cfg[0].tx_bw = 20e6;
            openair0_cfg[0].rx_bw = 20e6;
            break;
        case 23040000:
            //openair0_cfg[0].samples_per_packet    = 1024;
            openair0_cfg[0].tx_sample_advance = 113;
            openair0_cfg[0].tx_bw = 15e6;
            openair0_cfg[0].rx_bw = 15e6;
            break;
        case 15360000:
            //openair0_cfg[0].samples_per_packet    = 1024;
            openair0_cfg[0].tx_sample_advance = 60;
            openair0_cfg[0].tx_bw = 10e6;
            openair0_cfg[0].rx_bw = 10e6;
            break;
        case 7680000:
            //openair0_cfg[0].samples_per_packet    = 1024;
            openair0_cfg[0].tx_sample_advance = 30;
            openair0_cfg[0].tx_bw = 5e6;
            openair0_cfg[0].rx_bw = 5e6;
            break;
        case 1920000:
            //openair0_cfg[0].samples_per_packet    = 1024;
            openair0_cfg[0].tx_sample_advance = 20;
            openair0_cfg[0].tx_bw = 1.4e6;
            openair0_cfg[0].rx_bw = 1.4e6;
            break;
        default:
            printf("Error: unknown sampling rate %f\n", openair0_cfg[0].sample_rate);
            exit(-1);
            break;
    }

    printf("tx_sample_advance %d\n", openair0_cfg[0].tx_sample_advance);
    s->rx_num_channels = openair0_cfg[0].rx_num_channels;
    s->tx_num_channels = openair0_cfg[0].tx_num_channels;
    if ((s->rx_num_channels == 1 || s->rx_num_channels == 2) && (s->tx_num_channels == 1 || s->tx_num_channels == 2))
        printf("Enabling %d rx and %d tx channel(s) on each device...\n", s->rx_num_channels, s->tx_num_channels);
    else 
    {
        printf("Invalid rx or tx number of channels (%d, %d)\n", s->rx_num_channels, s->tx_num_channels);
        exit(-1);
    }

    for (r = 0; r < s->device_num; r++) 
    {
        // Setting RX Sampling Rate, Frequency, and Gain
	for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX); i++) 
	{
            if (i < s->rx_num_channels) 
	    {
                s->adrv[r]->setSampleRate(SOAPY_SDR_RX, i, openair0_cfg[0].sample_rate / SAMPLE_RATE_DOWN);
                s->adrv[r]->setFrequency(SOAPY_SDR_RX, i, "LO", openair0_cfg[0].rx_freq[i]);
                s->adrv[r]->setGainMode(SOAPY_SDR_RX, i, false);
                s->adrv[r]->setGain(SOAPY_SDR_RX, i, openair0_cfg[0].rx_gain[i]);
            }
        }

        // Setting TX Sampling Rate, Frequency, and Gain
        for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX); i++) 
	{
            if (i < s->tx_num_channels) 
	    {
                s->adrv[r]->setSampleRate(SOAPY_SDR_TX, i, openair0_cfg[0].sample_rate / SAMPLE_RATE_DOWN);
		if(openair0_cfg[0].tx_freq[i] != 0)
	                s->adrv[r]->setFrequency(SOAPY_SDR_TX, i, "LO", openair0_cfg[0].tx_freq[i]);
		else
			s->adrv[r]->setFrequency(SOAPY_SDR_TX, i, "LO", openair0_cfg[0].rx_freq[i]-120000000); // UL freq = DL freq - 120 MHz

                s->adrv[r]->setGain(SOAPY_SDR_TX, i, openair0_cfg[0].tx_gain[i]);
                    
		//s->adrv[r]->writeSetting("TX_ENABLE_DELAY", "0");
                //s->adrv[r]->writeSetting("TX_DISABLE_DELAY", "100");
            }
        }

        int tx_filt_bw = openair0_cfg[0].tx_bw;
        int rx_filt_bw = openair0_cfg[0].rx_bw;
        
	// Setting RX BW
        for (i = 0; i < s->tx_num_channels; i++)
	{
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX))
	    {
                s->adrv[r]->setBandwidth(SOAPY_SDR_TX, i, tx_filt_bw);
                printf("Setting tx bandwidth on channel %zu/%lu: BW %f (readback %f)\n", i,
                       s->adrv[r]->getNumChannels(SOAPY_SDR_TX), tx_filt_bw / 1e6,
                       s->adrv[r]->getBandwidth(SOAPY_SDR_TX, i) / 1e6);
            }
        }

	// Setting TX BW
        for (i = 0; i < s->rx_num_channels; i++) 
	{
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX)) 
	    {
                s->adrv[r]->setBandwidth(SOAPY_SDR_RX, i, rx_filt_bw);
                printf("Setting rx bandwidth on channel %zu/%lu : BW %f (readback %f)\n", i,
                       s->adrv[r]->getNumChannels(SOAPY_SDR_RX), rx_filt_bw / 1e6,
                       s->adrv[r]->getBandwidth(SOAPY_SDR_RX, i) / 1e6);
            }
        }

        /*for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX); i++) 
	{
            if (i < s->tx_num_channels) 
	    {
                printf("\nUsing SKLK calibration...\n");
                s->adrv[r]->writeSetting(SOAPY_SDR_TX, i, "CALIBRATE", "SKLK");

            }

        }

        for (i = 0; i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX); i++) 
	{
            if (i < s->rx_num_channels) 
	    {
                printf("\nUsing SKLK calibration...\n");
                s->adrv[r]->writeSetting(SOAPY_SDR_RX, i, "CALIBRATE", "SKLK");

            }

        }*/

        if(openair0_cfg[0].duplex_mode == 0)
	{
            printf("FDD: Setting receive antenna to %s\n", s->adrv[r]->listAntennas(SOAPY_SDR_RX, 0)[0].c_str());
            s->adrv[r]->setAntenna(SOAPY_SDR_RX, 0, s->adrv[r]->listAntennas(SOAPY_SDR_RX, i)[0].c_str());
            
	    printf("FDD: Setting transmit antenna to %s\n", s->adrv[r]->listAntennas(SOAPY_SDR_TX, 0)[0].c_str());
            s->adrv[r]->setAntenna(SOAPY_SDR_TX, 0, s->adrv[r]->listAntennas(SOAPY_SDR_TX, i)[0].c_str());
        }
	else 
	{
            printf("TDD: Not supported yet\n");
        }


        //s->adrv[r]->writeSetting("TX_SW_DELAY", std::to_string(
        //        -openair0_cfg[0].tx_sample_advance)); //should offset switching to compensate for RF path (Lime) delay -- this will eventually be automated

        // create tx & rx streamer
        //const SoapySDRKwargs &arg = SoapySDRKwargs();
        std::map <std::string, std::string> rxStreamArgs;
        rxStreamArgs["WIRE"] = SOAPY_SDR_CS16;

        std::vector <size_t> channels;
        for (i = 0; i < s->rx_num_channels; i++)
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX))
                channels.push_back(i);
        s->rxStream.push_back(s->adrv[r]->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CS16, channels));//, rxStreamArgs));

        std::vector <size_t> tx_channels = {};
        for (i = 0; i < s->tx_num_channels; i++)
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX))
                tx_channels.push_back(i);
        s->txStream.push_back(s->adrv[r]->setupStream(SOAPY_SDR_TX, SOAPY_SDR_CS16, tx_channels)); //, arg));
        //s->adrv[r]->setHardwareTime(0, "");

        //std::cout << "Front end detected: " << s->adrv[r]->getHardwareInfo()["frontend"] << "\n";
        for (i = 0; i < s->rx_num_channels; i++) 
	{
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_RX)) 
	    {
                printf("RX Channel %zu\n", i);
                printf("Actual RX sample rate: %fMSps...\n", (s->adrv[r]->getSampleRate(SOAPY_SDR_RX, i) / 1e6));
                printf("Actual RX frequency: %fGHz...\n", (s->adrv[r]->getFrequency(SOAPY_SDR_RX, i) / 1e9));
                printf("Actual RX gain: %f...\n", (s->adrv[r]->getGain(SOAPY_SDR_RX, i)));
                printf("Actual RX bandwidth: %fM...\n", (s->adrv[r]->getBandwidth(SOAPY_SDR_RX, i) / 1e6));
                printf("Actual RX antenna: %s...\n", (s->adrv[r]->getAntenna(SOAPY_SDR_RX, i).c_str()));
            }
        }

        for (i = 0; i < s->tx_num_channels; i++) 
	{
            if (i < s->adrv[r]->getNumChannels(SOAPY_SDR_TX)) 
	    {
                printf("TX Channel %zu\n", i);
                printf("Actual TX sample rate: %fMSps...\n", (s->adrv[r]->getSampleRate(SOAPY_SDR_TX, i) / 1e6));
                printf("Actual TX frequency: %fGHz...\n", (s->adrv[r]->getFrequency(SOAPY_SDR_TX, i) / 1e9));
                printf("Actual TX gain: %f...\n", (s->adrv[r]->getGain(SOAPY_SDR_TX, i)));
                printf("Actual TX bandwidth: %fM...\n", (s->adrv[r]->getBandwidth(SOAPY_SDR_TX, i) / 1e6));
                printf("Actual TX antenna: %s...\n", (s->adrv[r]->getAntenna(SOAPY_SDR_TX, i).c_str()));
            }
        }
    }
    //s->adrv[0]->writeSetting("SYNC_DELAYS", "");
    //for (r = 0; r < s->device_num; r++)
    //    s->adrv[r]->setHardwareTime(0, "TRIGGER");
    //s->adrv[0]->writeSetting("TRIGGER_GEN", "");
    //for (r = 0; r < s->device_num; r++)
    //    printf("Device timestamp: %f...\n", (s->adrv[r]->getHardwareTime("TRIGGER") / 1e9));

    device->priv = s;
    device->trx_start_func = trx_adrv_start;
    device->trx_write_func = trx_adrv_write;
    device->trx_read_func = trx_adrv_read;
    device->trx_get_stats_func = trx_adrv_get_stats;
    device->trx_reset_stats_func = trx_adrv_reset_stats;
    device->trx_end_func = trx_adrv_end;
    device->trx_stop_func = trx_adrv_stop;
    device->trx_set_freq_func = trx_adrv_set_freq;
    device->trx_set_gains_func = trx_adrv_set_gains;
    device->openair0_cfg = openair0_cfg;

    s->sample_rate = openair0_cfg[0].sample_rate;
    // TODO:
    // init tx_forward_nsamps based adrv_time_offset ex
    if (is_equal(s->sample_rate, (double) 30.72e6))
        s->tx_forward_nsamps = 176;
    if (is_equal(s->sample_rate, (double) 15.36e6))
        s->tx_forward_nsamps = 90;
    if (is_equal(s->sample_rate, (double) 7.68e6))
        s->tx_forward_nsamps = 50;

    LOG_I(HW, "Finished initializing %d adrv device(s).\n", s->device_num);
    fflush(stdout);
    return 0;
}
}
/*@}*/
