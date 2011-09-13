CMAKE_MINIMUM_REQUIRED (VERSION 2.4 FATAL_ERROR)
PROJECT (libnsdnet)

# Set the package version
SET (LIBRARY_VERSION 1.0 CACHE STRING "libnsdnet version")

# Include this CMake module to get most of the settings needed to build
SET (CMAKE_MODULE_PATH "@CMAKE_INSTALL_PREFIX@/share/cmake/Modules")
INCLUDE (UsePlayerPlugin)
INCLUDE (UsePlayerC)
INCLUDE (UsePlayerC++)

FIND_PACKAGE(Threads)
FIND_PACKAGE(Boost COMPONENTS unit_test_framework filesystem system thread)

# Settings
SET (CMAKE_CXX_FLAGS "-Wall")
SET (LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
SET (LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib${LIB_SUFFIX}" CACHE STRING "Directory where lib will install")
SET (INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "The directory the headers are installed in")
SET (CMAKE_MODULES_INSTALL_DIR "${CMAKE_ROOT}/Modules" CACHE PATH "The directory to install FindNSDNet.cmake to")

# Source, includes and libraries to build with
INCLUDE_DIRECTORIES (${PROJECT_BINARY_DIR})
PLAYER_ADD_PLUGIN_INTERFACE (nsdnet 320_nsdnet.def SOURCES dev_nsdnet.c)
# Note the use of files generated during the PLAYER_ADD_PLUGIN_INTERFACE step
PLAYER_ADD_PLUGIN_DRIVER (nsdnet_driver SOURCES nsdnet_driver.cc playernsd_client.cc nsdnet_interface.h nsdnet_xdr.h)
PLAYER_ADD_PLAYERC_CLIENT (nsdnet_client SOURCES examples/example_client.c nsdnet_interface.h)
PLAYER_ADD_PLAYERCPP_CLIENT (nsdnet_client_cpp SOURCES examples/example_client.cc nsdnetproxy.h)
TARGET_LINK_LIBRARIES (nsdnet_client nsdnet)
TARGET_LINK_LIBRARIES (nsdnet_client_cpp nsdnet)
TARGET_LINK_LIBRARIES (nsdnet_driver ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

# Generate SWIG Python bindings
FIND_PACKAGE(SWIG REQUIRED)
INCLUDE(${SWIG_USE_FILE})

FIND_PACKAGE(PythonLibs)
INCLUDE_DIRECTORIES(${PYTHON_INCLUDE_PATH})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

SET(CMAKE_SWIG_FLAGS "")

SET_SOURCE_FILES_PROPERTIES(nsdnet.i PROPERTIES CPLUSPLUS ON)
#SET_SOURCE_FILES_PROPERTIES(nsdnet.i PROPERTIES SWIG_FLAGS "-includeall")
SWIG_ADD_MODULE(nsdnet python nsdnet.i)
SWIG_LINK_LIBRARIES(nsdnet ${PYTHON_LIBRARIES} nsdnet ${PLAYERCPP_LINK_LIBS})

# Install the project
MESSAGE (STATUS "${PROJECT_NAME} version ${LIBRARY_VERSION} will be installed to:")
MESSAGE (STATUS "  ${LIB_INSTALL_DIR}")
INSTALL (TARGETS nsdnet_driver LIBRARY DESTINATION ${LIB_INSTALL_DIR})
INSTALL (TARGETS nsdnet LIBRARY DESTINATION ${LIB_INSTALL_DIR})
INSTALL (FILES examples/nsdnet_example.cfg DESTINATION "share/stage/worlds")
INSTALL (FILES examples/nsdnet_example.world DESTINATION "share/stage/worlds")
INSTALL (FILES examples/nsdnet_position_example.cfg DESTINATION "share/stage/worlds")

# Copy Python scripts
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/example_client.py
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_client.py ${CMAKE_CURRENT_BINARY_DIR}/example_client.py
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/example_client.py
)
ADD_CUSTOM_TARGET(python_clients DEPENDS example_client.py)
ADD_DEPENDENCIES(nsdnet_client python_clients)

# Install SWIG Python Bindings
EXECUTE_PROCESS (COMMAND ${PYTHON_EXECUTABLE} -V
	ERROR_VARIABLE pythonVersionString
	ERROR_STRIP_TRAILING_WHITESPACE)
STRING (REGEX REPLACE "^Python ([0-9]+\\.[0-9]+).*" "\\1" pythonVersion ${pythonVersionString})
SET (pythonInstallDir ${LIB_INSTALL_DIR}/python${pythonVersion}/site-packages)
SET (PYTHON_BINDINGS_INSTALL_DIR ${pythonInstallDir} CACHE PATH "Python bindings installation directory under $prefix")
INSTALL (FILES ${CMAKE_CURRENT_BINARY_DIR}/nsdnet.py
         ${CMAKE_CURRENT_BINARY_DIR}/_nsdnet.so
         DESTINATION ${PYTHON_BINDINGS_INSTALL_DIR})
