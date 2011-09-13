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

Running
-------

NSDnet creates three objects: an interface plugin, a driver plugin for
the new interface, and a client program that uses the new interface in
conjunction with the plugin driver.

Before executing player, playernsd need to be executed.

 $ playernsd/playernsd

The interface plugin and driver plugin are loaded by the example.cfg
configuration file. Copy this to the directory where you built the example and
execute Player:

 $ player nsdnet_example.cfg

In a separate terminal, execute the client program:

 $ ./nsdnet_client id

The id is a number, (0 or 1) to specify which node you want to run.

You should see commands, data and requests moving between the client and the
driver.
With a second client, you will be able to see communication between the two.

Examples
--------

### Config files

### Client proxy

TODO
----
The documentation using Doxygen is yet incomplete.
