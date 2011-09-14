#!/usr/bin/env python

#
# \brief nsdnet client example in python
# \author Tai Chi Minh Ralph Eastwood
# \author University of York
#

import sys
import time
import math
import random
import pickle
from playercpp import *
from nsdnet import *

# takes a parameter for the index
if len(sys.argv) < 2:
  print "Needs one parameter."
  sys.exit(1)
else:
  index = int(sys.argv[1])
  print "Using index", index

# create a client object and connect it
client = PlayerClient('localhost', 6665)

# create a proxy for nsdnet:0
proxy = NSDNetProxy(client, index)
posproxy= Position2dProxy(client, index)

# get our client id
proxy.RequestProperty("self.id")
print 'Client id:', proxy.GetProperty()
# get our index
proxy.RequestProperty("self.index")
print 'Client index:', proxy.GetProperty()

# get a list of clients connected
proxy.RequestClientList()
client.Read()
print 'Client List:'
for cid in proxy.GetClientList():
  print cid

# some random seed
random.seed(time.time())

for i in range(0, 1000):
  # random bot movement every 10
#  if i % 10 == 0:
    #speed = random.random() * 2
    #direction = (random.random() - 0.5) * 120
    #print "Setting speed:", speed, "direction:", direction
    #posproxy.SetSpeed(speed, direction * math.pi / 180.0)

  # Check for messages
  if (client.Peek()):
    # Read player client messages
    client.Read()
    # Get the current position
    px = posproxy.GetXPos()
    py = posproxy.GetYPos()
    pyaw = posproxy.GetYaw()
    # Create a picked tuple of the position
    pp = pickle.dumps(("position", px, py, pyaw))
    # Broadcast position
    proxy.SendMessage(pp)
    # Anything to receive?
    if proxy.ReceiveMessageCount() > 0:
      msg = proxy.ReceiveMessage()
      if msg != None:
        # Unpickle the message
        msgpickle = pickle.loads(msg.message)
        print "%s: %s [%d]" % (msg.source, msgpickle, i)
  # Broadcast Hello World
  proxy.SendMessage(pickle.dumps(("string", "Hello World 1")))
  time.sleep(1.0)


del proxy
del posproxy
del client

# vim: ai:ts=2:sw=2:sts=2:
