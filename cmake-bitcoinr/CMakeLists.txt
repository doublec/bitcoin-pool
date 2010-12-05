
IF(WIN32)
	ADD_DEFINITIONS(-D__WXMSW__)
ENDIF(WIN32)

SET(BITCOIN_REMOTE_MINER_SRC
	${CMAKE_SOURCE_DIR}/src/remoteminermain.cpp
	${CMAKE_SOURCE_DIR}/src/cryptopp/cpu.cpp
	${CMAKE_SOURCE_DIR}/src/cryptopp/sha.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_reader.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_value.cpp
	${CMAKE_SOURCE_DIR}/src/json/json_spirit_writer.cpp
	${CMAKE_SOURCE_DIR}/src/remote/base64.c
	${CMAKE_SOURCE_DIR}/src/remote/remoteminerclient.cpp
	${CMAKE_SOURCE_DIR}/src/remote/remoteminermessage.cpp
)

SET(BITCOIN_REMOTE_MINER_CUDA_SRC
	${CMAKE_SOURCE_DIR}/src/remote/remoteminerclientcuda.cpp
	${CMAKE_SOURCE_DIR}/src/remote/cuda/bitcoinminercuda.cpp
	${CMAKE_SOURCE_DIR}/src/remote/cuda/bitcoinminercuda.cu
)

IF(BITCOIN_ENABLE_CUDA)
	ADD_DEFINITIONS(-D_BITCOIN_MINER_CUDA_)
	CUDA_ADD_EXECUTABLE(bitcoinr WIN32 ${BITCOIN_REMOTE_MINER_SRC} ${BITCOIN_REMOTE_MINER_CUDA_SRC})
ELSE(BITCOIN_ENABLE_CUDA)
	ADD_EXECUTABLE(bitcoinr ${BITCOIN_REMOTE_MINER_SRC})
ENDIF(BITCOIN_ENABLE_CUDA)

TARGET_LINK_LIBRARIES(bitcoinr ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
IF(WIN32)
	TARGET_LINK_LIBRARIES(bitcoinr winmm.lib shlwapi.lib)
ELSE(WIN32)
	TARGET_LINK_LIBRARIES(bitcoinr pthread)
ENDIF(WIN32)

IF(BITCOIN_ENABLE_CUDA)
	TARGET_LINK_LIBRARIES(bitcoinr ${CUDA_LIBRARIES})
ENDIF(BITCOIN_ENABLE_CUDA)
