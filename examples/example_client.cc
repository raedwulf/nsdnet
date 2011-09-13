/**
 *
 * \brief nsdnet client example
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 *
 * \section Description
 *
 * This simple client illustrates the use of a plugin interface. It subscribes
 * to a device providing the interface, then tests each message.
 *
 */

#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include "nsdnetproxy.h"

using namespace PlayerCc;

int main(int argc, const char **argv)
{
	int i;
   PlayerClient *client;
	NSDNetProxy *proxy;

	// Takes a parameter for the index.
	int index = 0;
	if (argc < 2)
	{
		printf("Needs one parameter.\n");
		return 0;
	}
	else
	{
		index = atoi(argv[1]);
		printf("Using index %d\n", index);
	}

	// Create a client and connect it to the server.
	client = new PlayerClient("localhost", 6665);
   proxy = new NSDNetProxy(client, index);

	// Get our clientid
	proxy->RequestProperty(std::string("self.id"));

	// Print out what we get...
	printf("Client id: %s\n", proxy->GetProperty().c_str());

	// Endless loop...
   for (i=0;i<100;i++)
	{
      if (client->Peek())
      {
         client->Read();
         if (proxy->ReceiveMessageCount())
         {
            time_t timestamp;
            std::string src;
            std::string msg;
            proxy->ReceiveMessage(timestamp, src, msg);
            std::cout << src << ": " << msg << " [" << i << "]" << std::endl;
         }
         usleep(0);
      }
      else
      {
         std::cout << "Sending Hello World" << std::endl;
         std::stringstream hw;
         hw << "Hello World " << i;
         proxy->SendMessage(hw.str());
         usleep(100000);
      }
	}

	// Shutdown
   delete proxy;
   delete client;

	return 0;
}
