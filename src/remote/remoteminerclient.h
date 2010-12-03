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
#include "../cryptopp/sha.h"

class RemoteMinerClient
{
public:
	RemoteMinerClient();
	virtual ~RemoteMinerClient();

	void Run(const std::string &server, const std::string &port, const std::string &password, const std::string &address);

	const bool Connect(const std::string &server, const std::string &port);
	const bool Disconnect();
	const bool IsConnected() const		{ return m_socket!=INVALID_SOCKET; }

	void SendMessage(const RemoteMinerMessage &message);
	const bool MessageReady() const;
	const bool ProtocolError() const;
	const bool ReceiveMessage(RemoteMinerMessage &message);

private:
#ifdef _WIN32
	static bool m_wsastartup;
#endif

	void SocketSend();
	void SocketReceive();
	const bool Update();

	const bool EncodeBase64(const std::vector<unsigned char> &data, std::string &encoded) const;
	const bool DecodeBase64(const std::string &encoded, std::vector<unsigned char> &decoded) const;

	void SendClientHello(const std::string &password, const std::string &address);
	void SendMetaHash(const int64 blockid, const std::vector<unsigned char> &block, const unsigned int startnonce, const std::vector<unsigned char> &digest, const uint256 &besthash, const unsigned int besthashnonce);
	void SendWorkRequest();
	void SendFoundHash(const int64 blockid, const std::vector<unsigned char> &block, const unsigned int nonce);

	void HandleMessage(const RemoteMinerMessage &message);

	const bool FindGenerationAddressInBlock(const uint160 address, json_spirit::Object &obj, double &amount) const;
	const std::string ReverseAddressHex(const uint160 address) const;

	inline void SHA256Transform(void* pstate, void* pinput, const void* pinit)
	{
		::memcpy(pstate, pinit, 32);
		CryptoPP::SHA256::Transform((CryptoPP::word32*)pstate, (CryptoPP::word32*)pinput);
	}

	int FormatHashBlocks(void* pbuffer, unsigned int len)
	{
		unsigned char* pdata = (unsigned char*)pbuffer;
		unsigned int blocks = 1 + ((len + 8) / 64);
		unsigned char* pend = pdata + 64 * blocks;
		memset(pdata + len, 0, 64 * blocks - len);
		pdata[len] = 0x80;
		unsigned int bits = len * 8;
		pend[-1] = (bits >> 0) & 0xff;
		pend[-2] = (bits >> 8) & 0xff;
		pend[-3] = (bits >> 16) & 0xff;
		pend[-4] = (bits >> 24) & 0xff;
		return blocks;
	}

	uint160 m_address160;

	bool m_gotserverhello;
	bool m_havework;
	std::vector<unsigned char> m_metahashstartblock;
	unsigned int m_metahashstartnonce;
	std::vector<unsigned char> m_metahash;
	std::vector<unsigned char>::size_type m_metahashpos;

	int64 m_nextblockid;
	uint256 m_nexttarget;
	std::vector<unsigned char> m_nextmidstate;
	std::vector<unsigned char> m_nextblock;

	int64 m_currentblockid;
	uint256 m_currenttarget;
	unsigned char m_currentmidbuff[256];
	unsigned char m_currentblockbuff[256];
	unsigned char *m_midbuffptr;
	unsigned char *m_blockbuffptr;
	unsigned int *m_nonce;

	SOCKET m_socket;
	std::vector<char> m_receivebuffer;
	std::vector<char> m_sendbuffer;
	std::vector<char> m_tempbuffer;
	fd_set m_readfs;
	fd_set m_writefs;
	struct timeval m_timeval;
};

#endif	// _remote_miner_client_
