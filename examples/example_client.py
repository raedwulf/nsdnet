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

# set callbacks
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
  # Get geometry of position
  #posproxy.RequestGeom()
  # check the messages
  if (client.Peek()):
    client.Read()
    if proxy.ReceiveMessageCount() > 0:
      msg = proxy.ReceiveMessage()
      if msg != None:
        print "%s: %s [%d]" % (msg.source, msg.message, i)
    time.sleep(0)
  else:
    print "Sending Hello World"
    proxy.SendMessage("Hello World")
    time.sleep(0.1)

del proxy
del posproxy
del client

# vim: ai:ts=2:sw=2:sts=2:
