/**
 * \brief nsdnet driver
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 *
 * \section Description
 *
 * The nsdnet driver interfaces with the playernsd daemon to simulate
 * networking environment with player.
 */

#if !defined (WIN32)
   #include <unistd.h>
#endif
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <libplayercore/playercore.h>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>

#include "nsdnet_interface.h"
#include "playernsd_client.h"

/** Number of threads per driver instance */
#define NSDNET_THREAD_NUM 128

/** typedef for fixed strings */
typedef char clientIDString[PLAYER_NSDNET_CLIENTID_LEN];

/**
 * The class for the driver
 */
class NSDNetDriver : public ThreadedDriver, PlayerNSDClient::Handler
{
   public:
      /**
       * Constructor
       * Retrieve options from the configuration file and do any
       * pre-Setup() setup.
       * \param cf The config file.
       * \param section The section in the config file that defines this driver.
       */
      NSDNetDriver(ConfigFile* cf, int section) :
         ThreadedDriver(cf, section, true, PLAYER_MSGQUEUE_DEFAULT_MAXLEN, PLAYER_NSDNET_CODE),
         dataReadyListClients(false), dataReadyPropertyValue(false)
      {
         // Get address of the ground truth of the position 2d.
         if (cf->ReadDeviceAddr(&position2dAddr, section, "uses", PLAYER_POSITION2D_CODE, -1, NULL) == -1)
            hasPosition2d = false;
         else
            hasPosition2d = true;

         // Get the id of the nsdnet node, and get the daemon server configuration.
         const char *id = cf->ReadString(section, "id", NULL);
         host = cf->ReadString(section, "host", "localhost");
         port = cf->ReadString(section, "port", "9999");

         // Iterate sections to find stage
         worldFile = "";
         for (int i = 0; i < cf->GetSectionCount(); i++)
         {
            const char *value = cf->ReadString(i, "name", NULL);
            if (value && !strcmp(value, "stage"))
            {
               if ((value = cf->ReadFilename(i, "worldfile", NULL)))
               {
                  worldFile = value;
               }
               if ((value = cf->ReadString(i, "model", NULL)))
               {
                  model = value;
               }
            }
         }

         // If there is a world file, go open and read it.
         ConfigFile worldCf;
         localizationX = localizationY = localizationZ = localizationA = 0.0;
         poseX = poseY = poseZ = poseA = 0.0;
         if (worldFile.length() && worldCf.Load(worldFile.c_str()))
         {
            for (int i = 0; i < worldCf.GetSectionCount(); i++)
            {
               const char *value = worldCf.ReadString(i, "name", NULL);
               if (value && model == value)
               {
                  // Get pose origin
                  poseX = worldCf.ReadTupleLength(i, "pose", 0, 0.0);
                  poseY = worldCf.ReadTupleLength(i, "pose", 1, 0.0);
                  poseZ = worldCf.ReadTupleLength(i, "pose", 2, 0.0);
                  poseA = worldCf.ReadTupleAngle(i, "pose", 3, 0.0);
                  // Get localization origin
                  localizationX = worldCf.ReadTupleLength(i, "localization_origin", 0, 0.0);
                  localizationY = worldCf.ReadTupleLength(i, "localization_origin", 1, 0.0);
                  localizationZ = worldCf.ReadTupleLength(i, "localization_origin", 2, 0.0);
                  localizationA = worldCf.ReadTupleAngle(i, "localization_origin", 3, 0.0);
                  break;
               }
            }
         }

         if (id)
         {
            clientID = id;
         }
         else
         {
            PLAYER_ERROR("No 'id' in nsdnet section specified.");
            SetError(-1);
         }

         // Some initial zeroing
         respPropGet.value = 0;
         respListClients.clients = 0;

         std::cout << "Connecting to server " << host << " on port " << port << std::endl;
         client.reset(new PlayerNSDClient(*this));
         client->Connect(host, port);
      }

      /**
       * Destructor.
       * Cleanup memory allocated by the driver.
       */
      ~NSDNetDriver()
      {
         if (receivedMsg.msg)
            delete[] receivedMsg.msg;
         if (respListClients.clients)
            delete[] respListClients.clients;
         if (respPropGet.value)
            delete[] respPropGet.value;
      }

      /**
       * Set up the device.
       * \return 0 if things go well, and -1 otherwise.
       */
      virtual int MainSetup()
      {
         std::cout << "NSDNetDriver initialising" << std::endl;
         if (hasPosition2d)
         {
            position2dDevice = deviceTable->GetDevice(position2dAddr);
            if (!position2dDevice)
            {
               PLAYER_ERROR("Unable to locate position2d device");
               return -1;
            }
            if (position2dDevice->Subscribe(InQueue) != 0)
            {
               PLAYER_ERROR("Unable to subscribe to target device");
            }
         }
         std::cout << "NSDNetDriver ready" << std::endl;
         return 0;
      }

      /**
       * Shutdown the device.
       */
      virtual void MainQuit()
      {
         std::cout << "Shutting NSDNetDriver down" << std::endl;
         if (hasPosition2d)
            position2dDevice->Unsubscribe(InQueue);
         boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
         std::cout << "NSDNetDriver has been shutdown" << std::endl;
      }

      /**
       * The main loop resides here.
       * TODO: Maybe there's a tidy way of stopping at the end.
       */
      virtual void Main()
      {
         for (;;)
         {
            pthread_testcancel();
            this->Wait();
            this->ProcessMessages();
         }
      }

      /**
       * This method will be invoked on each incoming message
       * \param resp_queue Response queue of messages.
       * \param hdr The message header.
       * \param data The data of the message.
       */
      virtual int ProcessMessage(QueuePointer& resp_queue,
         player_msghdr *hdr, void *data)
      {
         if (Message::MatchMessage (hdr, PLAYER_MSGTYPE_REQ,
            PLAYER_NSDNET_REQ_LISTCLIENTS, device_addr))
         {
            std::cout << "NSDNetDriver: Got request for list clients " << std::endl;
            client->RequestClientList();
            // Block until we know the response.
            boost::unique_lock<boost::mutex> lock(mutListClients);
            while(!dataReadyListClients)
               condListClients.wait(lock);
            // TODO: Check if we got the right property we're waiting on
            dataReadyListClients = false;
            Publish(device_addr, PLAYER_MSGTYPE_RESP_ACK, PLAYER_NSDNET_REQ_LISTCLIENTS,
               &respListClients, sizeof(respListClients), NULL);
            return 0;
         }
         else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
            PLAYER_NSDNET_CMD_SEND, device_addr))
         {
            player_nsdnet_send_cmd *cmd = (player_nsdnet_send_cmd *)data;
            std::cout << "NSDNetDriver: Sending message to '" <<
               (strlen(cmd->clientid)?"all":cmd->clientid) << "', " << cmd->msg << std::endl;
            if (strlen(cmd->clientid))
               client->Send(cmd->clientid, cmd->msg_count, cmd->msg);
            else
               client->Send(cmd->msg_count, cmd->msg);
            return 0;
         }
         else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ,
            PLAYER_NSDNET_REQ_PROPGET, device_addr))
         {
            player_nsdnet_propget_req *req = (player_nsdnet_propget_req *)data;
            std::cout << "NSDNetDriver: Got request for property value of " <<
               req->key << std::endl;
            // Clear memory if used already
            if (respPropGet.value)
            {
               delete[] respPropGet.value;
               respPropGet.value_count = 0;
               respPropGet.value = 0;
            }
            // Short circuit if the client id is needed.
            if (!strcmp(req->key, "self.id"))
            {
               memcpy(&respPropGet, req, sizeof(respPropGet));
               respPropGet.value_count = clientID.size() + 1;
               respPropGet.value = new char[respPropGet.value_count];
               strcpy(respPropGet.value, clientID.c_str());
               Publish(device_addr, PLAYER_MSGTYPE_RESP_ACK, PLAYER_NSDNET_REQ_PROPGET,
                  &respPropGet, sizeof(respPropGet), NULL);
               return 0;
            }
            std::cout << "NSDNetDriver: Unhandled key by driver, passing on to daemon " << req->key << std::endl;
            client->PropertyGet(req->key);
            // Block until we know the response.
            boost::unique_lock<boost::mutex> lock(mutPropertyValue);
            while(!dataReadyPropertyValue)
               condPropertyValue.wait(lock);
            dataReadyPropertyValue = false;
            Publish(device_addr, PLAYER_MSGTYPE_RESP_ACK, PLAYER_NSDNET_REQ_PROPGET,
               &respPropGet, sizeof(respPropGet), NULL);
            return 0;
         }
         else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD,
            PLAYER_NSDNET_CMD_PROPSET, device_addr))
         {
            player_nsdnet_propset_cmd *cmd = (player_nsdnet_propset_cmd *)data;
            std::cout << "NSDNetDriver: Send property set for property " << cmd->key <<
               " with value " << cmd->value << std::endl;
            client->PropertySet(cmd->key, cmd->value);
            return 0;
         }
         else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
            PLAYER_POSITION2D_DATA_STATE, position2dAddr))
         {
            player_position2d_data_t *_data = (player_position2d_data_t *) data;
            std::stringstream ss;
            double x, y, a;
            x = _data->pos.px - localizationX;
            y = _data->pos.py - localizationY;
            a = _data->pos.pa - localizationA;
            ss << x << " " << y << " " << a;
            std::cout << "NSDNetDriver: Position2d data message " << ss.str() << std::endl;
            client->PropertySet("self.position", ss.str());
            return 0;
         }
         else if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_DATA,
            PLAYER_POSITION2D_DATA_GEOM, position2dAddr))
         {
            player_position2d_geom_t *geom = (player_position2d_geom_t *) data;
            std::stringstream ss;
            double x, y, z;
            x = geom->pose.px - localizationX;
            y = geom->pose.py - localizationY;
            z = geom->pose.pz - localizationA;
            ss << x << " " << y << " " << z;
            std::cout << "NSDNetDriver: Position2d geom message " << ss.str() << std::endl;
            client->PropertySet("self.position", ss.str());
            return 0;
         }
         return(-1);
      }

      /**
       * Handler is fired when the connection state changes.
       * \param state The new state when the state changes.
       */
      virtual void StateChanged(PlayerNSDClient::ConnectionState state)
      {
         std::stringstream ss;
         switch (state)
         {
            case PlayerNSDClient::StateGreeting:
               std::cout << "NSDNetDriver: Registering with playernsd server with id " << clientID << std::endl;
               client->Register(clientID);
               break;
            case PlayerNSDClient::StateRegistered:
               std::cout << "NSDNetDriver: Registered with playernsd server with id " << clientID << std::endl;
               // Initialisation
               ss << poseX << " " << poseY << " " << poseA;
               //std::cout << "Sending off the initial positions of the the robot of " << clientID << " " << ss.str() << std::endl;
               //client->PropertySet("self.position", ss.str());
               break;
            default:
               break;
         }
      }

      /**
       * Handler is fired when an error message is raised on the server.
       * \param err The error raised.
       * \param message The error message.
       */
      virtual void ErrorRaised(PlayerNSDClient::ServerError err, const std::string& message)
      {
         switch (err)
         {
            case PlayerNSDClient::ServerErrorClientIDInUse:
               clientID += "_";
               std::cout << "Playernsd error: Conflicting client id, retrying with "
                  << clientID << std::endl;
               client->Register(clientID);
               break;
            default:
               std::cout << "Playernsd error: " << message << std::endl;
               break;
         }
      }

      /**
       * Handler is fired when a text message is received.
       * \param source The source of the message.
       * \param data The text message received.
       */
      virtual void Receive(const std::string& source, const std::string& data)
      {
         strncpy(receivedMsg.clientid, source.c_str(), PLAYER_NSDNET_KEY_LEN-1);
         receivedMsg.msg_count = data.length() + 1;
         if (receivedMsg.msg)
            delete[] receivedMsg.msg;
         receivedMsg.msg = new char[receivedMsg.msg_count];
         strncpy(receivedMsg.msg, data.c_str(), receivedMsg.msg_count);
         std::cout << "NSDNetDriver: Received text message from " << source << std::endl;
         Publish(device_addr, PLAYER_MSGTYPE_DATA, PLAYER_NSDNET_DATA_RECV, &receivedMsg,
            sizeof(receivedMsg), NULL);
      }

      /**
       * Handler is fired when a binary message is received.
       * \param source The source of the message.
       * \param len The length of the binary message received.
       * \param data The binary message received.
       */
      virtual void Receive(const std::string& source, int len, const char *data)
      {
         strncpy(receivedMsg.clientid, source.c_str(), PLAYER_NSDNET_KEY_LEN-1);
         receivedMsg.msg_count = len;
         if (receivedMsg.msg)
            delete[] receivedMsg.msg;
         receivedMsg.msg = new char[len];
         memcpy(receivedMsg.msg, data, len);
         std::cout << "NSDNetDriver: Received binary message from " << source << std::endl;
         Publish(device_addr, PLAYER_MSGTYPE_DATA, PLAYER_NSDNET_DATA_RECV, &receivedMsg,
            sizeof(receivedMsg), NULL);
      }

      /**
       * Handler is fired when the response to a client listing is recieved.
       * \param clientList The list of clients received.
       */
      virtual void ClientListResponse(const std::vector<std::string>& clientList)
      {
         boost::lock_guard<boost::mutex> lock(mutListClients);
         respListClients.clients_count = clientList.size() * PLAYER_NSDNET_CLIENTID_LEN;
         if (respListClients.clients)
            delete[] respListClients.clients;
         respListClients.clients = new char[respListClients.clients_count];
         for (unsigned int i = 0; i < clientList.size(); i++)
            strcpy(((clientIDString *)respListClients.clients)[i], clientList[i].c_str());
         dataReadyListClients = true;
         std::cout << "NSDNetDriver: Received client list response." << std::endl;
         condListClients.notify_one();
      }

      /**
       * Handler is fired when the value of a property is received.
       * \param variable The property variable name.
       * \param value The property value.
       */
      virtual void PropertyValue(const std::string& variable, const std::string& value)
      {
         boost::lock_guard<boost::mutex> lock(mutPropertyValue);
         //player_nsdnet_propget_req_t resp;
         // TODO: Make safe.
         std::cout << "Got propval " << variable << ": " << value << std::endl;
         strcpy(respPropGet.key, variable.c_str());
         respPropGet.value_count = value.size();
         if (respPropGet.value)
            delete[] respPropGet.value;
         respPropGet.value = new char[value.size() + 1];
         memset(respPropGet.value, 0, value.size() + 1);
         strcpy(respPropGet.value, value.c_str());
         dataReadyPropertyValue = true;
         std::cout << "NSDNetDriver: Received property value " << variable << " = " << value<< std::endl;
         condPropertyValue.notify_one();
      }

   private:
      std::string worldFile;
      std::string model;
      std::string clientID;
      std::string host;
      std::string port;
      boost::scoped_ptr<PlayerNSDClient> client;
      boost::condition_variable condListClients;
      boost::condition_variable condPropertyValue;
      boost::mutex mutListClients;
      boost::mutex mutPropertyValue;
      bool dataReadyListClients;
      bool dataReadyPropertyValue;
      player_nsdnet_recv_data receivedMsg;
      player_nsdnet_listclients_req_t respListClients;
      player_nsdnet_propget_req_t respPropGet;

      bool hasPosition2d;
      Device *position2dDevice;
      player_devaddr_t position2dAddr;
      double localizationX, localizationY, localizationZ, localizationA;
      double poseX, poseY, poseZ, poseA;
};

/**
 * Factory creation function for creating a nsdnet driver.
 * Declared outside of the class so that it
 * can be invoked without any object context (alternatively, you can
 * declare it static in the class).  In this function, we create and return
 * (as a generic Driver*) a pointer to a new instance of this driver.
 */
Driver *NSDNetDriver_Init(ConfigFile* cf, int section)
{
   // Create and return a new instance of this driver
   return((Driver *)(new NSDNetDriver(cf, section)));
}

/**
 * A driver registration function.
 * Declared outside of the class so
 * that it can be invoked without object context.  In this function, we add
 * the driver into the given driver table, indicating which interface the
 * driver can support and how to create a driver instance.
 */
void NSDNetDriver_Register(DriverTable* table)
{
   table->AddDriver("nsdnetdriver", NSDNetDriver_Init);
}

/**
 * Extra stuff for building a shared object.
 */
extern "C" {
   int player_driver_init(DriverTable* table)
   {
      std::cout << "NSDNetDriver initialising" << std::endl;
      NSDNetDriver_Register(table);
      std::cout << "NSDNetDriver done" << std::endl;
      return(0);
   }
}

