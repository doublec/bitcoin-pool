
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

ADD_EXECUTABLE(bitcoinr ${BITCOIN_REMOTE_MINER_SRC})

TARGET_LINK_LIBRARIES(bitcoinr ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
IF(WIN32)
	TARGET_LINK_LIBRARIES(bitcoinr winmm.lib shlwapi.lib)
ENDIF(WIN32)
