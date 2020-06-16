#include "SoapyAdrvSDR.hpp"
#include <SoapySDR/Registry.hpp>

static std::vector<SoapySDR::Kwargs> results;
static std::vector<SoapySDR::Kwargs> find_AdrvSDR(const SoapySDR::Kwargs &args)
{
	if(!results.empty())
	 	return results;
	
	SoapySDR::Kwargs options;

	options["device"] = "adrvsdr";
	string hostname;
	int cmdport;
	int strport;
	int rxbuffersize;
	int txbuffersize;

	// Try the default hostname and ports
	if (args.count("hostname") == 0) 
	{
		hostname = "10.0.9.1";
		cmdport = 50707;
		strport = 50708;
		rxbuffersize = 5760+6;
		txbuffersize = 5760+1500+6;
	} 
	else
	{ 
		hostname = args.at("hostname");
		cmdport = stoi(args.at("cmdport"));
		strport = stoi(args.at("strport"));
		rxbuffersize = stoi(args.at("rxbuffersize"));
		txbuffersize = stoi(args.at("txbuffersize"));
	}
	
	if(UDPClient::findServer(cmdport,strport,hostname,rxbuffersize,txbuffersize)!=NULL)
	{
		options["hostname"] = hostname;
		options["cmdport"] = to_string(cmdport);
		options["strport"] = to_string(strport);
		options["rxbuffersize"] = to_string(rxbuffersize);
		options["txbuffersize"] = to_string(txbuffersize);
	}
	else
	{
		SoapySDR_logf(SOAPY_SDR_ERROR, "ADRV9361 Soapy driver could not be found");
		return results; //failed to connect
	}

	// Make the label
	char label_str[100];
	sprintf(label_str, "%s #%d ip: %s %s %s", options["device"].c_str(), 0, options["hostname"].c_str(),options["cmdport"].c_str(),options["strport"].c_str(),options["rxbuffersize"].c_str(),options["txbuffersize"].c_str());
	options["label"] = label_str;
	results.push_back(options);

	return results;
}

static SoapySDR::Device *make_AdrvSDR(const SoapySDR::Kwargs &args)
{
	return new SoapyAdrvSDR(args);
}

static SoapySDR::Registry register_adrvsdr("adrvsdr", &find_AdrvSDR, &make_AdrvSDR, SOAPY_SDR_ABI_VERSION);
