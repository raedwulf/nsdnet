/*
 * nsdnet python bindings
 * \author Tai Chi Minh Ralph Eastwood
 */

%module nsdnet

%{
#include <libplayerc/playerc.h>
#include <libplayerc++/playerclient.h>
#include <libplayerc++/clientproxy.h>
#include <libplayerc++/playerc++.h>
#include <libplayerinterface/player.h>
#include "nsdnetproxy.h"

struct Message
{
        time_t timestamp;
        std::string source;
        std::string message;
};

%}

%include "std_string.i"
%include "std_vector.i"

%import "playercpp.i"

%typemap(in) time_t
{
    if (PyLong_Check($input))
        $1 = (time_t) PyLong_AsLong($input);
    else if (PyInt_Check($input))
        $1 = (time_t) PyInt_AsLong($input);
    else if (PyFloat_Check($input))
        $1 = (time_t) PyFloat_AsDouble($input);
    else {
        PyErr_SetString(PyExc_TypeError,"Expected a large number");
        return NULL;
    }
}

%typemap(out) time_t
{
    $result = PyLong_FromLong((long)$1);
}

namespace std
{
        %template(StringVector) vector<string>;
}

struct Message
{
        time_t timestamp;
        std::string source;
        std::string message;
};

%ignore PlayerCc::NSDNetProxy::ReceiveMessage(time_t& timestamp, std::string& source, std::string& message);
%include "nsdnetproxy.h"

// Attach a ReceiveMessage function to the Proxy class
%extend PlayerCc::NSDNetProxy
{
        Message *PlayerCc::NSDNetProxy::ReceiveMessage()
        {
                Message *msg = new Message();
                if (!self->ReceiveMessage(msg->timestamp, msg->source, msg->message))
                        return 0;
                return msg;
        }
}
%newobject PlayerCc::NSDNetProxy::ReceiveMessage;
