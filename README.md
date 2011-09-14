NSDNet 0.0.1
============

Introduction
------------

This is a driver for [Player][1] to communicate between the [Player][1] client
and the [PlayerNSD][2] daemon.

For licensing details, please see the COPYING file.

 [1]: http://playerstage.sourceforge.net/index.php?src=player
 [2]: http://github.com/raedwulf/playernsd

Dependencies
------------

* [Player][1] (tested with 3.0.2)
* [CMake][3] (tested with 2.8.1)
* [PlayerNSD][2] (tested with 0.0.1)
* [Python][4] (for python example, tested with 2.7)
* [SWIG][5] (for compiling python bindings)

 [3]: http://www.cmake.org/
 [4]: http://www.python.org/
 [5]: http://www.swig.org/

Building
--------

This assumes you have followed the steps required to compile and run player/stage,
using the environment variable $PSINSTALLPATH to specify the installation directory
of player/stage (on prepackaged binaries this is usually /usr on Unix systems).

	$ cd nsdnet
	$ mkdir -p build
	$ cd build
	$ cmake .. -DCMAKE_MODULE_PATH=$PSINSTALLPATH/share/cmake/Modules -DCMAKE_INSTALL_PREFIX=$PSINSTALLPATH
	$ make && make install

### Problems

There seems to be an issue with the Player C++ bindings on some systems.
The current workaround (if you do not require C++ examples) has been to disable those
examples for the time being.

Running
-------

NSDnet creates three objects: an interface plugin, a driver plugin for
the new interface, and a client program that uses the new interface in
conjunction with the plugin driver.

Before executing player, playernsd need to be executed.

	$ playernsd/playernsd

The interface plugin and driver plugin are loaded by the nsdnet_example.cfg
configuration file. A supplied example, nsdnet_example.cfg, can be loaded as follows:

	$ player $PSINSTALLPATH/share/stage/worlds/nsdnet_example.cfg

In a separate terminal, execute the client program:

	$ ./nsdnet_client id

The id is a number, (``0`` or ``1``) to specify which node you want to run.

You should see commands, data and requests moving between the client and the
driver.
With a second client, you will be able to see communication between the two.

Examples
--------

### Config files

The general additions to the Player config files are that you include an interface
block:

	interface
	(
		name "nsdnet"
		code 320
		plugin "libnsdnet"
	)

For each robot, you typically would need to include a driver for the network
communication. You need at least ``plugin``, ``id`` and ``provides`` keys as follows:

	driver
	(
		name "nsdnetdriver"
		plugin "libnsdnet_driver"
		provides ["nsdnet:0"]
		id "node0"
	)

When the position is needed to be known, it is retrieved from a position2d driver
specified by a line such as ``uses ["position2d:0"]`` in the ``nsdnetdriver`` driver block.

Please see complete examples
[examples/nsdnet_example.cfg][1] and [example/nsdnet_position_example.cfg][2] for examples.

 [1]: http://github.com/raedwulf/nsdnet/blob/master/examples/nsdnet_example.cfg
 [2]: http://github.com/raedwulf/nsdnet/blob/master/examples/nsdnet_position_example.cfg

### Client proxy

There are examples for a number of platforms [C][1], [C++][2], [Python][3].

 [4]: http://github.com/raedwulf/nsdnet/blob/master/examples/example_client.c
 [5]: http://github.com/raedwulf/nsdnet/blob/master/examples/example_client.cc
 [6]: http://github.com/raedwulf/nsdnet/blob/master/examples/example_client.py

The python example has more features, and will be described here:

An example python client typically needs to import the ``playercpp`` and ``nsdnet``
libraries in order to run.  An example of this is:

```python
	import sys
	import time
	import math
	import random
	import pickle
	from playercpp import *
	from nsdnet import *
```

As the client may be one of many, it needs to identify which nsdnet driver it is using.
The example makes use of a single integer ([nsdnet_position_example.cfg][2] has 2
devices, 0 or 1).  Using this, the client can initialise.

```python
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
```

The client can get some properties from the PlayerNSD daemon, these usually need to be
defined in scripts or simulations running on the daemon.  However, ``self.id`` is
always defined as the network id of the client.

```python
	# get our client id
	proxy.RequestProperty("self.id")
	print 'Client id:', proxy.GetProperty()
	# get our index
	proxy.RequestProperty("self.index")
	print 'Client index:', proxy.GetProperty()
```

To get a list of clients:
```python
	# get a list of clients connected
	proxy.RequestClientList()
	client.Read()
	print 'Client List:'
	for cid in proxy.GetClientList():
		print cid
```

The the main loop of the client proxy is straightfoward:
```python
	while True:
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
```

TODO
----
The documentation using Doxygen is yet incomplete.
