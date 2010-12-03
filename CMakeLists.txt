CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(bitcoin)

OPTION(BITCOIN_ENABLE_CUDA "Enable CUDA miner" ON)
OPTION(BITCOIN_ENABLE_OPENCL "Enable OpenCL miner" OFF)
OPTION(BITCOIN_BUILD_GUI "Build GUI (bitcoin)" ON)
OPTION(BITCOIN_BUILD_DAEMON "Build Daemon (bitcoind)" ON)
OPTION(BITCOIN_BUILD_REMOTE_MINER "Build remote miner (bitcoinr)" ON)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules/" ${CMAKE_MODULE_PATH})

SET(BITCOIN_BASE_SRC
	${CMAKE_SOURCE_DIR}/src/db.cpp
	${CMAKE_SOURCE_DIR}/src/init.cpp
	${CMAKE_SOURCE_DIR}/src/irc.cpp
	${CMAKE_SOURCE_DIR}/src/main.cpp
	${CMAKE_SOURCE_DIR}/src/net.cpp
	${CMAKE_SOURCE_DIR}/src/rpc.cpp
	${CMAKE_SOURCE_DIR}/src/script.cpp
	${CMAKE_SOURCE_DIR}/src/sha256.cpp
	${CMAKE_SOURCE_DIR}/src/util.cpp
	${CMAKE_SOURCE_DIR}/src/cryptopp/cpu.cpp
	${CMAKE_SOURCE_DIR}/src/cryptopp/sha.cpp
	${CMAKE_SOURCE_DIR}/src/gpucommon/gpucommon.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_reader.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_value.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_writer.cpp
	${CMAKE_SOURCE_DIR}/src/remote/base64.c
	${CMAKE_SOURCE_DIR}/src/remote/remoteminer.cpp
	${CMAKE_SOURCE_DIR}/src/remote/remoteminermessage.cpp
)

IF(WIN32)
	SET(BITCOIN_BASE_SRC ${BITCOIN_BASE_SRC} ${CMAKE_SOURCE_DIR}/src/ui.rc)
ENDIF(WIN32)

SET(BITCOIN_GUI_SRC
	${CMAKE_SOURCE_DIR}/src/ui.cpp
	${CMAKE_SOURCE_DIR}/src/uibase.cpp
)

SET(BITCOIN_CUDA_SRC
	${CMAKE_SOURCE_DIR}/src/cuda/bitcoinminercuda.cpp
	${CMAKE_SOURCE_DIR}/src/cuda/bitcoinminercuda.cu
)

SET(BITCOIN_OPENCL_SRC
	${CMAKE_SOURCE_DIR}/src/opencl/bitcoinmineropencl.cpp
)



IF(BITOIN_ENABLE_CUDA AND BITCOIN_ENABLE_OPENCL)
	MESSAGE(FATAL_ERROR "You can only enable CUDA or OpenCL, not both")
ENDIF(BITOIN_ENABLE_CUDA AND BITCOIN_ENABLE_OPENCL)

IF(NOT BITCOIN_BUILD_GUI AND NOT BITCOIN_BUILD_DAEMON)
	MESSAGE(FATAL_ERROR "Nothing to build.  You must build either the GUI or Daemon.")
ENDIF(NOT BITCOIN_BUILD_GUI AND NOT BITCOIN_BUILD_DAEMON)







# Find all the libraries we will need (Boost, wxWidgets, BDB, OpenSSL, and optionally CUDA or OpenCL)

# Use Boot module built into CMake
SET(Boost_USE_STATIC_LIBS   ON)
SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost REQUIRED date_time filesystem program_options regex system thread)

IF(Boost_FOUND)
	INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
ENDIF(Boost_FOUND)

# Use wxWidgets module built into CMake
IF(BITCOIN_BUILD_GUI)
	SET(wxWidgets_USE_DEBUG OFF)
	SET(wxWidgets_USE_UNICODE ON)
	SET(wxWidgets_USE_UNIVERSAL ON)
	SET(wxWidgets_USE_STATIC ON)
	
	FIND_PACKAGE(wxWidgets REQUIRED base core adv html)
	
	IF(wxWidgets_FOUND)
		INCLUDE_DIRECTORIES(${wxWidgets_INCLUDE_DIRS})
	ENDIF(wxWidgets_FOUND)
ENDIF(BITCOIN_BUILD_GUI)

# Use OpenSSL module built into CMake
FIND_PACKAGE(OpenSSL REQUIRED)

IF(OPENSSL_FOUND)
	INCLUDE_DIRECTORIES(${OPENSSL_INCLUDE_DIR})
ENDIF(OPENSSL_FOUND)

# Use BDB local module
FIND_PACKAGE(BerkeleyDB)

IF(DB_INCLUDE_DIR)
	INCLUDE_DIRECTORIES(${DB_INCLUDE_DIR})
ELSE(DB_INCLUDE_DIR)
	MESSAGE(FATAL_ERROR "Could not locate BerkeleyDB")
ENDIF(DB_INCLUDE_DIR)

# Use fixed CUDA local module (2.8.2 standard one won't work in Windows when declaring WIN32)
IF(BITCOIN_ENABLE_CUDA)
	FIND_PACKAGE(CUDA REQUIRED)
	IF(CUDA_FOUND)
		INCLUDE_DIRECTORIES(${CUDA_INCLUDE_DIRS})
	ENDIF(CUDA_FOUND)
ENDIF(BITCOIN_ENABLE_CUDA)

# Use OpenCL local module
IF(BITCOIN_ENABLE_OPENCL)
	FIND_PACKAGE(OpenCL REQUIRED)
	IF(OpenCL_FOUND)
		INCLUDE_DIRECTORIES(${OPENCL_INCLUDE_DIR})
	ENDIF(OpenCL_FOUND)
ENDIF(BITCOIN_ENABLE_OPENCL)



IF(BITCOIN_BUILD_GUI)
	ADD_SUBDIRECTORY(cmake-bitcoin)
ENDIF(BITCOIN_BUILD_GUI)

IF(BITCOIN_BUILD_DAEMON)
	ADD_SUBDIRECTORY(cmake-bitcoind)
ENDIF(BITCOIN_BUILD_DAEMON)

IF(BITCOIN_BUILD_REMOTE_MINER)
	ADD_SUBDIRECTORY(cmake-bitcoinr)
ENDIF(BITCOIN_BUILD_REMOTE_MINER)
