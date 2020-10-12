#include <memory>
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <algorithm>
#include <chrono>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
//#include <boost/asio.hpp>

#include "SoapyAdrvSDR.hpp"

#define TS_TO_NS 10
#define RX_STAT_FILE 1

char rx_buffer[MAXBUF_SIZE_BYTE];
char tx_buffer[MAXBUF_SIZE_BYTE];

uint64_t reg_timeNs = 0;

rx_streamer* rxstr = NULL;
tx_streamer* txstr = NULL;

std::vector<std::string> SoapyAdrvSDR::getStreamFormats(const int direction, const size_t channel) const
{
	std::vector<std::string> formats;

	formats.push_back(SOAPY_SDR_CS8);
	formats.push_back(SOAPY_SDR_CS12);
	formats.push_back(SOAPY_SDR_CS16);
	formats.push_back(SOAPY_SDR_CF32);

	return formats;
}

std::string SoapyAdrvSDR::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
	if (direction == SOAPY_SDR_RX) 
		fullScale = 2048; // RX expects 12 bit samples LSB aligned
	else if (direction == SOAPY_SDR_TX)
		fullScale = 32768; // TX expects 12 bit samples MSB aligned
	
	return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList SoapyAdrvSDR::getStreamArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList streamArgs;
	return streamArgs;
}


bool SoapyAdrvSDR::IsValidRxStreamHandle(SoapySDR::Stream* handle) const
{
	if (handle == nullptr) 
		return false;

	//handle is an opaque pointer hiding either rx_stream or tx_streamer:
	//check that the handle matches one of them, consistently with direction:
	if (rx_stream) 
	{
		//test if these handles really belong to us:
		if (reinterpret_cast<rx_streamer*>(handle) == rx_stream.get())
	    		return true;
	}

	return false;
}

bool SoapyAdrvSDR::IsValidTxStreamHandle(SoapySDR::Stream* handle) const
{
	if (handle == nullptr) 
		return false;

	//handle is an opaque pointer hiding either rx_stream or tx_streamer:
	//check that the handle matches one of them, consistently with direction:
	if (tx_stream) 
	{
		//test if these handles really belong to us:
		if (reinterpret_cast<tx_streamer*>(handle) == tx_stream.get()) 
			return true;
	}

	return false;
}

SoapySDR::Stream *SoapyAdrvSDR::setupStream(const int direction, const std::string &format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args)
{
	//check the format
	plutosdrStreamFormat streamFormat;
	if (format == SOAPY_SDR_CF32) 
	{
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
		streamFormat = PLUTO_SDR_CF32;
	}
	else if (format == SOAPY_SDR_CS16) 
	{
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
		streamFormat = PLUTO_SDR_CS16;
	}
	else if (format == SOAPY_SDR_CS12) 
	{
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS12.");
		streamFormat = PLUTO_SDR_CS12;
	}
	else if (format == SOAPY_SDR_CS8) 
	{
		SoapySDR_log(SOAPY_SDR_INFO, "Using format CS8.");
		streamFormat = PLUTO_SDR_CS8;
	}
	else 
	{
		throw std::runtime_error("setupStream invalid format '" + format + "' -- Only CS8, CS12, CS16 and CF32 are supported by SoapyAdrvSDR module.");
	}

	if(direction == SOAPY_SDR_RX)
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
        	this->rx_stream = std::unique_ptr<rx_streamer>(new rx_streamer(udpc,streamFormat, channels, args, phandler));
		rxstr = (rx_streamer*)this->rx_stream.get();
        	return reinterpret_cast<SoapySDR::Stream*>(this->rx_stream.get());
	}
	else if (direction == SOAPY_SDR_TX) 
	{
        	std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
        	this->tx_stream = std::unique_ptr<tx_streamer>(new tx_streamer(udpc, streamFormat, channels, args, phandler));
		txstr = (tx_streamer*)this->tx_stream.get();
	        return reinterpret_cast<SoapySDR::Stream*>(this->tx_stream.get());
	}

	return nullptr;
}

void SoapyAdrvSDR::closeStream(SoapySDR::Stream *handle)
{
	//scope lock:
    	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
        	if (IsValidRxStreamHandle(handle)) 
		{
        		this->rx_stream.reset();
        	}
    	}

    	//scope lock:
    	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
        	if (IsValidTxStreamHandle(handle)) 
		{
        		this->tx_stream.reset();
        	}
	}

}

size_t SoapyAdrvSDR::getStreamMTU(SoapySDR::Stream *handle) const
{
	{
		std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

		if (IsValidRxStreamHandle(handle)) 
		{
			return this->rx_stream->get_mtu_size();
		}
	}
	
	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

		if (IsValidTxStreamHandle(handle)) 
		{
			return this->tx_stream->get_mtu_size();
		}
	}

	return 0;
}

int SoapyAdrvSDR::activateStream(SoapySDR::Stream *handle, const int flags, const long long timeNs, const size_t numElems )
{
	if (flags & ~SOAPY_SDR_END_BURST)
		return SOAPY_SDR_NOT_SUPPORTED;


    	if (IsValidRxStreamHandle(handle))
	{
        	SoapySDR_logf(SOAPY_SDR_INFO, "Activating the rx stream");
		std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);
        	this->rx_stream->start(flags, timeNs, numElems);
	}
        	
    	if (IsValidTxStreamHandle(handle))
	{
        	SoapySDR_logf(SOAPY_SDR_INFO, "Activating the tx stream");
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);
		this->tx_stream->start(flags, timeNs, numElems);
	}
    	
	return 0;
}

int SoapyAdrvSDR::deactivateStream(SoapySDR::Stream *handle,const int flags,const long long timeNs )
{
	//scope lock:
	{
        	std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

		if (IsValidRxStreamHandle(handle))
		{
			SoapySDR_logf(SOAPY_SDR_INFO, "Deactivating the rx stream");
			this->rx_stream->stop(flags, timeNs);
		}
	}

	//scope lock :
	{
		std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

		if (IsValidTxStreamHandle(handle)) 
		{
			SoapySDR_logf(SOAPY_SDR_INFO, "Deactivating the tx stream");
			this->tx_stream->flush();
			this->tx_stream->stop(flags, timeNs);
		}
	}
	return 0;
}

int SoapyAdrvSDR::readStream(SoapySDR::Stream *handle,void * const *buffs,const size_t numElems,int &flags,long long &timeNs,const long timeoutUs )
{
	//the spin_mutex is especially very useful here for minimum overhead !
	//std::lock_guard<pluto_spin_mutex> lock(rx_device_mutex);

	if (IsValidRxStreamHandle(handle))
		return int(this->rx_stream->receive(buffs, numElems, flags, timeNs, timeoutUs));
	else
		return SOAPY_SDR_NOT_SUPPORTED;
}

int SoapyAdrvSDR::writeStream(SoapySDR::Stream *handle,const void * const *buffs,const size_t numElems,int &flags,const long long timeNs,const long timeoutUs )
{
	//std::lock_guard<pluto_spin_mutex> lock(tx_device_mutex);

	if (IsValidTxStreamHandle(handle))
		return this->tx_stream->send(buffs, numElems, flags, timeNs, timeoutUs);
	else 
        	return SOAPY_SDR_NOT_SUPPORTED;
}

int64_t tmp = 0;

int SoapyAdrvSDR::readStreamStatus(SoapySDR::Stream *stream,size_t &chanMask,int &flags,long long &timeNs,const long timeoutUs)
{
	//return SOAPY_SDR_NOT_SUPPORTED;
	//printf("[SoapyAdrv] Monitoring %s for underflows/overflows\n",iio_device_get_name(dev));
	//printf("[SoapyAdrv][rx_streamer] RX dif timestamp: %lld, TX dif timestamp: %lld \n",phandler->rxTimestampDif,phandler->txDifTimestampNS);
	
	sleep(5);
	
	// double underflows to string with precision
	ostringstream streamObj;
	streamObj << fixed;
	streamObj << setprecision(2);
	streamObj << phandler->rxunderflows;
	string strRxUnderflows = streamObj.str();

	// print link stats
	cout << left << setw(13) << "Link stats - " <<
		left << setw(22) << "RX drops: " + to_string(phandler->rxdrops) <<
        	left << setw(22) << "RX counter: " + to_string(phandler->rxidcounter) <<
		left << setw(22) << "TX sot-cmd: " + to_string(phandler->tx_sot_cmd) <<
		left << setw(22) << "TX drops: " + to_string(phandler->txdrops) <<
		left << setw(22) << "TX lates: " + to_string(phandler->txlates) <<
       		left << setw(22) << " TX earlies: " + to_string(phandler->txearlies) <<
       		left << setw(30) << " RX underflows: " + strRxUnderflows <<
       		left << setw(22) << " TX counter: " + to_string(phandler->txidcounter) << endl;

	/*if(phandler->txDifTimestampNS != tmp)
	{
		printf("[SoapyAdrv][rx_streamer] TX dif timestamp: %lld \n",phandler->txDifTimestampNS);
		tmp = phandler->txDifTimestampNS;
	}*/

	return 0;
}

rx_streamer::rx_streamer(UDPClient* _udpc, const plutosdrStreamFormat _format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args, pluto_handler_t* ph):
	udpc(_udpc), format(_format), phandler(ph), mtu_size(udpc->getRXBufferSizeByte()/4 - 6)
{
        SoapySDR_logf(SOAPY_SDR_INFO, "Constructing rx_streamer");

	// Get the actual buffer size from UDPClient
        buffer_size = udpc->getRXBufferSizeByte()/4;
	//mtu_size = udpc->getRXBufferSizeByte()/4 - 6;

        // Memset zero the buffer
	memset(&rx_buffer,0,MAXBUF_SIZE_BYTE);

	// Set the buffer size by sample rate
	//long long samplerate;
	//iio_channel_attr_read_longlong(iio_device_find_channel(dev, "voltage0", false),"sampling_frequency",&samplerate);
	//this->set_buffer_size_by_samplerate(samplerate);
	
	// Run the RX timestamp async thread
	fast_timestamp_en = true;
}

#if RX_STAT_FILE
vector<uint64_t> rxdrops;
vector<int> rxunderflowsDiff;
vector<uint64_t> rxunderflowsID;
#endif

rx_streamer::~rx_streamer()
{
#if RX_STAT_FILE
        //print rx drops and underflows ids to txt file
        FILE *fpOut;
        if((fpOut = fopen("/tmp/adrv_rxdrops.txt", "w")) == NULL)
        {
          printf("Unable to open file - quitting\n");
          return;
        }
        fprintf(fpOut, "id\n");

        for(int i=0; i<rxdrops.size(); i++)
        {
            fprintf(fpOut, "%lu", rxdrops[i]);
            fprintf(fpOut, "\n");
        }
        fprintf(fpOut, "\n");
        fprintf(fpOut, "%lu", phandler->rxidcounter);
        fprintf(fpOut, "\n");

        fclose(fpOut);
        
	//print underflows ids to txt file
        if((fpOut = fopen("/tmp/adrv_rxunderflows.txt", "w")) == NULL)
        {
          printf("Unable to open file - quitting\n");
          return;
        }
        fprintf(fpOut, "id diff\n");

        for(int i=0; i<rxunderflowsID.size(); i++)
        {
            fprintf(fpOut, "%lu %d", rxunderflowsID[i], rxunderflowsDiff[i]);
            fprintf(fpOut, "\n");
        }
        fprintf(fpOut, "\n");
        fprintf(fpOut, "%lu", phandler->rxidcounter);
        fprintf(fpOut, "\n");

        fclose(fpOut);
#endif
}

void rx_streamer::set_buffer_size_by_samplerate(const size_t samplerate) 
{
    //Adapt buffer size (= MTU) as a tradeoff to minimize readStream overhead but at
    //the same time allow realtime applications. Keep it a power of 2 which seems to be better.
    //so try to target very roughly 60fps [30 .. 100] readStream calls / s for realtime applications.
    int rounded_nb_samples_per_call = (int)::round(samplerate / 60.0);

    int power_of_2_nb_samples = 0;

    while (rounded_nb_samples_per_call > (1 << power_of_2_nb_samples)) 
    {
        power_of_2_nb_samples++;
    }

    this->set_buffer_size(1 << power_of_2_nb_samples);

    SoapySDR_logf(SOAPY_SDR_INFO, "Auto setting Buffer Size: %lu", (unsigned long)buffer_size);

    //Recompute MTU from buffer size change.
    //We always set MTU size = Buffer Size.
    //On buffer size adjustment to sample rate,
    //MTU can be changed accordingly safely here.
    //set_mtu_size(this->buffer_size);

    SoapySDR_logf(SOAPY_SDR_INFO, "Auto setting buffer size done");
}

uint64_t rxidOffset;

size_t rx_streamer::receive(void * const *buffs, const size_t numElems, int &flags, long long &timeNs, const long timeoutUs)
{
	// Clock frequency = 100 MHz
	double ts_to_ns = (double)TS_TO_NS;

	// Clock frequency = 4*sampling frequency
	//double ts_to_ns = (double)250000000/(double)phandler->rxSamplingFrequency;
	//if(isnan(ts_to_ns))
	//	ts_to_ns = ((double)250000000.0)/((double)(5760000));

	if(items_in_buffer <= 0)
	{
		int ret = udpc->receiveStreamBuffer(rx_buffer);

	    	if(ret < 0)
	    	{
			SoapySDR_logf(SOAPY_SDR_ERROR, "Unable to receive RX buffer");
			return SOAPY_SDR_TIMEOUT;
		}

		// Read and save the timestamp in the last 8 bytes of the buffer.
		// We save the timestamp and modify the items_in_buffer
		if(fast_timestamp_en)
		{
			// Read the RX timestamp and tx_dif_timestamp
	                uint64_t* rx_timestamp_pointer = (uint64_t*)(rx_buffer+(buffer_size*4)-8);
			uint64_t tmpRxTimestampNS = ((double)(*rx_timestamp_pointer))*((double)ts_to_ns);
			
			// Calculate underflow
			double bufferDurationNS = (((double)buffer_size-6.000)/((double)phandler->rxSamplingFrequency))*((double)1e9);
			int diffNS = tmpRxTimestampNS - phandler->rxTimestampNS;
			double diffMS = (double)diffNS/bufferDurationNS;

			//printf("rx ts_to_ns: %LG\n",ts_to_ns());
			//printf("Got a timestamp in: %lld, received items: %d\n",phandler->rxTimestampDif,ret/4);
			//printf("Got a timestamp (NS): %llu, original: %llu, received items: %d\n",phandler->rxTimestampNS,(*rx_timestamp_pointer),ret/4);
		
			// Read TX link stats data
        	        uint64_t* tx_timestamp_pointer = (uint64_t*)(rx_buffer+(buffer_size*4)-16);
			uint16_t* pdrops = (uint16_t*)tx_timestamp_pointer;
			uint16_t* plates = pdrops + 1;
			uint16_t* pearlies = pdrops + 2;
			uint16_t* pidcounter = pdrops + 3;
			//cout << "Stats - drops: " << *pdrops << " lates: " << *plates << " earlies: " << *pearlies << " idcounter: " << *pidcounter << endl;
			phandler->txdrops = *pdrops;
			phandler->txlates = *plates;
			phandler->txearlies = *pearlies;
			phandler->txidcounter = *pidcounter;
			
			// Read RX link stats data
        	        uint32_t* p_rxidcounter = (uint32_t*)(rx_buffer+(buffer_size*4)-24);
			if (phandler->rxidcounter == 0)
			{
				rxidOffset = *p_rxidcounter-1;
			}
			uint64_t rxidc = *p_rxidcounter - rxidOffset; 
			
			if(rxidc - phandler->rxidcounter > 1)
			{
#if RX_STAT_FILE
				// SAVE RX drops TO FILE
				for(uint64_t i = phandler->rxidcounter+1; i<rxidc; i++)
					rxdrops.push_back(i);
#endif

				phandler->rxdrops += (rxidc - phandler->rxidcounter - 1);
			}
			// Set rxidcounter into the handler
			phandler->rxidcounter = rxidc;
			
			// If we have underflows above threshold ~100 us
			if(diffMS > 1.1)
			{
#if RX_STAT_FILE
				// SAVE RX underflow TO FILE
				rxunderflowsID.push_back(phandler->rxidcounter);
				rxunderflowsDiff.push_back(diffNS-1000000);
#endif
				phandler->rxunderflows += diffMS-1;
			}

			// Set rxtimestamp into the handler
			phandler->rxTimestampNS = tmpRxTimestampNS;
			
			// Read tx sot_cmd and copy to handler
        	        uint32_t* psot_cmd = p_rxidcounter+1;
			phandler->tx_sot_cmd = *psot_cmd;
			//if(phandler->tx_sot_cmd > 100 || phandler->tx_sot_cmd < -100)
			//{
			//	cout << phandler->tx_sot_cmd << endl;
			//}

			
			// timestamp reading is done
			// modify the items_in_buffer number so the rest of the code doesnt take the timestamp
			ret = ret - 24;
		}

		// Read and save the tx dif timestamp in the last 24 bytes of the buffer.
	    	items_in_buffer = (unsigned long)ret / 4;
	    	byte_offset = 0;
	}

	size_t items = std::min(items_in_buffer,numElems);
			
	// optimize for single RX, 2 channel (I/Q), same endianess direct copy
	// note that RX is 12 bits LSB aligned, i.e. fullscale 2048
	uint8_t *src = (uint8_t *)rx_buffer + byte_offset;
	int16_t const *src_ptr = (int16_t *)src;

	if (format == PLUTO_SDR_CS16)
	{
		::memcpy(buffs[0], src_ptr, 2 * sizeof(uint16_t) * items);
	}
	else if (format == PLUTO_SDR_CF32) 
	{
		float *dst_cf32 = (float *)buffs[0];

		for (size_t index = 0; index < items * 2; ++index) 
		{
			*dst_cf32 = float(*src_ptr) / 2048.0f;

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
	else
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, "Unknown RX format");
	        throw std::runtime_error("Unknown RX format");
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
        size_t bytes_remain = (items_in_buffer-items) * 4;

        // timeNs_sub is the value calculated and should be subtracted from reg_timeNs based on the interpolateion
        uint64_t timeNs_sub = (double)(bytes_remain)/(double)4*evr;
        // printf("from SoapyAdrv reg_timeNs: %llu, byte_offset: %lu, bytes_remain: %lu, sampling_freq: %d, 4 bytes every: %lf ns\n",reg_timeNs,byte_offset,bytes_remain, sampling_freq,evr);
        timeNs = reg_timeNs - timeNs_sub;
	
	//timeNs = reg_timeNs; 

	//printf("New RX timestamp: %llu\n",timeNs);

	items_in_buffer -= items;
	byte_offset += items * 4;

	//printf("rx_streamer: New RX items: %d, timestamp: %llu\n",items,timeNs);
	
	flags = flags | SOAPY_SDR_HAS_TIME;

	return items;
}

int rx_streamer::start(const int flags,const long long timeNs,const size_t numElems)
{
	//force proper stop before
    	stop(flags, timeNs);
    
    	SoapySDR_logf(SOAPY_SDR_INFO, "Adrv rx_streamer start streaming");

	// Enable RX Channel
        string req = "start rx";
        string res = udpc->sendCommand(req);
	if(res.find("error") != string::npos)
        {
        	SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
		throw runtime_error(res);
        }

	return 0;
}

int rx_streamer::stop(const int flags,const long long timeNs)
{
        memset(&rx_buffer,0,MAXBUF_SIZE_BYTE);

	string req = "stop rx";
	string res = udpc->sendCommand(req);
	if(res.find("error") != string::npos)
        {
        	SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
		throw runtime_error(res);
        }

    	items_in_buffer = 0;
	byte_offset = 0;

	return 0;
}

void rx_streamer::set_buffer_size(const int _buffer_size)
{
//TODO
/*	if (_buffer_size<=16*1024) 
	{
		// first clean
        	memset(&rx_buffer,0,_buffer_size*4);

		// init things
		items_in_buffer = 0;
        	byte_offset = 0;
		this->buffer_size=_buffer_size;
	}
	else
		SoapySDR_logf(SOAPY_SDR_ERROR, "UDP buffer cannot be larger than 16K samples/64KB: %d",a_buffer_size);*/
	
	// init things
	/*items_in_buffer = 0;
        byte_offset = 0;

	this->buffer_size=_buffer_size;*/

}


tx_streamer::tx_streamer(UDPClient* _udpc, const plutosdrStreamFormat _format, const std::vector<size_t> &channels, const SoapySDR::Kwargs &args,pluto_handler_t* ph) :
	udpc(_udpc),format(_format),phandler(ph), mtu_size(udpc->getTXBufferSizeByte()/4 - 6)
{
        SoapySDR_logf(SOAPY_SDR_INFO, "Constructing tx_streamer");

	// Get the actual buffer size from UDPClient
	buffer_size = udpc->getTXBufferSizeByte()/4;
	//mtu_size = udpc->getTXBufferSizeByte()/4 - 6;

	// Memset zero the buffer
	memset(&tx_buffer,0,MAXBUF_SIZE_BYTE);
	items_in_buf = 0;

	// Enable timed transmissions	
	fast_timestamp_en = true;
}

tx_streamer::~tx_streamer()
{

}

int tx_streamer::start(const int flags,const long long timeNs,const size_t numElems)
{
	//force proper stop before
	stop(flags, timeNs);
    
	SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer start streaming");

	// Enable TX Channel
        string req = "start tx";
        string res = udpc->sendCommand(req);
        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

	return 0;
}


int tx_streamer::stop(const int flags,const long long timeNs)
{
	memset(&tx_buffer,0,MAXBUF_SIZE_BYTE);

	string req = "stop tx";
        string res = udpc->sendCommand(req);
        if(res.find("error") != string::npos)
        {
                SoapySDR_logf(SOAPY_SDR_ERROR, res.c_str());
                throw runtime_error(res);
        }

	items_in_buf = 0;

	return 0;
}

int txId=0;
unsigned long prevTime = 0;

int tx_streamer::send(	const void * const *buffs,const size_t numElems,int &flags,const long long timeNs,const long timeoutUs )
{
	if(numElems < 20)
		return numElems;

	// Keep 6 samples free at the end of the buffer for metadata
	size_t buf_size_revised = buffer_size - 6; // Buf_size is the number of samples in the buffer. Metadata is 6 samples.
	size_t items = std::min(buf_size_revised - items_in_buf, numElems);

	double ts_to_ns = (double)TS_TO_NS;
	int sampling_freq = phandler->txSamplingFrequency;
	uint64_t tx_timestamp = ((double)timeNs)/((double)ts_to_ns);
	uint64_t tx_duration = ((double)items*(double)1e9)/((double)sampling_freq*(double)ts_to_ns);

	//struct timeval tv;
	//gettimeofday(&tv,NULL);
	//unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;
	//SoapySDR_logf(SOAPY_SDR_INFO, "tx_streamer_send timeNs: %lld, buf_size: %d, items: %d, numElems: %d, id:%d, time:%lu , time_dif: %lu",timeNs, (int)buf_size_revised, (int)items, numElems,txId,time_in_micros,time_in_micros-prevTime);
	//prevTime = time_in_micros;

	uint8_t *dst_ptr;
	int buf_step = 4;

	if (format == PLUTO_SDR_CS16)
	{
		//printf("tx_streamer::send direct_copy and PLUTO_SDR_CS16 \n");
		
		// optimize for single TX, 2 channel (I/Q), same endianess direct copy
		dst_ptr = (uint8_t *)tx_buffer + items_in_buf * 2 * sizeof(int16_t);

		memcpy(dst_ptr, buffs[0], 2 * sizeof(int16_t) * items);
	}
	else if (format == PLUTO_SDR_CF32)
	{
		// By Samie
		float* samples_cf32 = (float*) buffs[0];
		
		// optimize for single TX, 2 channel (I/Q), same endianess direct copy
		dst_ptr = (uint8_t *)tx_buffer + items_in_buf * 2 * sizeof(int16_t);
	
		for (size_t j = 0; j < items; ++j) 
		{
			int16_t src_i = (int16_t)(samples_cf32[j*2] * 32767.999f); // 32767.999f (0x46ffffff) will ensure better distribution
			int16_t src_q = (int16_t)(samples_cf32[j*2+1] * 32767.999f); // 32767.999f (0x46ffffff) will ensure better distribution
			
			((int16_t*)dst_ptr)[0] = src_i;
			((int16_t*)dst_ptr)[1] = src_q;
			
			//printf("Items: %d, j: %d, Input samps: %f, %f - Output samps: %d, %d\n",items, j,samples_cf32[j*2],samples_cf32[j*2+1],src_i,src_q);

			dst_ptr += buf_step;
		}
		
	}
	else if (format == PLUTO_SDR_CS12) 
	{
		//printf("tx_streamer::send direct_copy and PLUTO_SDR_CS12 \n");

		dst_ptr = (uint8_t *)tx_buffer + items_in_buf * 2 * sizeof(int16_t);
		int8_t *samples_cs12 = (int8_t *)buffs[0];

		for (size_t index = 0; index < items; ++index) 
		{
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
	else if (format == PLUTO_SDR_CS12) 
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, "CS12 not available with this endianess or channel layout");
		throw std::runtime_error("CS12 not available with this endianess or channel layout");
	}
	else
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, "Unknown TX format");
                throw std::runtime_error("Unknown TX format");
	}
	
	phandler->txTimestampNS = timeNs;
	items_in_buf += items;

	//SoapySDR_logf(SOAPY_SDR_INFO, "send_buf items_in_buf: %d, buf_size_revised: %d, timeNs: %llu",(int)items_in_buf,(int)buf_size_revised,phandler->txTimestampNS);
	//SoapySDR_logf(SOAPY_SDR_INFO, "send_buf items_in_buf: %d, buf_size: %d\n",(int)items_in_buf,(int)buf_size_revised);

	// Write metadata
	// Write TX timestamp
	uint64_t* p_tx_timestamp = (uint64_t*)(tx_buffer+(buffer_size*4)-8);
	*p_tx_timestamp = tx_timestamp;
		
	//printf("tx ts_to_ns: %f, txSamplingFrequency: %d, timeNs: %llu, timestamp double: %f\n",ts_to_ns,phandler->txSamplingFrequency, timeNs,tmp);
	//if(*p_tx_timestamp < 100000)
	//printf("TX timestamp written: %llu\n",*p_tx_timestamp);
			
	// Write frame duration
	uint64_t* p_tx_duration = (uint64_t*)(tx_buffer+(buffer_size*4)-16);
	*p_tx_duration = tx_duration;
			
	// Write TX flag
	uint32_t* p_tx_flag = (uint32_t*)(tx_buffer+(buffer_size*4)-24);
        *p_tx_flag = 1234512345;

	// Write TX id
	uint16_t* p_tx_id = (uint16_t*)(p_tx_flag+1);
        *p_tx_id = txId;

	// Write the actual TX frame size
	uint16_t* p_tx_size = p_tx_id+1;
	*p_tx_size = buf_size_revised - items;
		
	items_in_buf += 6;

	int ret = send_buf();
	if (ret < 0) 
		return SOAPY_SDR_ERROR;

	return items;
}

int tx_streamer::flush()
{
	return send_buf();
}


int tx_streamer::send_buf()
{
	if (items_in_buf > 0) 
	{
		if (items_in_buf < buffer_size)
		{
			int buf_step = 4;
			uint8_t *buf_ptr = (uint8_t *)tx_buffer + (items_in_buf-6) * buf_step;
			uint8_t *buf_end = (uint8_t *)tx_buffer + ((buffer_size-6)*4);

			memset(buf_ptr, 0, buf_end - buf_ptr);
		}
			

		//if(txId>500)
		//	return -1;

		int ret = udpc->sendStreamBuffer(tx_buffer);
		txId++;
		
		items_in_buf = 0;

		if (ret < 0)
			return ret;

		if(fast_timestamp_en)
		{
			ret = ret - 24;
		}

		return int(ret / 4);
	}

	return 0;

}

