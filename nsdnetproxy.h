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
 * \brief nsdnet client proxy for libplayerc++
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 */

#ifndef _NSDNETPROXY_H_
#define _NSDNETPROXY_H_

#include "dev_nsdnet.h"
#include <libplayerc/playerc.h>
#include <libplayercommon/playercommon.h>
#include <libplayerc++/playerc++.h>
#include <vector>
#include <string>

#ifndef PLAYERCC_EXPORT
#define PLAYERCC_EXPORT
#endif

extern "C" playerxdr_function_t* player_plugininterf_gettable (void);

namespace PlayerCc
{

/**
The @p NSDNetProxy allows communication through player to the playernsd */
class PLAYERCC_EXPORT NSDNetProxy : public ClientProxy
{
   private:

      // libplayerc data structure
      nsdnet_t *device;
      // client list that easily passable in c++ & python
      std::vector<std::string> clientList;
      // property value
      std::string propertyValue;

   public:

      /// Constructor subscrives to the device.
      NSDNetProxy(PlayerClient *aPc, int index=0)
         : ClientProxy(aPc, index),
         device(NULL)
      {
         scoped_lock_t lock(mPc->mMutex);
         // Load the plugin interface
         if (playerc_add_xdr_ftable(player_plugininterf_gettable(), 0) < 0)
            throw PlayerError("Could not add xdr functions\n");
         device = nsdnet_create(mClient, index);
         device->user = this;
         if (!device)
            throw PlayerError("NSDNetProxy::NSDNetProxy()", "could not create");
         if (nsdnet_subscribe(device, PLAYER_OPEN_MODE))
            throw PlayerError("NSDNetProxy::NSDNetProxy()", "could not subscribe");
         // how can I get this into the clientproxy.cc?
         // right now, we're dependent on knowing its device type
         mInfo = &(device->info);
      }

      /// destructor
      ~NSDNetProxy()
      {
         assert(device);
         scoped_lock_t lock(mPc->mMutex);
         nsdnet_unsubscribe(device);
         nsdnet_destroy(device);
         device = NULL;
      }

      /// Send a message.
      void SendMessage(const std::string target, int len, const char *message)
      {
         scoped_lock_t lock(mPc->mMutex);
         if (nsdnet_send_message(this->device, target.c_str(), len, (char *)message))
            throw PlayerError("NSDNetProxy::SendMessage()", "error sending message");
      }

      /// Send a message (broadcast).
      void SendMessage(int len, const char *message)
      {
         scoped_lock_t lock(mPc->mMutex);
         if (nsdnet_send_message(this->device, NULL, len, (char *)message))
            throw PlayerError("NSDNetProxy::SendMessage()", "error sending message");
      }

      /// Send a message using std::string.
      void SendMessage(const std::string &target, const std::string &message)
      {
         SendMessage(target, message.length() + 1, message.c_str());
      }

      /// Send a message using std::string (broadcast).
      void SendMessage(const std::string &message)
      {
         SendMessage(message.length() + 1, message.c_str());
      }

      /// Received message.
      bool ReceiveMessage(time_t& timestamp, std::string& source, std::string& message)
      {
         scoped_lock_t lock(mPc->mMutex);
         int err;
         nsdmsg_t *msg;
         if ((err = nsdnet_receive_message(this->device, &msg)))
         {
            if (err < 0)
               throw PlayerError("NSDNetProxy::ReceiveMessage()", "error receiving message");
            else
            {
               timestamp = msg->timestamp;
               message = std::string(msg->msg, msg->msg_count);
               source = std::string(msg->clientid);
               return true;
            }
         }
         return false;
      }

      /// Get Last Error message
      bool GetLastErrorMessage(int& code, std::string& error)
      {
         scoped_lock_t lock(mPc->mMutex);
         if (this->device->error_code)
         {
            code = this->device->error_code;
            error = std::string(this->device->error_msg, this->device->error_msg_count);
            return true;
         }
         else
            return false;
      }

      /// Receive message count.
      int ReceiveMessageCount()
      {
         return device->queue_head - device->queue_tail;
      }

      /// Request a property value.
      void RequestProperty(const std::string& variable)
      {
         scoped_lock_t lock(mPc->mMutex);
         if (nsdnet_property_get(this->device, variable.c_str()))
            throw PlayerError("NSDNetProxy::RequestProperty()", "error requesting property");
      }

      /// Get a property value.
      std::string GetProperty()
      {
         scoped_lock_t lock(mPc->mMutex);
         propertyValue.assign(this->device->propval, strlen(this->device->propval));
         return propertyValue;
      }

      /// Set a property Value.
      void SetProperty(const std::string& variable, const std::string& value)
      {
         scoped_lock_t lock(mPc->mMutex);
         if (nsdnet_property_set(this->device, variable.c_str(), value.c_str()))
            throw PlayerError("NSDNetProxy::SetProperty()", "error setting property");
      }

      /// Request a list of clients.
      void RequestClientList()
      {
         scoped_lock_t lock(mPc->mMutex);
         if (nsdnet_get_listclients(this->device))
            throw PlayerError("NSDNetProxy::RequestClientList()", "error requesting client list");
         // Convert the retrieved client list into a string vector.
         this->clientList.clear();
         for (int i = 0; i < this->device->listclients_count; i++)
            this->clientList.push_back(this->device->listclients[i]);

      }

      /// Get the list of clients.
      const std::vector<std::string>& GetClientList()
      {
         scoped_lock_t lock(mPc->mMutex);
         return this->clientList;
      }
};

}

#endif
