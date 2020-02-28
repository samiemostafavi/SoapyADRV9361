#include <iostream>
#include <vector>
#include <string>

#include "UDPClient.h"

using namespace std;


int main()
{
	try
	{
		UDPClient* udpc = new UDPClient(50707,50708, "10.0.9.1",15*1024,15*1024);
		string req;
		string res;
		
		// LO frequency TX
		req.clear(); res.clear();
		req = "get tx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx frequency 4000000000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "get tx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// LO frequency RX
		req.clear(); res.clear();
		req = "get rx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "set rx frequency 300000000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "get rx frequency";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// Sampling frequency RX
		req.clear(); res.clear();
		req = "get rx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx samplerate 5760000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Sampling frequency TX
		req.clear(); res.clear();
		req = "get tx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx samplerate 5120000";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get tx samplerate";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Gain RX
		req.clear(); res.clear();
		req = "get rx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx gain 40";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// Gain TX
		req.clear(); res.clear();
		req = "get tx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set tx gain 20";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get tx gain";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		// GainMode RX
		req.clear(); res.clear();
		req = "get rx gainmode";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
		req = "set rx gainmode manual";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
		req = "get rx gainmode";
		res = udpc->sendCommand(req);
		cout << "Req: " << req << " - Res: " << res << endl;
		
		// Antenna RX
		req.clear(); res.clear();
                req = "get rx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set rx antenna B_BALANCED";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get rx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

                // Antenna TX
		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set tx antenna B";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Bandwidth RX
		req.clear(); res.clear();
                req = "get rx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set rx bandwidth 2200000";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get rx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Bandwidth TX
		req.clear(); res.clear();
                req = "get tx bandwidth";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "set tx bandwidth 3100000";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		req.clear(); res.clear();
                req = "get tx antenna";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

		// Enable Channels
		req.clear(); res.clear();
                req = "start rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "start tx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		// Disable Channels
		req.clear(); res.clear();
                req = "stop rx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;
		
		req.clear(); res.clear();
                req = "stop tx";
                res = udpc->sendCommand(req);
                cout << "Req: " << req << " - Res: " << res << endl;

	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
        }
	




	return 0;
}
