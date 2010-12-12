/**
    Copyright (C) 2010  puddinpop

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**/

#ifndef _remote_miner_client_
#define _remote_miner_client_

#include <string>
#include <vector>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#include "remotebitcoinheaders.h"
#include "remoteminermessage.h"
#include "remoteminerthreadcpu.h"
#include "remoteminerthreadgpu.h"
#include "../cryptopp/sha.h"

class RemoteMinerClient
{
public:
	RemoteMinerClient();
	virtual ~RemoteMinerClient();

	virtual void Run(const std::string &server, const std::string &port, const std::string &password, const std::string &address, const int threadcount=1);

	const bool Connect(const std::string &server, const std::string &port);
	const bool Disconnect();
	const bool IsConnected() const		{ return m_socket!=INVALID_SOCKET; }

	void SendMessage(const RemoteMinerMessage &message);
	const bool MessageReady() const;
	const bool ProtocolError() const;
	const bool ReceiveMessage(RemoteMinerMessage &message);

protected:
#ifdef _WIN32
	static bool m_wsastartup;
#endif

	void SocketSend();
	void SocketReceive();
	const bool Update(const int ms=100);

	const bool EncodeBase64(const std::vector<unsigned char> &data, std::string &encoded) const;
	const bool DecodeBase64(const std::string &encoded, std::vector<unsigned char> &decoded) const;

	void SendClientHello(const std::string &password, const std::string &address);
	void SendMetaHash(const int64 blockid, const unsigned int startnonce, const std::vector<unsigned char> &digest, const uint256 &besthash, const unsigned int besthashnonce);
	void SendWorkRequest();
	void SendFoundHash(const int64 blockid, const unsigned int nonce);

	void HandleMessage(const RemoteMinerMessage &message);

	const bool FindGenerationAddressInBlock(const uint160 address, json_spirit::Object &obj, double &amount) const;
	const std::string ReverseAddressHex(const uint160 address) const;

	void SaveBlock(json_spirit::Object &block, const std::string &filename);

	uint160 m_address160;
	bool m_gotserverhello;
	SOCKET m_socket;
	std::vector<char> m_receivebuffer;
	std::vector<char> m_sendbuffer;
	std::vector<char> m_tempbuffer;
	fd_set m_readfs;
	fd_set m_writefs;
	struct timeval m_timeval;
	unsigned int m_metahashsize;

/*
#if  defined(_BITCOIN_MINER_CUDA_)
	RemoteMinerThreadCUDA m_minerthread;
#elif defined(_BITCOIN_MINER_OPENCL_)
	RemoteMinerThreadOpenCL m_minerthread;
#else
	RemoteMinerThreadCPU m_minerthread;
#endif
	*/

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)
	typedef RemoteMinerThreadGPU threadtype;
#else
	typedef RemoteMinerThreadCPU threadtype;
#endif

	RemoteMinerThreads m_minerthreads;
};

#endif	// _remote_miner_client_
