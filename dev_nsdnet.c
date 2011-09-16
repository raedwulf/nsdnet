/**
 * Copyright (C) 2011 The University of York
 * Author(s):
 *   Tai Chi Minh Ralph Eastwood <tcmreastwood@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 *
 * \brief nsdnet client proxy for libplayerc
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libplayerc/playerc.h>
#include <libplayercommon/playercommon.h>

#include "nsdnet_interface.h"
#include "nsdnet_xdr.h"
#include "dev_nsdnet.h"

void nsdnet_putmsg(nsdnet_t *device, player_msghdr_t *header, uint8_t *data);

/**
 * Create a device.
 */
nsdnet_t *nsdnet_create(playerc_client_t *client, int index)
{
	nsdnet_t *device;

	/* initialisation */
	device = (nsdnet_t*) malloc(sizeof(nsdnet_t));
	memset(device, 0, sizeof (nsdnet_t));
	playerc_device_init(&device->info, client, PLAYER_NSDNET_CODE,
		index, (playerc_putmsg_fn_t) nsdnet_putmsg);

	return device;
}

/**
 * Destroy the device.
 */
void nsdnet_destroy (nsdnet_t *device)
{
	playerc_device_term (&device->info);
	if (device->listclients)
		free (device->listclients);
	free (device);
}

/**
 * Subscribe to a device.
 */
int nsdnet_subscribe (nsdnet_t *device, int access)
{
	return playerc_device_subscribe (&device->info, access);
}

/**
 * Unsubscribe to a device.
 */
int nsdnet_unsubscribe (nsdnet_t *device)
{
	return playerc_device_unsubscribe (&device->info);
}

/**
 * Called by the client library whenever there a data
 * message is received for this proxy.
 */
void nsdnet_putmsg (nsdnet_t *device, player_msghdr_t *header, uint8_t *data)
{
	if (header->type == PLAYER_MSGTYPE_DATA)
	{
		assert(header->size > 0);
		if (header->subtype == PLAYER_NSDNET_DATA_RECV)
		{
			player_nsdnet_recv_data_t *recv_data = (player_nsdnet_recv_data_t *) data;
			/* TODO: Detect overflow. */
			nsdmsg_t *m = device->queue + (device->queue_head % MAX_MESSAGES);
			if (device->queue_head - device->queue_tail >= MAX_MESSAGES) {
				printf("message overflow!");
				device->queue_tail++;
			}
			m->timestamp = time(NULL);
			strncpy(m->clientid, recv_data->clientid, CLIENTID_LEN - 1);
			m->msg_count = recv_data->msg_count;
			m->msg = malloc(m->msg_count);
			memcpy(m->msg, recv_data->msg, m->msg_count);
			device->queue_head++;
		}
		else if (header->subtype == PLAYER_NSDNET_DATA_ERROR)
		{
			player_nsdnet_error_data_t *err_data = (player_nsdnet_error_data_t *) data;
			device->error_msg_count = err_data->msg_count;
			if (device->error_msg)
				free(device->error_msg);
			device->error_msg = malloc(device->error_msg_count);
			strncpy(device->error_msg, err_data->msg, device->error_msg_count);
			device->error_code = err_data->code;
		}
		else
			printf ("skipping nsdnet message with unknown type/subtype: %s/%d\n", msgtype_to_str(header->type), header->subtype);
	}
}

/**
 * Request a list of clients connected.
 */
int nsdnet_get_listclients(nsdnet_t *device)
{
	int result = 0;
	player_nsdnet_listclients_req_t *resp;

	if ((result = playerc_client_request(device->info.client,
		&device->info, PLAYER_NSDNET_REQ_LISTCLIENTS, NULL,
		(void **)&resp)) < 0)
		return result;

	if (device->listclients)
		free(device->listclients);
	device->listclients = malloc(resp->clients_count);
	memcpy(device->listclients, resp->clients, resp->clients_count);
	device->listclients_count = resp->clients_count / CLIENTID_LEN;
	free(resp);
	return 0;
}

/**
 * Send a message to a target client id.
 */
int nsdnet_send_message(nsdnet_t *device, const char *target, int len, char *message)
{
	player_nsdnet_send_req_t req;
	memset(&req, 0, sizeof(req));
	if (target)
		strncpy(req.clientid, target, sizeof(req.clientid) - 1);
	req.msg_count = len;
	req.msg = message;
	return playerc_client_request(device->info.client, &device->info, PLAYER_NSDNET_REQ_SEND, &req, NULL);
}

/**
 * Receive a message from the queue (0 on success, otherwise no message to receive).
 */
int nsdnet_receive_message(nsdnet_t *device, nsdmsg_t **message)
{
	nsdmsg_t *m = device->queue + (device->queue_tail % MAX_MESSAGES);
	if (device->queue_head == device->queue_tail)
		return 0;
	if (message)
		*message = m;
	device->queue_tail++;
	return 1;
}

/**
 * Get a property value from the device.
 */
int nsdnet_property_get(nsdnet_t *device, const char *variable)
{
	int result;
	player_nsdnet_propget_req_t req;
	player_nsdnet_propget_req_t *resp;
	memset(&req, 0, sizeof(req));
	strncpy(req.key, variable, PLAYER_NSDNET_KEY_LEN-1);

	if ((result = playerc_client_request(device->info.client,
		&device->info, PLAYER_NSDNET_REQ_PROPGET, &req,
		(void **)&resp)) < 0)
		return result;

	if (device->propval)
		free(device->propval);
	device->propval = malloc(resp->value_count + 1);
	device->propval[resp->value_count] = '\0';
	memcpy(device->propval, resp->value, resp->value_count);
	free(resp);
	return 0;
}

/**
 * Set a property value from the device.
 */
int nsdnet_property_set(nsdnet_t *device, const char *variable, const char *value)
{
	player_nsdnet_propset_req_t req;
	memset(&req, 0, sizeof(req));
	strncpy(req.key, variable, PLAYER_NSDNET_KEY_LEN-1);
	req.value_count = strlen(value);
	req.value = (char *)value;
	return playerc_client_request(device->info.client, &device->info,
		PLAYER_NSDNET_REQ_PROPSET, &req, NULL);
}

