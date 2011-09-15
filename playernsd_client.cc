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
 * \brief playernsd client code.
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 *
 * \section Description
 *
 * This class interfaces with the nsd daemon server and provides some handling
 * of messages before passing them down to the driver.
 */

#include "playernsd_client.h"
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/scoped_array.hpp>

PlayerNSDClient::PlayerNSDClient(PlayerNSDClient::Handler& handler) :
      socket(ioService), connectionState(StateDisconnected), handler(handler)
{
}

PlayerNSDClient::~PlayerNSDClient(void)
{
   Close();
}

bool PlayerNSDClient::Connect(const std::string& host, const std::string& port)
{
   tcp::resolver resolver(ioService);
   tcp::resolver::query query(host, port);
   tcp::resolver::iterator iterator = resolver.resolve(query);

   tcp::endpoint endpoint = *iterator;
   while (iterator != tcp::resolver::iterator())
   {
      try
      {
         socket.connect(endpoint);
         break;
      }
      catch (std::exception& e)
      {
         std::cerr << "Exception: " << e.what() << "\n";
         return false;
      }
      ++iterator;
   }
   changeState(StateConnected);

   this->id = id;
   this->host = host;
   this->port = port;

   // Start threads
   reader = boost::thread(&PlayerNSDClient::processReader, this);
   writer = boost::thread(&PlayerNSDClient::processWriter, this);

   return true;
}

void PlayerNSDClient::Close()
{
   reader.interrupt();
   writer.interrupt();

   boost::system::error_code error;
   std::ostream request_stream(&request);
   request_stream << "bye\n";
   try
   {
      boost::asio::write(socket, request);
   }
   catch (std::exception& e)
   {
      std::cerr << "Exception: " << e.what() << "\n";
   }

   socket.close();
}

void PlayerNSDClient::processReader()
{
   std::cout << "Starting reader..." << std::endl;
   while (true)
   {
      boost::system::error_code error;

      // Read until newline
      //std::cout << id <<  ": begin read command..." << std::endl;
      boost::asio::read_until(socket, response, '\n', error);
      if (error == boost::asio::error::eof)
      {
         std::cout << "Got EOF... stopping reader" << std::endl;
         return;
      }
      else if (error)
      {
         std::cout << "ASIO Error: " << error.message() << std::endl;
         return;
      }

      // Process the greeting.
      std::istream response_stream(&response);
      std::string command;
      std::getline(response_stream, command);

      //std::cout << id <<  ": read command " << command << std::endl;

      // Tokenise the greeting.
      boost::char_separator<char> sep(" ");
      boost::tokenizer<boost::char_separator<char> > toker(command, sep);
      std::vector<std::string> tokens(toker.begin(), toker.end()) ;

      // Handle pinging
      if (tokens[0] == "ping")
      {
         messageSendQueue.push("pong\n");
      }
      else if (tokens[0] == "listclients")
      {
         //tokens.erase(tokens.begin(), tokens.begin()+1);
         tokens.erase(tokens.begin());
         handler.ClientListResponse(tokens);
      }
      else if (connectionState < StateRegistered)
      {
         switch (connectionState)
         {
            case StateConnected:
               if (tokens[0] == "greetings" && tokens[2] == "playernsd")
               {
                  if (tokens[3] == PLAYERNSD_PROTOCOL_VERSION)
                  {
                     protocolVersion = tokens[3];
                     changeState(StateGreeting);
                  }
                  else
                  {
                     throw Exception((std::string("Incompatible protocol [") + tokens[3] + "]"));
                  }
               }
               else
               {
                  throw Exception((std::string("Unexpected server cmd [") + command + "]"));
               }
               break;
            case StateGreeting:
               if (tokens[0] == "error")
               {
                  handler.ErrorRaised(ServerErrorUnknown, command);
               }
               else
               {
                  throw Exception((std::string("Unexpected server cmd [") + command + "]"));
               }
               break;
            case StateWaitingRegistration:
               // Check if it is the Registered command.
               if (tokens[0] == "registered")
               {
                  changeState(StateRegistered);
               }
               else if (tokens[0] == "error")
               {
                  // Check if it is the clientidinuse error, gives a chance for
                  // recovery.
                  if (tokens[1] == "clientidinuse")
                  {
                     // Reset the state to StateGreeting
                     connectionState = StateGreeting;
                     handler.ErrorRaised(ServerErrorClientIDInUse, command);
                  }
                  else
                  {
                     handler.ErrorRaised(ServerErrorUnknown, command);
                  }
               }
               else // Unrecoverable Error.
               {
                  throw Exception((std::string("Server error [") + command + "]").c_str());
               }
               break;
            default:
               throw Exception((std::string("Unexpected server command (state ") +
                  boost::lexical_cast<std::string>(connectionState) + ") [" + command + "]"));
         }
      }
      else
      {
         // Handle pinging
         if (tokens.size() == 0)
         {
            std::cerr << "ERROR: Received message: " << command << std::endl;
            //throw Exception(std::string("Don't know what to do with message without tokens ") + command);;
         }
         else if (tokens[0] == "msgtext")
         {
            boost::asio::read_until(socket, response, '\n', error);
            if (error == boost::asio::error::eof)
            {
               std::cout << "Got EOF... stopping reader" << std::endl;
               return;
            }
            else if (error)
            {
               std::cout << "ASIO Error: " << error.message() << std::endl;
               return;
            }
            // Process the response.
            std::istream response_stream(&response);
            std::string message;
            std::getline(response_stream, message);
            handler.Receive(tokens[1], message);
         }
         else if (tokens[0] == "msgbin")
         {
            if (tokens.size() != 3)
               throw Exception(std::string("Read message error [expected 2 parameters to msgbin]"));
            std::size_t length = boost::lexical_cast<size_t>(tokens[2]);
            if (response.size() < length)
            {
               //std::cout << id << ": expecting " << length - response.size() << std::endl;
               boost::asio::read(socket, response, boost::asio::transfer_at_least(length - response.size()), error);
               if (error == boost::asio::error::eof)
               {
                  std::cout << "Got EOF... stopping reader" << std::endl;
                  return;
               }
               else if (error)
               {
                  std::cout << "ASIO Error: " << error.message() << std::endl;
                  return;
               }
            }
            std::istream response_stream(&response);
            char message[length];
            //std::cout << id << ": response read " << length << std::endl;
            response_stream.read(message, length);
            //std::cout << id << ": response read done " << length << std::endl;
            handler.Receive(tokens[1], length, message);
         }
         else if (tokens[0] == "propval")
         {
            // Remove the first two tokens.
            std::string val = command.substr(tokens[0].size() + tokens[1].size() + 2);
            handler.PropertyValue(tokens[1], val);
         }
         else if (tokens[0] == "error")
         {
            if (tokens[1] == "clientidinuse")
               handler.ErrorRaised(ServerErrorClientIDInUse, command);
            else if (tokens[1] == "invalidparam")
               handler.ErrorRaised(ServerErrorInvalidParameter, command);
            else if (tokens[1] == "invalidparamcount")
               handler.ErrorRaised(ServerErrorInvalidParameterCount, command);
            else if (tokens[1] == "unknowncommand")
               handler.ErrorRaised(ServerErrorUnknownCommand, command);
            else if (tokens[1] == "alreadyregistered")
               handler.ErrorRaised(ServerErrorAlreadyRegistered, command);
            else if (tokens[1] == "propertynotexist")
               handler.ErrorRaised(ServerErrorPropertyNotExist, command);
            else if (tokens[1] == "unknownclient")
               handler.ErrorRaised(ServerErrorUnknownClient, command);
            else
               handler.ErrorRaised(ServerErrorUnknown, command);
         }
         else
         {
            std::cerr << "ERROR: Received message: " << command << std::endl;
            //throw Exception(std::string("Don't know what to do with message[") + tokens[0] + "]" + command);;
         }
      }
      //std::cout << id <<  ": end read command..." << command << std::endl;
   }
}

void PlayerNSDClient::Register(const std::string& clientID)
{
   // Copy the client id.
   this->id = clientID;
   // Send reply greetings.
   if (connectionState == StateGreeting)
   {
      // Set state for waiting registration.
      changeState(StateWaitingRegistration);
      // Write the greeting
      std::ostream request_stream(&request);
      request_stream << "greetings " << clientID <<
         " playernsd " << PLAYERNSD_PROTOCOL_VERSION << "\n";
      try
      {
         boost::asio::write(socket, request);
      }
      catch (std::exception& e)
      {
         std::cerr << "Exception: " << e.what() << "\n";
         return;
      }
   }
   else if (connectionState == StateWaitingRegistration)
   {
      throw Exception("Multiple registrations.");
   }
   else
   {
      throw Exception("Tried to register before greeting received.");
   }
}

void PlayerNSDClient::RequestClientList()
{
   messageSendQueue.push("listclients\n");
}

void PlayerNSDClient::Send(const std::string& target, const std::string& data)
{
   std::string msg("msgtext ");
   msg += target + "\n" + data + "\n";
   messageSendQueue.push(msg);
}

void PlayerNSDClient::Send(const std::string& target, uint32_t len, const char *data)
{
   std::string msg("msgbin ");
   msg += target + " " + boost::lexical_cast<std::string>(len) + "\n" + std::string(data, len);
   messageSendQueue.push(msg);
}

void PlayerNSDClient::Send(const std::string& data)
{
   std::string msg("msgtext\n");
   msg += data + "\n";
   messageSendQueue.push(msg);
}

void PlayerNSDClient::Send(uint32_t len, const char *data)
{
   std::string msg("msgbin ");
   msg += boost::lexical_cast<std::string>(len) + "\n" + std::string(data, len);
   messageSendQueue.push(msg);
}

void PlayerNSDClient::PropertyGet(const std::string& variable)
{
   std::string msg("propget ");
   msg += variable + "\n";
   messageSendQueue.push(msg);
}

void PlayerNSDClient::PropertySet(const std::string& variable, const std::string& value)
{
   std::string msg("propset ");
   msg += variable + " " + value + "\n";
   messageSendQueue.push(msg);
}

void PlayerNSDClient::processWriter()
{
   std::cout << "Starting writer..." << std::endl;
   while (true)
   {
      // Wait on the queue until we have something to send.
      std::string msg = messageSendQueue.pop(true);
      //std::cout << "Sending " << msg << std::endl;
      std::ostream request_stream(&request);
      request_stream << msg;
      boost::system::error_code error;
      try
      {
         boost::asio::write(socket, request);
      }
      catch (std::exception& e)
      {
         std::cerr << "Exception: " << e.what() << "\n";
         return;
      }
   }
}

void PlayerNSDClient::changeState(ConnectionState state)
{
   connectionState = state;
   handler.StateChanged(state);
}


