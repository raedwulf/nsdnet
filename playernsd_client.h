/**
 * \brief playernsd client code.
 * \author Tai Chi Minh Ralph Eastwood
 * \author University of York
 *
 * \section Description
 *
 * This class interfaces with the nsd daemon server and provides some handling
 * of messages before passing them down to the driver.
 */

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <exception>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include "boost/locking_queue.hpp"

using boost::asio::ip::tcp;

#define PLAYERNSD_PROTOCOL_VERSION "0001"

class PlayerNSDClient
{
   public:
      enum ConnectionState
      {
         StateDisconnected,
         StateConnected,
         StateGreeting,
         StateWaitingRegistration,
         StateRegistered,
      };

      enum ServerError
      {
         ServerErrorUnknown,
         ServerErrorClientIDInUse,
         ServerErrorInvalidParameter,
         ServerErrorInvalidParameterCount,
         ServerErrorUnknownCommand,
         ServerErrorAlreadyRegistered,
         ServerErrorUnknownClient,
         ServerErrorPropertyNotExist,
      };

      class Handler
      {
         public:
            virtual void ErrorRaised(ServerError err, const std::string& message) = 0;
            virtual void Receive(const std::string& source, const std::string& data) = 0;
            virtual void Receive(const std::string& source, int len, const char *data) = 0;
            virtual void ClientListResponse(const std::vector<std::string>& clientList) = 0;
            virtual void PropertyValue(const std::string& variable, const std::string& value) = 0;
            virtual void StateChanged(ConnectionState state) = 0;
      };

      PlayerNSDClient(Handler& handler);
      ~PlayerNSDClient(void);
      bool Connect(const std::string& host, const std::string &port);
      void Close();
      void Register(const std::string &clientID);
      void Send(const std::string& target, const std::string& data);
      void Send(const std::string& target, uint32_t len, const char *data);
      void Send(const std::string& data);
      void Send(uint32_t len, const char *data);
      void PropertyGet(const std::string& variable);
      void PropertySet(const std::string& variable, const std::string& value);
      void RequestIP(const std::string &target);
      void RequestClientList();
      ConnectionState GetConnectionState() { return connectionState; }
      class Exception : public std::exception
      {
         public:
            Exception(const char* s = "playernsd error") : msg(s) {}
            Exception(std::string s) : msg(s) {}
            virtual ~Exception() throw() {}
            virtual const char* what() const throw() { return msg.c_str(); }
         private:
            std::string msg;
      };

   private:
      boost::asio::io_service ioService;
      tcp::socket socket;
      boost::thread reader, writer;
   protected:
      ConnectionState connectionState;
      std::string protocolVersion;
      Exception exception;
      boost::locking_queue<std::string> messageSendQueue;
      std::string id, host, port;

   private:
      void processReader();
      void processWriter();
      void changeState(ConnectionState state);

      Handler& handler;
      boost::asio::io_service io_service;
      boost::asio::streambuf request;
      boost::asio::streambuf response;
};

