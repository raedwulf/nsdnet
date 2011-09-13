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
 * The important point to note for the purposes of this example is the call to
 * playerc_add_xdr_ftable(). This function adds the XDR serialisation functions
 * to Player's internal table so that messages sent and received by this client
 * can be serialised correctly.
 */

#include <libplayerc/playerc.h>
#include <stdio.h>
#include <unistd.h>

#include "dev_nsdnet.h"

playerxdr_function_t* player_plugininterf_gettable (void);

int main(int argc, const char **argv)
{
	int i;
	playerc_client_t *client;
	nsdnet_t *device;

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
	client = playerc_client_create(NULL, "localhost", 6665);
	if (0 != playerc_client_connect(client))
	{
		printf("Could not connect\n");
		return -1;
	}

	// Load the plugin interface
	if (playerc_add_xdr_ftable (player_plugininterf_gettable (), 0) < 0)
		printf("Could not add xdr functions\n");

	// Create and subscribe to a device using the interface.
	device = nsdnet_create(client, index);
	if (nsdnet_subscribe(device, PLAYER_OPEN_MODE) != 0)
	{
		printf("Could not subscribe\n");
		return -1;
	}

	// Get our clientid
	if (nsdnet_property_get(device, "self.id"))
	{
		printf("Failed to get registered device id...\n");
		return -1;
	}

	// Print out what we get...
	printf("Client id: %s\n", device->propval);

	// Get the list of clients
	if (nsdnet_get_listclients(device))
	{
		printf("Failed to get list of clients...\n");
		return -1;
	}

	// Print out what we get...
	printf("Clients: ");
	for (i = 0; i < device->listclients_count; i++)
	{
		printf("%s ", device->listclients[i]);
	}
	printf("\n");

	// Endless loop...
	for (i = 0; i < 1000; i++)
	{
		if (playerc_client_peek(client, 0) == 1)
		{
			playerc_client_read(client);
			while (device->queue_head != device->queue_tail)
			{
				nsdmsg_t *msg; int err;
				if ((err = nsdnet_receive_message(device, &msg)))
				{
					if (err < 0)
						printf("ERROR\n");
					else
						printf("%s: %s [%d]\n", msg->clientid, msg->msg, i);
				}
			}
			usleep(0);
		}
		else
		{
			printf("Sending Hello World\n");
			if (nsdnet_send_message(device, NULL, 12, "Hello World"))
			{
				printf("Could not broadcast 'Hello World'\n");
				return -1;
			}
			usleep(100000);
		}
	}

	// Shutdown
	nsdnet_unsubscribe(device);
	nsdnet_destroy(device);
	playerc_client_disconnect(client);
	playerc_client_destroy(client);

	return 0;
}
