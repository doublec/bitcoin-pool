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

#ifndef _bitcoin_remote_miner_
#define _bitcoin_remote_miner_

#include "../headers.h"
#include "remoteminermessage.h"
#include "../cryptopp/sha.h"
#include "timestats.h"
#include <vector>
#include <map>
#include <string>

extern const int BITCOINMINERREMOTE_THREADINDEX;
extern const int BITCOINMINERREMOTE_HASHESPERMETA;
#define BITCOINMINERREMOTE_SERVERVERSIONSTR "1.2.2"

void ThreadBitcoinMinerRemote(void* parg);
void BitcoinMinerRemote();

extern CCriticalSection cs_mapTransactions;
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast);
int FormatHashBlocks(void* pbuffer, unsigned int len);
void SHA256Transform(void* pstate, void* pinput, const void* pinit);
bool ProcessBlock(CNode* pfrom, CBlock* pblock);

extern TimeStats timestats;
#define SCOPEDTIME(section)	ScopedTimer scopedtimer(timestats,section);

class RemoteClientConnection
{
public:
	RemoteClientConnection(const SOCKET sock, sockaddr_storage &addr, const int addrlen);
	~RemoteClientConnection();

	const SOCKET GetSocket() const		{ return m_socket; }

	const bool IsConnected() const		{ return m_socket!=INVALID_SOCKET; }
	const bool Disconnect();

	void SendMessage(const RemoteMinerMessage &message);
	const bool MessageReady() const;
	const bool ProtocolError() const;
	const bool ReceiveMessage(RemoteMinerMessage &message);

	void SetRequestedRecipientAddress(const uint160 &recipient)		{ m_recipientaddress=recipient; }
	const bool GetRequestedRecipientAddress(uint160 &recipient)		{ recipient=m_recipientaddress; return m_recipientaddress!=0; }
	const int64 GetCalculatedKHashRateFromBestHash(const int sec=60, const int minsec=60) const;
	const int64 GetCalculatedKHashRateFromMetaHash(const int sec=60, const int minsec=60) const;
	const bool GotClientHello() const								{ return m_gotclienthello; }
	void SetGotClientHello(bool got)								{ m_gotclienthello=got; }

	const time_t GetLastVerifiedMetaHash() const					{ return m_lastverifiedmetahash; }
	void SetLastVerifiedMetaHash(const time_t t)					{ m_lastverifiedmetahash=t; }
	const int64 GetVerifiedMetaHashCount() const					{ return m_verifiedmetahashcount; }

	const std::vector<char>::size_type ReceiveBufferSize() const	{ return m_receivebuffer.size(); }
	const std::vector<char>::size_type SendBufferSize() const		{ return m_sendbuffer.size(); }

	const bool SocketReceive();
	const bool SocketSend();

	const std::string GetAddress(const bool withport=true) const;

	const uint160 GetRecipientAddress() const						{ return m_recipientaddress; }

	int64 &NextBlockID()											{ return m_nextblockid; }

	void ClearOldSentWork(const int sec);

	struct metahash
	{
		metahash():m_verified(false),m_senttime(time(0))	{ }
		std::vector<unsigned char> m_metahash;
		unsigned int m_startnonce;
		bool m_verified;
		time_t m_senttime;
		uint256 m_besthash;
		unsigned int m_besthashnonce;
	};

	struct sentwork
	{
		sentwork():m_pblock(0)			{ }
		
		int64 m_blockid;
		time_t m_senttime;
		std::vector<unsigned char> m_block;
		std::vector<unsigned char> m_midstate;
		uint256 m_target;
		CKey m_key;
		std::vector<metahash> m_metahashes;
		CBlock *m_pblock;
		CBlockIndex *m_indexprev;

		const bool CheckNonceOverlap(const unsigned int nonce, const unsigned int hashespermetahash)
		{
			for(std::vector<metahash>::const_iterator i=m_metahashes.begin(); i!=m_metahashes.end(); i++)
			{
				if((*i).m_startnonce<=nonce && (*i).m_startnonce+hashespermetahash>nonce)
				{
					return true;
				}
			}
			return false;
		}
	};

	const std::vector<sentwork> &GetSentWork() const		{ return m_sentwork; }
	std::vector<sentwork> &GetSentWork()					{ return m_sentwork; }
	const bool GetSentWorkByBlock(const std::vector<unsigned char> &block, sentwork **work);
	const bool GetSentWorkByID(const int64 id, sentwork **work);
	const bool GetNewestSentWorkWithMetaHash(sentwork &work) const;
	void SetWorkVerified(const int64 id, const int64 mhindex, const bool valid);

	const std::vector<char>::size_type GetReceiveBufferSize() const	{ return m_receivebuffer.size(); }
	const std::vector<char>::size_type GetSendBufferSize() const	{ return m_sendbuffer.size(); }

private:
	SOCKET m_socket;
	struct sockaddr_storage m_addr;
	int m_addrlen;

	std::vector<char> m_tempbuffer;
	std::vector<char> m_receivebuffer;
	std::vector<char> m_sendbuffer;

	std::vector<sentwork> m_sentwork;

	time_t m_connecttime;
	time_t m_lastactive;
	time_t m_lastverifiedmetahash;
	bool m_gotclienthello;
	uint160 m_recipientaddress;

	int64 m_nextblockid;
	int64 m_verifiedmetahashcount;

};

class MetaHashVerifier
{
public:
	MetaHashVerifier();
	~MetaHashVerifier();

	void Start(RemoteClientConnection *client, const RemoteClientConnection::sentwork &work);
	void Step(const int hashes=10000);

	RemoteClientConnection *GetClient()				{ return m_client; }
	void ClearClient()								{ m_client=0; }

	const bool Done() const							{ return m_done; }
	const bool Verified() const						{ return m_verified; }
	const int64 GetWorkID() const					{ return m_workid; }
	const int64 GetMetaHashIndex() const			{ return m_mhindex; }

private:

	int64 m_workid;
	int64 m_mhindex;
	bool m_done;
	bool m_verified;
	uint256 m_tempbuff[3];
	uint256 m_hashbuff[3];
	uint256 *m_temphash;
	uint256 *m_hash;
	unsigned char m_midbuff[256];
	unsigned char m_blockbuff[256];
	unsigned char *m_midbuffptr;
	unsigned char *m_blockbuffptr;
	unsigned int *m_nonce;
	unsigned int m_startnonce;
	std::vector<unsigned char> m_digest;
	std::vector<unsigned char> m_metahash;
	std::vector<unsigned char>::size_type m_metahashpos;
	RemoteClientConnection *m_client;

};

class BitcoinMinerRemoteServer
{
public:
	BitcoinMinerRemoteServer();
	~BitcoinMinerRemoteServer();

	const bool StartListen(const std::string &bindaddr="127.0.0.1", const std::string &bindport="8335");
	const bool Step();

	std::vector<RemoteClientConnection *> &Clients()		{ return m_clients; }

	void SendServerHello(RemoteClientConnection *client, const int metahashrate);
	void SendWork(RemoteClientConnection *client);
	void SendServerStatus();
	void SendWorkToAllClients();

	static const bool EncodeBase64(const std::vector<unsigned char> &data, std::string &encoded);
	static const bool DecodeBase64(const std::string &encoded, std::vector<unsigned char> &decoded);

	const int64 GetAllClientsCalculatedKHashFromMeta() const;
	const int64 GetAllClientsCalculatedKHashFromBest() const;
	
	RemoteClientConnection *GetOldestNonVerifiedMetaHashClient();

	int64 &GeneratedCount()													{ return m_generatedcount; }

	void LoadContributedHashes();
	void SaveContributedHashes();

	void AddContributedHashes(const uint160 address, const int64 hashes)	{ m_currenthashescontributed[address]+=hashes; }
	void ClearCurrentHashesContributed()									{ m_previoushashescontributed=m_currenthashescontributed; m_currenthashescontributed.clear(); }

private:
	void BlockToJson(const CBlock *block, json_spirit::Object &obj);
	void ReadBanned(const std::string &filename);

	void AddDistributionFromConnected(CBlock *pblock, CBlockIndex *pindexPrev, int64 nFees);
	void AddDistributionFromContributed(CBlock *pblock, CBlockIndex *pindexPrev, int64 nFees);

	CBigNum m_bnExtraNonce;

#ifdef _WIN32
	static bool m_wsastartup;
#endif
	std::string m_distributiontype;
	std::vector<SOCKET> m_listensockets;
	std::vector<RemoteClientConnection *> m_clients;
	std::map<uint160,uint256> m_previoushashescontributed;	// number of hashes each address contributed to the previous block solve
	std::map<uint160,uint256> m_currenthashescontributed;		// number of hashes each address contributed since last block solve
	std::set<std::string> m_banned;
	time_t m_startuptime;
	int64 m_generatedcount;

};

#endif	// _bitcoin_remote_miner_
