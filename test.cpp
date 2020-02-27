#include <iostream>
#include <vector>
#include <string>

#include "UDPClient.h"

using namespace std;


int main()
{
	try
	{
		UDPClient* udpc = new UDPClient(50707,50708, "127.0.0.1",15*1024,15*1024);
	}
	catch(runtime_error& re)
        {
                cout << "Runtime error in UDPServer runCommands: " << re.what() << endl;
        }





	return 0;
}
