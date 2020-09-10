#ifndef SOAPY_PLUTO_SDR_H
#define SOAPY_PLUTO_SDR_H

#include <vector>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <algorithm>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string>

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/time.h>
#include <iomanip>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>

#include "UDPClient.h"

#define MAXBUF_SIZE_BYTE 65535

using namespace std;

typedef enum plutosdrStreamFormat {
	PLUTO_SDR_CF32,
	PLUTO_SDR_CS16,
	PLUTO_SDR_CS12,
	PLUTO_SDR_CS8
} plutosdrStreamFormat;

// A local spin_mutex usable with std::lock_guard
//for lightweight locking for short periods.
class pluto_spin_mutex 
{
	public:
    		pluto_spin_mutex() = default;
		pluto_spin_mutex(const pluto_spin_mutex&) = delete;
		pluto_spin_mutex& operator=(const pluto_spin_mutex&) = delete;
		~pluto_spin_mutex() { lock_state.clear(std::memory_order_release); }
    		void lock() { while (lock_state.test_and_set(std::memory_order_acquire)); }
    		void unlock() { lock_state.clear(std::memory_order_release); }
	private:
		std::atomic_flag lock_state = ATOMIC_FLAG_INIT;
};

typedef struct 
{
	uint64_t txTimestampNS = 0;
	int txSamplingFrequency;
	int64_t txDifTimestampNS = 0;

	uint64_t rxTimestampNS = 0;
	int rxSamplingFrequency;
	int64_t rxTimestampDif = 0;

	uint16_t txdrops = 0;
	uint16_t txlates = 0;
	uint16_t txearlies = 0;
	uint16_t txidcounter = 0;
	uint64_t rxidcounter = 0;
	double rxunderflows = 0;
	uint64_t rxdrops = 0;
	int32_t tx_sot_cmd = 0;

} pluto_handler_t;

class rx_streamer 
{
	public:
		rx_streamer(UDPClient* _udpc,const plutosdrStreamFormat format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args, pluto_handler_t* ph);
		~rx_streamer();
		size_t receive(void * const *buffs,const size_t numElems,int &flags,long long &timeNs,const long timeoutUs=100000);
		int start(const int flags,const long long timeNs,const size_t numElems);
		int stop(const int flags,const long long timeNs=100000);
		void set_buffer_size_by_samplerate(const size_t _samplerate);
	        size_t get_mtu_size() const { return mtu_size; }
        	pluto_handler_t* phandler;
	private:
		UDPClient* udpc;
		size_t byte_offset;
		size_t items_in_buffer;
		const plutosdrStreamFormat format;
		bool direct_copy;
		void set_buffer_size(const int _buffer_size);
        	const size_t mtu_size;
		size_t buffer_size;
		bool fast_timestamp_en;
};

class tx_streamer 
{

	public:
		tx_streamer(UDPClient* _udpc,const plutosdrStreamFormat format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args, pluto_handler_t* ph);
		~tx_streamer();
		int send(const void * const *buffs,const size_t numElems,int &flags,const long long timeNs,const long timeoutUs );
		int flush();
		int start(const int flags,const long long timeNs,const size_t numElems);
		int stop(const int flags,const long long timeNs=100000);
	        size_t get_mtu_size() const { return mtu_size; }
        	pluto_handler_t* phandler;
	private:
		UDPClient* udpc;
		int send_buf();
		const plutosdrStreamFormat format;
        	const size_t mtu_size;
		size_t buffer_size;
		size_t items_in_buf;
		bool direct_copy;
		bool fast_timestamp_en;
};

class SoapyAdrvSDR : public SoapySDR::Device
{
	public:
		SoapyAdrvSDR( const SoapySDR::Kwargs & args );
		~SoapyAdrvSDR();

		/*******************************************************************
		 * Identification API
		 ******************************************************************/

		std::string getDriverKey( void ) const;
		std::string getHardwareKey( void ) const;
		SoapySDR::Kwargs getHardwareInfo( void ) const;
		
		/*******************************************************************
		 * Channels API
		 ******************************************************************/

		size_t getNumChannels( const int ) const;
		bool getFullDuplex( const int direction, const size_t channel ) const;

		/*******************************************************************
		 * Stream API
		 ******************************************************************/

		std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;
		std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;
		SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

		SoapySDR::Stream *setupStream(
				const int direction,
				const std::string &format,
				const std::vector<size_t> &channels = std::vector<size_t>(),
				const SoapySDR::Kwargs &args = SoapySDR::Kwargs() );

		void closeStream( SoapySDR::Stream *stream );

		size_t getStreamMTU( SoapySDR::Stream *stream ) const;

		int activateStream(
				SoapySDR::Stream *stream,
				const int flags = 0,
				const long long timeNs = 0,
				const size_t numElems = 0 );

		int deactivateStream(
				SoapySDR::Stream *stream,
				const int flags = 0,
				const long long timeNs = 0 );

		int readStream(
				SoapySDR::Stream *stream,
				void * const *buffs,
				const size_t numElems,
				int &flags,
				long long &timeNs,
				const long timeoutUs = 100000 );

		int writeStream(
				SoapySDR::Stream *stream,
				const void * const *buffs,
				const size_t numElems,
				int &flags,
				const long long timeNs = 0,
				const long timeoutUs = 100000);

		int readStreamStatus(
				SoapySDR::Stream *stream,
				size_t &chanMask,
				int &flags,
				long long &timeNs,
				const long timeoutUs
				);


		/*******************************************************************
		 * Settings API
		 ******************************************************************/

		SoapySDR::ArgInfoList getSettingInfo(void) const;
		void writeSetting(const std::string &key, const std::string &value);
		std::string readSetting(const std::string &key) const;

		/*******************************************************************
		 * Antenna API
		 ******************************************************************/

		std::vector<std::string> listAntennas( const int direction, const size_t channel ) const;
		void setAntenna( const int direction, const size_t channel, const std::string &name );
		std::string getAntenna( const int direction, const size_t channel ) const;

		/*******************************************************************
		 * Frontend corrections API
		 ******************************************************************/

		bool hasDCOffsetMode( const int direction, const size_t channel ) const;

		/*******************************************************************
		 * Gain API
		 ******************************************************************/

		std::vector<std::string> listGains( const int direction, const size_t channel ) const;
		bool hasGainMode(const int direction, const size_t channel) const;
		void setGainMode( const int direction, const size_t channel, const bool automatic );
		bool getGainMode( const int direction, const size_t channel ) const;
		void setGain( const int direction, const size_t channel, const double value );
		void setGain( const int direction, const size_t channel, const std::string &name, const double value );
		double getGain( const int direction, const size_t channel, const std::string &name ) const;
		SoapySDR::Range getGainRange( const int direction, const size_t channel, const std::string &name ) const;

		/*******************************************************************
		 * Frequency API
		 ******************************************************************/

		void setFrequency( const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args = SoapySDR::Kwargs() );
		double getFrequency( const int direction, const size_t channel, const std::string &name ) const;
		SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;
		std::vector<std::string> listFrequencies( const int direction, const size_t channel ) const;
		SoapySDR::RangeList getFrequencyRange( const int direction, const size_t channel, const std::string &name ) const;

		/*******************************************************************
		 * Sample Rate API
		 ******************************************************************/

		void setSampleRate( const int direction, const size_t channel, const double rate );
		double getSampleRate( const int direction, const size_t channel ) const;
		std::vector<double> listSampleRates( const int direction, const size_t channel ) const;
		void setBandwidth( const int direction, const size_t channel, const double bw );
		double getBandwidth( const int direction, const size_t channel ) const;
		std::vector<double> listBandwidths( const int direction, const size_t channel ) const;

		private:

		UDPClient* udpc;

	        bool IsValidRxStreamHandle(SoapySDR::Stream* handle) const;
        	bool IsValidTxStreamHandle(SoapySDR::Stream* handle) const;
		bool is_sensor_channel(struct iio_channel *chn) const;
		double double_from_buf(const char *buf) const;
		double get_sensor_value(struct iio_channel *chn) const;
		std::string id_to_unit(const std::string &id) const;

		mutable pluto_spin_mutex rx_device_mutex;
        	mutable pluto_spin_mutex tx_device_mutex;
        	mutable pluto_spin_mutex tx_buffer_mutex;

		std::unique_ptr<rx_streamer> rx_stream;
	        std::unique_ptr<tx_streamer> tx_stream;

        	pluto_handler_t* phandler;
};


#endif
