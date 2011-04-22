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

#ifndef _rpcminer_client_
#define _rpcminer_client_

#include <string>
#include <vector>

#include "../json/json_spirit.h"
#include "../minercommon/minerheaders.h"
#include "rpcminerthreadcpu.h"
#include "rpcminerthreadgpu.h"
#include "../cryptopp/sha.h"

class RPCMinerClient
{
public:
	RPCMinerClient();
	virtual ~RPCMinerClient();

	virtual void Run(const std::string &url, const std::string &user, const std::string &password, const int threadcount=1);
	void SetServerStatsURL(const std::string &url)	{ m_serverstatsurl=url; }
	void SetWorkRefreshMS(const int ms)				{ m_workrefreshms=ms; }

protected:

	const bool EncodeBase64(const std::vector<unsigned char> &data, std::string &encoded) const;
	const bool DecodeBase64(const std::string &encoded, std::vector<unsigned char> &decoded) const;

	void SendWorkRequest();
	void SendFoundHash(const int64 blockid, const unsigned int nonce);
	const bool GetServerStats(json_spirit::Object &stats);

	void PrintServerStats(json_spirit::Object &stats);

	void SaveBlock(json_spirit::Object &block, const std::string &filename);

	const std::string GetTimeStr(const time_t timet) const;

	void CleanupOldBlocks();

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)
	typedef RPCMinerThreadGPU threadtype;
#else
	typedef RPCMinerThreadCPU threadtype;
#endif

	RPCMinerThreads m_minerthreads;
	int64 m_blockid;
	std::string m_url;
	std::string m_user;
	std::string m_password;
	std::map<int64,std::pair<int64,std::vector<unsigned char> > > m_blocklookup;
	uint256 m_lasttarget;
	std::string m_serverstatsurl;
	int m_workrefreshms;
	int m_threadcount;
};

#endif	// _rpcminer_client_
