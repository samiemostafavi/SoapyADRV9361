#include "SoapyPlutoSDR.hpp"
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


// Need to be a power of 2 for maximum efficiency ?
# define DEFAULT_RX_BUFFER_SIZE (1 << 16)
# define WRITE_RXSAMPLES_FILE 0

uint64_t reg_timeNs = 0;

std::vector<std::string> SoapyPlutoSDR::getStreamFormats(const int direction, const size_t channel) const
{
	std::vector<std::string> formats;

	formats.push_back(SOAPY_SDR_CS8);
	formats.push_back(SOAPY_SDR_CS12);
	formats.push_back(SOAPY_SDR_CS16);
	formats.push_back(SOAPY_SDR_CF32);

	return formats;
}

std::string SoapyPlutoSDR::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
	if (direction == SOAPY_SDR_RX) {
		fullScale = 2048; // RX expects 12 bit samples LSB aligned
	}
	else if (direction == SOAPY_SDR_TX) {
		fullScale = 32768; // TX expects 12 bit samples MSB aligned
	}
	return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList SoapyPlutoSDR::getStreamArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList streamArgs;

	return streamArgs;
}


bool SoapyPlutoSDR::IsValidRxStreamHandle(SoapySDR::Stream* handle) const
{
    if (handle == nullptr) {
        return false;
    }

    //handle is an opaque pointer hiding either rx_stream or tx_streamer:
    //check that the handle matches one of them, consistently with direction:
    if (rx_stream) {
        //test if these handles really belong to us:
        if (reinterpret_cast<rx_streamer*>(handle) == rx_stream.get()) {
            return true;
        }
    }

    return false;
}

bool SoapyPlutoSDR::IsValidTxStreamHandle(SoapySDR::Stream* handle)
{
    if (handle == nullptr) {
        return false;
    }

    //handle is an opaque pointer hiding either rx_stream or tx_streamer:
    //check that the handle matches one of them, consistently with direction:
    if (tx_stream) {
        //test if these handles really belong to us:
        if (reinterpret_cast<tx_streamer*>(handle) == tx_stream.get()) {
            return true;
        }
    }

    return false;
}

SoapySDR::Stream *SoapyPlutoSDR::setupStream(
		const int direction,
		const std::string &format,
		const std::vector<size_t> &channels,
		const SoapySDR::Kwargs &args )
{
	//check the format
	plutosdrStreamFormat streamFormat;
	if (format == SOAPY_SDR_CF32) {
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
		streamFormat = PLUTO_SDR_CF32;
	}
	else if (format == SOAPY_SDR_CS16) {
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
		streamFormat = PLUTO_SDR_CS16;
	}
	else if (format == SOAPY_SDR_CS12) {
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS12.");
		streamFormat = PLUTO_SDR_CS12;
	}
	else if (format == SOAPY_SDR_CS8) {
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS8.");
		streamFormat = PLUTO_SDR_CS8;
	}
	else {
		throw std::runtime_error(
			"setupStream invalid format '" + format + "' -- Only CS8, CS12, CS16 and CF32 are supported by SoapyPlutoSDR module.");
	}

	if(direction == SOAPY_SDR_RX)
	{

        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
        	this->rx_stream = std::unique_ptr<rx_streamer>(new rx_streamer (rx_dev, streamFormat, channels, args, phandler));
        	return reinterpret_cast<SoapySDR::Stream*>(this->rx_stream.get());
	}

	else if (direction == SOAPY_SDR_TX) 
	{

        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
        	this->tx_stream = std::unique_ptr<tx_streamer>(new tx_streamer (tx_dev, streamFormat, channels, args, phandler));
	        return reinterpret_cast<SoapySDR::Stream*>(this->tx_stream.get());
	}

	return nullptr;

}

void SoapyPlutoSDR::closeStream( SoapySDR::Stream *handle)
{
	//scope lock:
    	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
        	if (IsValidRxStreamHandle(handle)) 
		{
        		this->rx_stream.reset();
        	}
    	}

    	//scope lock :
    	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
        	if (IsValidTxStreamHandle(handle)) 
		{
        		this->tx_stream.reset();
        	}
	}

}

size_t SoapyPlutoSDR::getStreamMTU( SoapySDR::Stream *handle) const
{
    std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

    if (IsValidRxStreamHandle(handle)) {

        return this->rx_stream->get_mtu_size();
    }

    return 0;
}

int SoapyPlutoSDR::activateStream(
		SoapySDR::Stream *handle,
		const int flags,
		const long long timeNs,
		const size_t numElems )
{
	if (flags & ~SOAPY_SDR_END_BURST)
		return SOAPY_SDR_NOT_SUPPORTED;

    std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

    if (IsValidRxStreamHandle(handle)) {
        return this->rx_stream->start(flags, timeNs, numElems);
    }

    return 0;
}

int SoapyPlutoSDR::deactivateStream(
		SoapySDR::Stream *handle,
		const int flags,
		const long long timeNs )
{
    //scope lock:
    {
        std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

        if (IsValidRxStreamHandle(handle)) {
            return this->rx_stream->stop(flags, timeNs);
        }
    }

    //scope lock :
    {
        std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

        if (IsValidTxStreamHandle(handle)) {
            this->tx_stream->flush();
            return 0;
        }
    }

	return 0;
}

int SoapyPlutoSDR::readStream(
		SoapySDR::Stream *handle,
		void * const *buffs,
		const size_t numElems,
		int &flags,
		long long &timeNs,
		const long timeoutUs )
{
    //the spin_mutex is especially very useful here for minimum overhead !
    std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

    if (IsValidRxStreamHandle(handle)) {
        return int(this->rx_stream->recv(buffs, numElems, flags, timeNs, timeoutUs));
    } else {
        return SOAPY_SDR_NOT_SUPPORTED;
    }
}

int SoapyPlutoSDR::writeStream(
		SoapySDR::Stream *handle,
		const void * const *buffs,
		const size_t numElems,
		int &flags,
		const long long timeNs,
		const long timeoutUs )
{
    std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

    if (IsValidTxStreamHandle(handle)) {
        return this->tx_stream->send(buffs, numElems, flags, timeNs, timeoutUs);;
    } else {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

}

int SoapyPlutoSDR::readStreamStatus(
		SoapySDR::Stream *stream,
		size_t &chanMask,
		int &flags,
		long long &timeNs,
		const long timeoutUs)
{
	//return SOAPY_SDR_NOT_SUPPORTED;
	
	//printf("[SoapyPluto] Monitoring %s for underflows/overflows\n",iio_device_get_name(dev));

	int ret;
	int result = 0;
	uint32_t val;
		
	/* Give the main thread a moment to start the DMA */
        sleep(1);
	//printf("[SoapyPluto][rx_streamer] RX dif timestamp: %lld, TX dif timestamp: %lld \n",phandler->rxTimestampDif,phandler->txDifTimestampNS);

	return result;
}

void rx_streamer::set_buffer_size_by_samplerate(const size_t samplerate) 
{

    //Adapt buffer size (= MTU) as a tradeoff to minimize readStream overhead but at
    //the same time allow realtime applications. Keep it a power of 2 which seems to be better.
    //so try to target very roughly 60fps [30 .. 100] readStream calls / s for realtime applications.
    int rounded_nb_samples_per_call = (int)::round(samplerate / 60.0);

    int power_of_2_nb_samples = 0;

    while (rounded_nb_samples_per_call > (1 << power_of_2_nb_samples)) {
        power_of_2_nb_samples++;
    }

    //this->set_buffer_size(1 << power_of_2_nb_samples);
    //this->set_buffer_size(64*1024); // this is min
    this->set_buffer_size(64*1024+4); 

    SoapySDR_logf(SOAPY_SDR_INFO, "Auto setting Buffer Size: %lu", (unsigned long)buffer_size);

    //Recompute MTU from buffer size change.
    //We always set MTU size = Buffer Size.
    //On buffer size adjustment to sample rate,
    //MTU can be changed accordingly safely here.
    set_mtu_size(this->buffer_size);

    SoapySDR_logf(SOAPY_SDR_INFO, "Auto setting buffer size done");
}

void rx_streamer::set_mtu_size(const size_t mtu_size) {

    this->mtu_size = mtu_size;

    SoapySDR_logf(SOAPY_SDR_INFO, "Set MTU Size: %lu", (unsigned long)mtu_size);
}

#if WRITE_RXSAMPLES_FILE
FILE *fileOutputDebug;
#endif

rx_streamer::rx_streamer(const iio_device *_dev, const plutosdrStreamFormat _format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args, pluto_handler_t* ph):
	dev(_dev), buffer_size(DEFAULT_RX_BUFFER_SIZE), buf(nullptr), format(_format), mtu_size(DEFAULT_RX_BUFFER_SIZE), phandler(ph)

{
	if (dev == nullptr) {
		SoapySDR_logf(SOAPY_SDR_ERROR, "cf-ad9361-lpc not found!");
		throw std::runtime_error("cf-ad9361-lpc not found!");
	}
	unsigned int nb_channels = iio_device_get_channels_count(dev), i;
	for (i = 0; i < nb_channels; i++)
		iio_channel_disable(iio_device_get_channel(dev, i));

	//default to channel 0, if none were specified
	const std::vector<size_t> &channelIDs = channels.empty() ? std::vector<size_t>{0} : channels;

	for (i = 0; i < channelIDs.size() * 2; i++) {
		struct iio_channel *chn = iio_device_get_channel(dev, i);
		iio_channel_enable(chn);
		channel_list.push_back(chn);
	}

	printf("[SoapyPluto][rx_steamer] cf-ad9361-lpc number of channels: %d \n",channel_list.size());

	if ( args.count( "bufflen" ) != 0 )
	{
		try
		{
			size_t bufferLength = std::stoi(args.at("bufflen"));
			if (bufferLength > 0)
				this->set_buffer_size(bufferLength);
		}
		catch (const std::invalid_argument &){}
	}
	else
	{
		long long samplerate;
		iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", false),"sampling_frequency",&samplerate);
		this->set_buffer_size_by_samplerate(samplerate);

	}
	
#if WRITE_RXSAMPLES_FILE
        fileOutputDebug = fopen("/home/warpadmin/ad9361-plutosdr.bin" , "w" );
#endif

	// Run the RX timestamp async thread
	fast_timestamp_en = true;
	difts_piggy_en = true;
}

rx_streamer::~rx_streamer()
{
	if (buf) 
	{
        	iio_buffer_cancel(buf);
       		iio_buffer_destroy(buf);
    	}

    	for (unsigned int i = 0; i < channel_list.size(); ++i) 
	{
        	iio_channel_disable(channel_list[i]);
    	}

#if WRITE_RXSAMPLES_FILE
    	fclose(fileOutputDebug);
#endif

}

size_t rx_streamer::recv(void * const *buffs,
		const size_t numElems,
		int &flags,
		long long &timeNs,
		const long timeoutUs)
{
	if(items_in_buffer <= 0) 
	{
		if(!buf) 
	    	{
			// rx_streamer has already stopped but someone wants some samples
		    	return 0;
	    	}
	    	ssize_t ret = iio_buffer_refill(buf);
	    	if(ret < 0)
	    	{
	       		printf("rx_streamer: iio_buffer_refill error. \n");
			return SOAPY_SDR_TIMEOUT;
		}

		// Read and save the timestamp in the last 4 bytes of the buffer.
		// We save the timestamp and modify the items_in_buffer
		if(fast_timestamp_en)
		{
			// fix the p_end pointer if the timestamp is there
			char* p_end = (char*)(iio_buffer_end(buf) - 8);    // The timestamp is 8 bytes and the pointer here points to 1 byte.
			// Read the RX timestamp
			uint64_t* rx_timestamp_pointer = (uint64_t*)p_end;
			//printf("RX timestamp read: %llu\n",*rx_timestamp_pointer);
			//printf("rx dif ts: %lld\n",phandler->rxTimestampNS - *rx_timestamp_pointer);
			phandler->rxTimestampNS = *rx_timestamp_pointer;
			// timestamp reading is done
			// modify the items_in_buffer number so the rest of the code doesnt take the timestamp
			ret = ret - 8;
		
			if(difts_piggy_en)
			{
				// fix the p_end pointer if the timestamp is there
				p_end = p_end - 8;    			// The timestamp is 8 bytes and the pointer here points to 1 byte.
				// Read the RX timestamp
				uint64_t* txdif_timestamp_pointer = (uint64_t*)p_end;
				//printf("RX timestamp read: %llu\n",*rx_timestamp_pointer);
				if(*txdif_timestamp_pointer != -10)
					phandler->txDifTimestampNS = *txdif_timestamp_pointer;
				
				// timestamp reading is done
				// modify the items_in_buffer number so the rest of the code doesnt take the timestamp
				ret = ret - 8;
			}
	    
		}

		// Read and save the tx dif timestamp in the last 8-4 bytes of the buffer.
	    	items_in_buffer = (unsigned long)ret / iio_buffer_step(buf);
	    	byte_offset = 0;
	}

	size_t items = std::min(items_in_buffer,numElems);
			
	
	ptrdiff_t buf_step = iio_buffer_step(buf);
	if (direct_copy) 
	{
		// optimize for single RX, 2 channel (I/Q), same endianess direct copy
		// note that RX is 12 bits LSB aligned, i.e. fullscale 2048
		uint8_t *src = (uint8_t *)iio_buffer_start(buf) + byte_offset;
		int16_t const *src_ptr = (int16_t *)src;

		if (format == PLUTO_SDR_CS16) 
		{
			::memcpy(buffs[0], src_ptr, 2 * sizeof(int16_t) * items);
		}
		else if (format == PLUTO_SDR_CF32) 
		{
			float *dst_cf32 = (float *)buffs[0];

			for (size_t index = 0; index < items * 2; ++index) 
			{
				*dst_cf32 = float(*src_ptr) / 2048.0f;

				// writes 1920*2 samples (I and Q) of 1 (ms)
#if WRITE_RXSAMPLES_FILE
				// debug write in file
				fwrite(dst_cf32, 1, sizeof(*dst_cf32) , fileOutputDebug );
#endif

				src_ptr++;
				dst_cf32++;
			}

		}
		else if (format == PLUTO_SDR_CS12) 
		{
			int8_t *dst_cs12 = (int8_t *)buffs[0];

			for (size_t index = 0; index < items; ++index) 
			{
				int16_t i = *src_ptr++;
				int16_t q = *src_ptr++;
				// produce 24 bit (iiqIQQ), note the input is LSB aligned, scale=2048
				// note: byte0 = i[7:0]; byte1 = {q[3:0], i[11:8]}; byte2 = q[11:4];
				*dst_cs12++ = uint8_t(i);
				*dst_cs12++ = uint8_t((q << 4) | ((i >> 8) & 0x0f));
				*dst_cs12++ = uint8_t(q >> 4);
			}
		}
		else if (format == PLUTO_SDR_CS8) 
		{
			int8_t *dst_cs8 = (int8_t *)buffs[0];

			for (size_t index = 0; index < items * 2; index++) 
			{
				*dst_cs8 = int8_t(*src_ptr >> 4);
				src_ptr++;
				dst_cs8++;
			}
		}
		
	}
	else {
		int16_t conv = 0, *conv_ptr = &conv;

		for (unsigned int i = 0; i < channel_list.size(); i++) 
		{
			iio_channel *chn = channel_list[i];
			unsigned int index = i / 2;
		
			uint8_t *src = (uint8_t *)iio_buffer_first(buf, chn) + byte_offset;
			int16_t const *src_ptr = (int16_t *)src;

			if (format == PLUTO_SDR_CS16) 
			{
				int16_t *dst_cs16 = (int16_t *)buffs[index];

				for (size_t j = 0; j < items; ++j) {
					iio_channel_convert(chn, conv_ptr, src_ptr);
					src_ptr += buf_step;
					dst_cs16[j * 2 + i] = conv;
				}
			}
			else if (format == PLUTO_SDR_CF32) 
			{
				float *dst_cf32 = (float *)buffs[index];

				for (size_t j = 0; j < items; ++j) {
					iio_channel_convert(chn, conv_ptr, src_ptr);
					src_ptr += buf_step;
					dst_cf32[j * 2 + i] = float(conv) / 2048.0f;
				}
			}
			else if (format == PLUTO_SDR_CS8) 
			{
				int8_t *dst_cs8 = (int8_t *)buffs[index];

				for (size_t j = 0; j < items; ++j) {
					iio_channel_convert(chn, conv_ptr, src_ptr);
					src_ptr += buf_step;
					dst_cs8[j * 2 + i] = int8_t(conv >> 4);
				}
			}
			
		}
	}

	// timestamp handling
        if(byte_offset == 0)
	{
        	phandler->rxTimestampDif = phandler->rxTimestampNS - reg_timeNs;
        	reg_timeNs = phandler->rxTimestampNS;
	}
	
        // interpolate the timestamp for each data chunk read
        int sampling_freq = phandler->rxSamplingFrequency;
        // the rate of the stream is 4 bytes ((i,q) == (16 bits, 16 bits)) every 1e9/SAMPLING_FREQUENCY nanoseconds.
        double evr = (double)1e9/(double)sampling_freq;

        // how much bytes remain in the buffer
        size_t bytes_remain = (items_in_buffer-items) * iio_buffer_step(buf);
        
        // timeNs_sub is the value calculated and should be subtracted from reg_timeNs based on the interpolateion
        uint64_t timeNs_sub = (double)(bytes_remain)/(double)4*evr;
        //printf("from SoapyPluto reg_timeNs: %llu, byte_offset: %lu, bytes_remain: %lu, sampling_freq: %d, 4 bytes every: %lf ns\n",reg_timeNs,byte_offset,bytes_remain, sampling_freq,evr);
        timeNs = reg_timeNs - timeNs_sub;
	
	//printf("New RX timestamp: %llu\n",timeNs);

	items_in_buffer -= items;
	byte_offset += items * iio_buffer_step(buf);

	//printf("rx_streamer: New RX items: %d, timestamp: %llu\n",items,timeNs);

	return(items);

}

int rx_streamer::start(const int flags,
		const long long timeNs,
		const size_t numElems)
{
    //force proper stop before
    stop(flags, timeNs);
    
    SoapySDR_logf(SOAPY_SDR_INFO, "rX_Streamer::start re-create buffer");

    // re-create buffer
    buf = iio_device_create_buffer(dev, buffer_size, false);

    if (!buf) {
	SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to create buffer!");
	throw std::runtime_error("Unable to create buffer!\n");
    }

    direct_copy = has_direct_copy();

    //SoapySDR_logf(SOAPY_SDR_INFO, "Has direct RX copy: %d", (int)direct_copy);

    return 0;
}

int rx_streamer::stop(const int flags,
		const long long timeNs)
{
    if (buf) {
    
   	SoapySDR_logf(SOAPY_SDR_INFO, "rX_Streamer::stop cancel and destroy buf");
    	//cancel first
        iio_buffer_cancel(buf);
    	//then destroy
	iio_buffer_destroy(buf);
	buf = nullptr;
    
    }
    
    items_in_buffer = 0;
    byte_offset = 0;

    return 0;
}

void rx_streamer::set_buffer_size(const size_t _buffer_size)
{

	if (!buf || this->buffer_size != _buffer_size) 
	{
        	//cancel first
        	if (buf) 
            		iio_buffer_cancel(buf);
        	
		//then destroy
        	if (buf)
        		iio_buffer_destroy(buf);
        
		items_in_buffer = 0;
        	byte_offset = 0;

		buf = iio_device_create_buffer(dev, _buffer_size, false);
		if (!buf) 
		{
			SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to create buffer!");
			throw std::runtime_error("Unable to create buffer!\n");
		}

	}
	this->buffer_size=_buffer_size;
}

size_t rx_streamer::get_mtu_size() {
    return this->mtu_size;
}

// return wether can we optimize for single RX, 2 channel (I/Q), same endianess direct copy
bool rx_streamer::has_direct_copy()
{
	if (channel_list.size() != 2) // one RX with I + Q
		return false;

	ptrdiff_t buf_step = iio_buffer_step(buf);

	if (buf_step != 2 * sizeof(int16_t))
		return false;

	if (iio_buffer_start(buf) != iio_buffer_first(buf, channel_list[0]))
		return false;

	int16_t test_dst, test_src = 0x1234;
	iio_channel_convert(channel_list[0], &test_dst, (const void *)&test_src);

	return test_src == test_dst;

}


tx_streamer::tx_streamer(const iio_device *_dev, const plutosdrStreamFormat _format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args,pluto_handler_t* ph) :
	dev(_dev), format(_format), buf(nullptr),phandler(ph)
{
        SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer::tx_streamer");

	if (dev == nullptr) {
		SoapySDR_logf(SOAPY_SDR_ERROR, "cf-ad9361-dds-core-lpc not found!");
		throw std::runtime_error("cf-ad9361-dds-core-lpc not found!");
	}

	unsigned int nb_channels = iio_device_get_channels_count(dev), i;
	for (i = 0; i < nb_channels; i++)
		iio_channel_disable(iio_device_get_channel(dev, i));

	//default to channel 0, if none were specified
	const std::vector<size_t> &channelIDs = channels.empty() ? std::vector<size_t>{0} : channels;

	for (i = 0; i < channelIDs.size() * 2; i++) {
		iio_channel *chn = iio_device_get_channel(dev, i);
		iio_channel_enable(chn);
		channel_list.push_back(chn);
	}


	//buf_size = 5760; // Samie: it was 4096
	//buf_size = 4096;
	// min: 64*1024
	buf_size = 64*1024+4;
	items_in_buf = 0;
	buf = iio_device_create_buffer(dev, buf_size, false);
	if (!buf) {
		SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to create buffer!");
		throw std::runtime_error("Unable to create buffer!");
	}

	direct_copy = has_direct_copy();

	//SoapySDR_logf(SOAPY_SDR_INFO, "Has direct TX copy: %d", (int)direct_copy);
	printf("[SoapyPluto][tx_steamer] cf-ad9361-dds-core-lpc number of channels: %d, buf_size: %d \n",channel_list.size(),buf_size);
        //SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer::tx_streamer number of channels: %d, buf_size: %d",channel_list.size(),buf_size);
	
	// Run the TX timestamp async thread
	fast_timestamp_en = true;
	difts_piggy_en = true;
}

tx_streamer::~tx_streamer()
{
	if(buf) 
	{ 
		iio_buffer_destroy(buf);
	}

	for(unsigned int i=0;i<channel_list.size(); ++i)
		iio_channel_disable(channel_list[i]);

}

int tx_streamer::start(const int flags,
		const long long timeNs,
		const size_t numElems)
{
    //force proper stop before
    stop(flags, timeNs);
    
    SoapySDR_logf(SOAPY_SDR_INFO, "tx_Streamer::start re-create buffer");

    // re-create buffer
    buf = iio_device_create_buffer(dev, buf_size, false);


    if (!buf) {
	SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to create buffer!");
	throw std::runtime_error("Unable to create buffer!\n");
    }

    direct_copy = has_direct_copy();

    //SoapySDR_logf(SOAPY_SDR_INFO, "Has direct TX copy: %d", (int)direct_copy);

    return 0;
}

int tx_streamer::stop(const int flags,
		const long long timeNs)
{
    if (buf) {
    
   	SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer::stop cancel and destroy buf");
    	//cancel first
        iio_buffer_cancel(buf);
    	//then destroy
	iio_buffer_destroy(buf);
	buf = nullptr;
    
    }
    
    items_in_buf = 0;

    return 0;
}

int tx_streamer::send(	const void * const *buffs,
		const size_t numElems,
		int &flags,
		const long long timeNs,
		const long timeoutUs )

{
	if (!buf)
        	return 0;

	// Keep 2 samples free at the end of the buffer for the timestamp if it is enabled
	size_t buf_size_revised;
	if(fast_timestamp_en && difts_piggy_en)
		buf_size_revised = buf_size - 4; // Buf_size is the number of samples in the buffer. The timestamp is 2 samples.
	else if(fast_timestamp_en)
		buf_size_revised = buf_size - 2; // Buf_size is the number of samples in the buffer. The timestamp is 2 samples.
	else
		buf_size_revised = buf_size;

	size_t items = std::min(buf_size_revised - items_in_buf, numElems);
    	
	//SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer_send timeNs: %lld, buf_size: %d, items: %d",timeNs, (int)buf_size_revised, (int)items);

	int16_t src = 0;
	int16_t const *src_ptr = &src;
	uint8_t *dst_ptr;
	ptrdiff_t buf_step = iio_buffer_step(buf);

	if (direct_copy && format == PLUTO_SDR_CS16) {
		//printf("tx_streamer::send direct_copy and PLUTO_SDR_CS16 \n");
		
		// optimize for single TX, 2 channel (I/Q), same endianess direct copy
		dst_ptr = (uint8_t *)iio_buffer_start(buf) + items_in_buf * 2 * sizeof(int16_t);

		memcpy(dst_ptr, buffs[0], 2 * sizeof(int16_t) * items);
	}
	else if (direct_copy && format == PLUTO_SDR_CF32)
	{
		// By Samie
		float* samples_cf32 = (float*) buffs[0];
		
		// optimize for single TX, 2 channel (I/Q), same endianess direct copy
		dst_ptr = (uint8_t *)iio_buffer_start(buf) + items_in_buf * 2 * sizeof(int16_t);
	
		for (size_t j = 0; j < items; ++j) 
		{
			int16_t src_i = (int16_t)(samples_cf32[j*2] * 32767.999f); // 32767.999f (0x46ffffff) will ensure better distribution
			int16_t src_q = (int16_t)(samples_cf32[j*2+1] * 32767.999f); // 32767.999f (0x46ffffff) will ensure better distribution
			
			((int16_t*)dst_ptr)[0] = src_i;
			((int16_t*)dst_ptr)[1] = src_q;
			
			dst_ptr += buf_step;
		}
		
	}
	else if (direct_copy && format == PLUTO_SDR_CS12) {
		//printf("tx_streamer::send direct_copy and PLUTO_SDR_CS12 \n");

		dst_ptr = (uint8_t *)iio_buffer_start(buf) + items_in_buf * 2 * sizeof(int16_t);
		int8_t *samples_cs12 = (int8_t *)buffs[0];

		for (size_t index = 0; index < items; ++index) {
			// consume 24 bit (iiqIQQ)
			uint16_t src0 = uint16_t(*(samples_cs12++));
			uint16_t src1 = uint16_t(*(samples_cs12++));
			uint16_t src2 = uint16_t(*(samples_cs12++));
			// produce 2x 16 bit, note the output is MSB aligned, scale=32768
			// note: byte0 = i[11:4]; byte1 = {q[7:4], i[15:12]}; byte2 = q[15:8];
			*dst_ptr = int16_t((src1 << 12) | (src0 << 4));
			dst_ptr++;
			*dst_ptr = int16_t((src2 << 8) | (src1 & 0xf0));
			dst_ptr++;
		}
	}
	else if (format == PLUTO_SDR_CS12) {
		SoapySDR_logf(SOAPY_SDR_ERROR, "CS12 not available with this endianess or channel layout");
		throw std::runtime_error("CS12 not available with this endianess or channel layout");
	}
	else
	{
		// channel size is 2 (I and Q)
		
		for (unsigned int k = 0; k < channel_list.size(); k++) {
		
			iio_channel *chn = channel_list[k];


			unsigned int index = k / 2;

			dst_ptr = (uint8_t *)iio_buffer_first(buf, chn) + items_in_buf * buf_step;

			// note that TX expects samples MSB aligned, unlike RX which is LSB aligned
			if (format == PLUTO_SDR_CS16) {
				//printf("tx_streamer::send no direct PLUTO_SDR_CS16 \n");

				int16_t *samples_cs16 = (int16_t *)buffs[index];

				for (size_t j = 0; j < items; ++j) {
					src = samples_cs16[j*2+k];
					iio_channel_convert_inverse(chn, dst_ptr, src_ptr);
					dst_ptr += buf_step;
				}
			}
			else if (format == PLUTO_SDR_CF32) {
				//printf("tx_streamer::send no direct PLUTO_SDR_CF32 \n");
	
				float *samples_cf32 = (float *)buffs[index];
	
				for (size_t j = 0; j < items; ++j) {
					src = (int16_t)(samples_cf32[j*2+k] * 32767.999f); // 32767.999f (0x46ffffff) will ensure better distribution
					iio_channel_convert_inverse(chn, dst_ptr, src_ptr);
					dst_ptr += buf_step;
				}
			}
			else if (format == PLUTO_SDR_CS8) {
				//printf("tx_streamer::send no direct PLUTO_SDR_CS8 \n");
	
				int8_t *samples_cs8 = (int8_t *)buffs[index];
	
				for (size_t j = 0; j < items; ++j) {
					src = (int16_t)(samples_cs8[j*2+k] << 8);
					iio_channel_convert_inverse(chn, dst_ptr, src_ptr);
					dst_ptr += buf_step;
				}
			}
		}
	}

	// Save the timestamp if we are at the begining of the buffer, we need it in the end
	if(items_in_buf == 0)
		phandler->txTimestampNS = timeNs;

	//SoapySDR_logf(SOAPY_SDR_INFO, "send_buf items_in_buf: %d, buf_size: %d, timeNs: %s",(int)items_in_buf,(int)buf_size_revised,samp);
	//SoapySDR_logf(SOAPY_SDR_INFO, "send_buf items_in_buf: %d, buf_size: %d\n",(int)items_in_buf,(int)buf_size_revised);
	
	items_in_buf += items;

	if (items_in_buf == buf_size_revised || (flags & SOAPY_SDR_END_BURST && numElems == items))
	{
		//Write the timestamp in the last 8 bytes of the buffer
		if(fast_timestamp_en)
		{
			char* p_end = (char*)iio_buffer_end(buf);
			uint64_t* tx_timestamp_pointer = (uint64_t*)(p_end-8);
			uint64_t old_val = *tx_timestamp_pointer;
			*tx_timestamp_pointer = phandler->txTimestampNS;
			//printf("TX timestamp written: %llu, old_val: %llu\n",*tx_timestamp_pointer,old_val);
			items_in_buf = items_in_buf + 2;
		}

		// Since all last 16 bytes will be discarded after the timestamp is read,
		// No data should be written there
		if(difts_piggy_en)
			items_in_buf = items_in_buf + 2;

		int ret = send_buf();
		if (ret < 0) 
			return SOAPY_SDR_ERROR;

		if ((size_t)ret != buf_size_revised)
			return SOAPY_SDR_ERROR;
	}

	return items;
}

int tx_streamer::flush()
{
	return send_buf();
}

int tx_streamer::send_buf()
{
    	if (!buf) 
        	return 0;

	if (items_in_buf > 0) 
	{
		if (items_in_buf < buf_size) 
		{
			ptrdiff_t buf_step = iio_buffer_step(buf);
			uint8_t *buf_ptr = (uint8_t *)iio_buffer_start(buf) + items_in_buf * buf_step;
			uint8_t *buf_end = (uint8_t *)iio_buffer_end(buf);

			memset(buf_ptr, 0, buf_end - buf_ptr);
		}

		ssize_t ret = iio_buffer_push(buf);
		items_in_buf = 0;

		if (ret < 0)
			return ret;

		if(fast_timestamp_en)
			ret = ret - 8; // The timestamp is 8 bytes and the pointer here points to 1 byte.

		if(difts_piggy_en)
			ret = ret - 8;

		return int(ret / iio_buffer_step(buf));
	}

	return 0;

}

// return wether can we optimize for single TX, 2 channel (I/Q), same endianess direct copy
bool tx_streamer::has_direct_copy()
{

	if (channel_list.size() != 2) // one TX with I/Q
		return false;

	ptrdiff_t buf_step = iio_buffer_step(buf);

	if (buf_step != 2 * sizeof(int16_t))
		return false;

	if (iio_buffer_start(buf) != iio_buffer_first(buf, channel_list[0]))
		return false;

	int16_t test_dst, test_src = 0x1234;
	iio_channel_convert_inverse(channel_list[0], &test_dst, (const void *)&test_src);

	return test_src == test_dst;

}
