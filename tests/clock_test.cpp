#include <iostream>
#include <vector>
#include <string>

#include "../UDPClient.h"

#include <sstream>
#include <sys/time.h>
#include <iomanip>
#include <ctime>
#include <ratio>
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>
#include <string>
#include <functional>

using namespace std;
using namespace std::chrono;

void setTimerOffset(UDPClient* udpc,int64_t offset)
{
        stringstream gstream;
        gstream << offset;
        string offset_str = gstream.str();

        string req = "set tx timeroffset " + offset_str;
        string res = udpc->sendCommand(req);
}

uint64_t getHWTimestamp(UDPClient* udpc)
{
        uint64_t val = 0;
        string req = "get tx hwtimestamp";
        string res = udpc->sendCommand(req);
        istringstream iss(res);
        iss >> val;
        return val;
}

steady_clock::time_point host_t0;
uint64_t adrv_t0 = 0;
int64_t time_calibration(UDPClient* udpc)
{
        // Host and Adrv clock synchronization
        long long nanoseconds_adrv = 0;
        long long nanoseconds_host = 0;
        if(adrv_t0 != 0)
        {
                uint64_t adrv_t1 = getHWTimestamp(udpc);
                steady_clock::time_point host_t1 = steady_clock::now();

                nanoseconds_adrv = (adrv_t1-adrv_t0)*10;
                nanoseconds_host = std::chrono::duration_cast<std::chrono::nanoseconds>(host_t1-host_t0).count();

                //setTimerOffset((int64_t) ((microseconds_host-microseconds_adrv)*100));
        }
        adrv_t0 = getHWTimestamp(udpc);
        host_t0 = steady_clock::now();

	return (nanoseconds_host-nanoseconds_adrv);
}

int main()
{
	try
	{
		UDPClient* udpc = new UDPClient(50707,50708, "10.0.9.1",15*1024*4,15*1024*4);
		string req;
		string res;
		vector<int64_t> rtts;
		vector<int64_t> offs;


		for(int i=0;i<40;i++)
		{
			
                	steady_clock::time_point t1 = steady_clock::now();
                	getHWTimestamp(udpc);
                	steady_clock::time_point t2 = steady_clock::now();
                	long long rtt = std::chrono::duration_cast<std::chrono::nanoseconds>(t2-t1).count();
			cout << "RTT (with timestamp fetching): " << rtt << " ns" << endl;
			rtts.push_back(rtt);
		}

		for(int i=0;i<20;i++)
		{
			offs.push_back(time_calibration(udpc));
			cout << "offset: " << offs[offs.size()-1] << " ns" << endl;
			sleep(300);
		}
		double avg_offs =  1.0 * std::accumulate(offs.begin(), offs.end(), 0LL) / offs.size();
		cout << "average: "<< avg_offs << endl;

		// get HW timestamp
		req.clear(); res.clear();
		req = "get tx hwtimestamp";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// set timer offset
		req.clear(); res.clear();
		req = "set tx timeroffset 12345123451234";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// get HW timestamp
		req.clear(); res.clear();
		req = "get tx hwtimestamp";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// set timer offset
		req.clear(); res.clear();
		req = "set tx timeroffset -12345123451234";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// get HW timestamp
		req.clear(); res.clear();
		req = "get tx hwtimestamp";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
        }
	

	return 0;
}
