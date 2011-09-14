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
