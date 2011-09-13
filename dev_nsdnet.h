/*
 * This file declares a C client library proxy for the interface. The functions
 * are defined in nsdnet_client.c.
 */
#ifndef _DEV_NSDNET_H_
#define _DEV_NSDNET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libplayerc/playerc.h>
#include <time.h>

#if defined (WIN32)
  #if defined (PLAYER_STATIC)
    #define NSDNET_EXPORT
  #elif defined (nsdnet_EXPORTS)
    #define NSDNET_EXPORT    __declspec (dllexport)
  #else
    #define NSDNET_EXPORT    __declspec (dllimport)
  #endif
#else
  #define NSDNET_EXPORT
#endif

#define CLIENTID_LEN 64
typedef char clientid_string_t[CLIENTID_LEN];

#define MAX_MESSAGES 16384

typedef struct nsdmsg_s
{
   time_t timestamp;
   char clientid[CLIENTID_LEN];
   int msg_count;
   char *msg;
} nsdmsg_t;

struct nsdnet_s;

typedef struct nsdnet_s
{
	/** Device info; must be at the start of all device structures. */
	playerc_device_t info;

   /** List of clients received on request */
   clientid_string_t *listclients;

   /** Count of the list of clients received on request */
   int listclients_count;

   /** Property values requested */
   char *propval;

   /** Queue of received messages */
   nsdmsg_t queue[MAX_MESSAGES];
   int queue_head;
   int queue_tail;

   /** Last error message */
   int error_msg_count;
   char *error_msg;
   char error_code;

   /** User value */
   void *user;
} nsdnet_t;

/**
 * Creates a proxy for the interface.
 * This creates the proxy and provides the initialisation for it.
 * \param client The client object.
 * \param index The index of the client object.
 * \return A nsdnet_t proxy device object.
 */
NSDNET_EXPORT nsdnet_t *nsdnet_create(playerc_client_t *client, int index);

/**
 * Destroys a proxy for the interface.
 * \param device The nsdnet_t proxy device to be destroyed.
 */
NSDNET_EXPORT void nsdnet_destroy(nsdnet_t *device);

/**
 * Subscribes to a device that provides the interface.
 * \param device The nsdnet_t proxy object to be subscribed to.
 * \param access The access required.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_subscribe(nsdnet_t *device, int access);

/**
 * Unsubscribes from a subscribed device.
 * \param device The nsdnet_t proxy object to be unsubscribed from.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_unsubscribe(nsdnet_t *device);

/**
 * Makes a request for a list of clients currently connected.
 * \param device The nsdnet_t proxy object to request a list of clients from.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_get_listclients(nsdnet_t *device);

/**
 * Sends a command (a message) to a particular client.
 * \param device The nsdnet_t proxy object to send messages.
 * \param target The target client id to send a message to.
 * \param len The length of the message.
 * \param message The actual message.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_send_message(nsdnet_t *device, const char *target, int len, char *message);

/**
 * Receives a command (a message) from a particular client.
 * \param device The nsdnet_t proxy object to receive messages from.
 * \param message The message to be received.
 * \return 0 if no messages, anything else means there's a message to be received.
 */
NSDNET_EXPORT int nsdnet_receive_message(nsdnet_t *device, nsdmsg_t **message);

/**
 * Get a property value from the device.
 * \param device The nsdnet_t proxy object to get a property from.
 * \param variable The name of the property variable.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_property_get(nsdnet_t *device, const char *variable);

/**
 * Set a property value from the device.
 * \param device The nsdnet_t proxy object to set a property.
 * \param variable The name of the property variable.
 * \param value The value of the property variable for setting.
 * \return 0 if successful, anything else is an error.
 */
NSDNET_EXPORT int nsdnet_property_set(nsdnet_t *device, const char *variable, const char *value);
#ifdef __cplusplus
}
#endif

#endif
