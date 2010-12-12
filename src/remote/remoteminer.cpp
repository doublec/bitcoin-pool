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

#define NOMINMAX

#include "remoteminer.h"
#include "base64.h"
#include "../cryptopp/misc.h"

#include <ctime>
#include <cstring>
#include <map>
#include <sstream>

#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <netdb.h>
#endif

const int BITCOINMINERREMOTE_THREADINDEX=5;
const int BITCOINMINERREMOTE_HASHESPERMETA=2000000;

TimeStats timestats("timestats.txt",600000);

#ifdef _WIN32
bool BitcoinMinerRemoteServer::m_wsastartup=false;
#endif

RemoteClientConnection::RemoteClientConnection(const SOCKET sock, sockaddr_storage &addr, const int addrlen):m_socket(sock),m_addr(addr),m_addrlen(addrlen),m_gotclienthello(false),m_connecttime(time(0)),m_lastactive(time(0)),m_lastverifiedmetahash(0),m_nextblockid(1),m_verifiedmetahashcount(0)
{
	m_tempbuffer.resize(8192,0);
}

RemoteClientConnection::~RemoteClientConnection()
{
	Disconnect();
	for(std::vector<sentwork>::iterator i=m_sentwork.begin(); i!=m_sentwork.end(); i++)
	{
		if((*i).m_pblock!=0)
		{
			delete (*i).m_pblock;
		}
	}
}

void RemoteClientConnection::ClearOldSentWork(const int sec)
{
	for(std::vector<sentwork>::iterator i=m_sentwork.begin(); i!=m_sentwork.end();)
	{
		if(difftime(time(0),(*i).m_senttime)>=sec)
		{
			if((*i).m_pblock)
			{
				delete (*i).m_pblock;
			}
			i=m_sentwork.erase(i);
		}
		else
		{
			i++;
		}
	}
}

const bool RemoteClientConnection::Disconnect()
{
	m_sendbuffer.clear();
	m_receivebuffer.clear();
	if(IsConnected())
	{
		myclosesocket(m_socket);
	}
	m_socket=INVALID_SOCKET;
	m_gotclienthello=false;
	return true;
}

const std::string RemoteClientConnection::GetAddress(const bool withport) const
{
	std::string address("");
#ifdef _WIN32
	std::vector<TCHAR> buffer(128,0);
	DWORD bufferlen=buffer.size();
	WSAAddressToString((sockaddr *)&m_addr,m_addrlen,0,&buffer[0],&bufferlen);
	if(bufferlen>0)
	{
		address.append(buffer.begin(),buffer.begin()+bufferlen);
	}
#else
	std::vector<char> buffer(128,0);
	int bufferlen=buffer.size();
	struct sockaddr *sa=(sockaddr *)&m_addr;
	switch(sa->sa_family)
	{
	case AF_INET:
			inet_ntop(AF_INET,&(((struct sockaddr_in *)sa)->sin_addr),&buffer[0],bufferlen);
			break;
	case AF_INET6:
			inet_ntop(AF_INET6,&(((struct sockaddr_in6 *)sa)->sin6_addr),&buffer[0],bufferlen);
			break;
	}
	if(bufferlen>0)
	{
		address.append(buffer.begin(),buffer.begin()+bufferlen);
	}
#endif

	if(withport==false)
	{
		std::string::size_type pos=address.find_last_of(':');
		if(pos!=std::string::npos)
		{
			address.erase(pos);
		}
	}

	return address;
}

const int64 RemoteClientConnection::GetCalculatedKHashRateFromBestHash(const int sec, const int minsec) const
{
	SCOPEDTIME("RemoteClientConnection::GetCalculatedKHashRateFromBestHash");
	// we find how many hashes were necessary to get the best hash, and total this number for all best hashes reported in the time period
	// then we divide by the time period to get khash/s
	int64 hashes=0;
	int64 bits=0;
	time_t now=time(0);
	time_t earliest=now;

	for(std::vector<sentwork>::const_iterator i=m_sentwork.begin(); i!=m_sentwork.end(); i++)
	{
		for(std::vector<metahash>::const_iterator j=(*i).m_metahashes.begin(); j!=(*i).m_metahashes.end(); j++)
		{
			if(difftime(now,(*j).m_senttime)<=sec)
			{
				if(earliest>(*j).m_senttime)
				{
					earliest=(*j).m_senttime;
				}
				// find number of 0 bits on the right
				//bits=0;
				for(int i=7; i>=0; i--)
				{
					unsigned int ch=((unsigned int *)&(*j).m_besthash)[i];
					for(int j=31; j>=0; j--)
					{
						if(((ch >> j) & 0x1)!=0)
						{
							i=0;
							j=0;
							continue;
						}
						else
						{
							bits++;
						}
					}
				}
				//hashes+=static_cast<int64>(static_cast<int64>(2) << bits);
				hashes++;
			}
		}
	}

	long double averagenbits=0;
	if(hashes>0)
	{
		averagenbits=static_cast<long double>(bits)/static_cast<long double>(hashes);
	}
	return static_cast<int64>(::pow(static_cast<long double>(2),averagenbits)/static_cast<long double>(1000));
}

const int64 RemoteClientConnection::GetCalculatedKHashRateFromMetaHash(const int sec, const int minsec) const
{
	SCOPEDTIME("RemoteClientConnection::GetCalculatedKHashRateFromMetaHash");
	int64 hash=0;
	time_t now=time(0);
	time_t earliest=now;
	for(std::vector<sentwork>::const_iterator i=m_sentwork.begin(); i!=m_sentwork.end(); i++)
	{
		for(std::vector<metahash>::const_iterator j=(*i).m_metahashes.begin(); j!=(*i).m_metahashes.end(); j++)
		{
			if(difftime(now,(*j).m_senttime)<=sec)
			{
				if(earliest>(*j).m_senttime)
				{
					earliest=(*j).m_senttime;
				}
				hash+=BITCOINMINERREMOTE_HASHESPERMETA;
			}
		}
	}

	int s=(std::min)(static_cast<int>(now-earliest),sec);
	s=(std::max)(s,minsec);
	int64 denom=(static_cast<int64>(s)*static_cast<int64>(1000));
	if(denom!=0)
	{
		return hash/denom;
	}
	else
	{
		return 0;
	}
}

const bool RemoteClientConnection::GetNewestSentWorkWithMetaHash(sentwork &work) const
{
	SCOPEDTIME("RemoteClientConnection::GetNewestSentWorkWithMetaHash");
	bool found=false;
	time_t newest=0;
	for(std::vector<sentwork>::const_iterator i=m_sentwork.begin(); i!=m_sentwork.end(); i++)
	{
		if((*i).m_senttime>=newest && (*i).m_metahashes.size()>0)
		{
			work=(*i);
			newest=(*i).m_senttime;
			found=true;
		}
	}
	return found;
}

const bool RemoteClientConnection::GetSentWorkByBlock(const std::vector<unsigned char> &block, sentwork **work)
{
	SCOPEDTIME("RemoteClientConnection::GetSentWorkByBlock");
	for(std::vector<sentwork>::reverse_iterator i=m_sentwork.rbegin(); i!=m_sentwork.rend(); i++)
	{
		if((*i).m_block==block)
		{
			*work=&(*i);
			return true;
		}
	}

	return false;
}

const bool RemoteClientConnection::GetSentWorkByID(const int64 id, sentwork **work)
{
	SCOPEDTIME("RemoteClientConnection::GetSentWorkByID");
	for(std::vector<sentwork>::reverse_iterator i=m_sentwork.rbegin(); i!=m_sentwork.rend(); i++)
	{
		if((*i).m_blockid==id)
		{
			*work=&(*i);
			return true;
		}
	}

	return false;
}

const bool RemoteClientConnection::MessageReady() const
{
	SCOPEDTIME("RemoteClientConnection::MessageReady");
	return RemoteMinerMessage::MessageReady(m_receivebuffer);
}

const bool RemoteClientConnection::ProtocolError() const
{
	SCOPEDTIME("RemoteClientConnection::ProtocolError");
	return RemoteMinerMessage::ProtocolError(m_receivebuffer);
}

const bool RemoteClientConnection::ReceiveMessage(RemoteMinerMessage &message)
{
	SCOPEDTIME("RemoteClientConnection::ReceiveMessage");
	return RemoteMinerMessage::ReceiveMessage(m_receivebuffer,message);
}

void RemoteClientConnection::SendMessage(const RemoteMinerMessage &message)
{
	SCOPEDTIME("RemoteClientConnection::SendMessage");
	message.PushWireData(m_sendbuffer);
}

void RemoteClientConnection::SetWorkVerified(const int64 id, const int64 mhindex, const bool valid)
{
	SCOPEDTIME("RemoteClientConnection::SetWorkVerified");
	if(mhindex>=0)
	{
		for(std::vector<sentwork>::reverse_iterator i=m_sentwork.rbegin(); i!=m_sentwork.rend(); i++)
		{
			if((*i).m_blockid==id && (*i).m_metahashes.size()>mhindex)
			{
				(*i).m_metahashes[mhindex].m_verified=true;
				if(valid)
				{
					m_verifiedmetahashcount++;
				}
				return;
			}
		}
	}
}

const bool RemoteClientConnection::SocketReceive()
{
	SCOPEDTIME("RemoteClientConnection::SocketReceive");
	bool received=false;
	if(IsConnected())
	{
		int rval=::recv(GetSocket(),&m_tempbuffer[0],m_tempbuffer.size(),0);
		if(rval>0)
		{
			m_receivebuffer.insert(m_receivebuffer.end(),m_tempbuffer.begin(),m_tempbuffer.begin()+rval);
			received=true;
			m_lastactive=time(0);
		}
		else
		{
			Disconnect();
		}
	}
	return received;
}

const bool RemoteClientConnection::SocketSend()
{
	SCOPEDTIME("RemoteClientConnection::SocketSend");
	bool sent=false;
	if(IsConnected() && m_sendbuffer.size()>0)
	{
		int rval=::send(GetSocket(),&m_sendbuffer[0],m_sendbuffer.size(),0);
		if(rval>0)
		{
			m_sendbuffer.erase(m_sendbuffer.begin(),m_sendbuffer.begin()+rval);
			m_lastactive=time(0);
		}
		else
		{
			Disconnect();
		}
	}
	return sent;
}





MetaHashVerifier::MetaHashVerifier():m_done(false),m_client(0)
{

}

MetaHashVerifier::~MetaHashVerifier()
{
	
}

void MetaHashVerifier::Start(RemoteClientConnection *client, const RemoteClientConnection::sentwork &work)
{
	SCOPEDTIME("MetaHashVerifier::Start");
	m_temphash=alignup<16>(m_tempbuff);
	m_hash=alignup<16>(m_hashbuff);
	m_midbuffptr=alignup<16>(m_midbuff);
	m_blockbuffptr=alignup<16>(m_blockbuff);
	m_nonce=(unsigned int *)(m_blockbuffptr+12);
	m_metahash.clear();
	m_metahash.resize(BITCOINMINERREMOTE_HASHESPERMETA,0);
	m_metahashpos=0;

	m_client=client;
	m_workid=work.m_blockid;

	for(int i=0; i<3; i++)
	{
		m_tempbuff[i]=0;
		m_hashbuff[i]=0;
	}
	*m_temphash=0;
	*m_hash=0;

	m_mhindex=work.m_metahashes.size()-1;

	FormatHashBlocks(m_temphash,sizeof(uint256));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)m_temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)m_temphash)[i]);
	}

	::memcpy(m_blockbuffptr,&(work.m_block[0]),work.m_block.size());
	::memcpy(m_midbuffptr,&(work.m_midstate[0]),work.m_midstate.size());

	m_startnonce=work.m_metahashes[m_mhindex].m_startnonce;
	m_digest=work.m_metahashes[m_mhindex].m_metahash;

	m_done=false;

}



void MetaHashVerifier::Step(const int hashes)
{
	SCOPEDTIME("MetaHashVerifier::Step");
	const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	const unsigned int startpos=m_metahashpos;

	for((*m_nonce)=m_startnonce+m_metahashpos; (*m_nonce)<(m_startnonce+startpos+hashes) && (*m_nonce)<m_startnonce+BITCOINMINERREMOTE_HASHESPERMETA; (*m_nonce)++,m_metahashpos++)
	{
		SHA256Transform(m_temphash,m_blockbuffptr,m_midbuffptr);
		SHA256Transform(m_hash,m_temphash,SHA256InitState);
		
		m_metahash[m_metahashpos]=((unsigned char *)m_hash)[0];
	}

	if(m_metahashpos>=m_metahash.size())
	{
		std::vector<unsigned char> digest(SHA256_DIGEST_LENGTH,0);
		SHA256(&m_metahash[0],m_metahash.size(),&digest[0]);
		
		m_verified=(digest==m_digest);
		m_done=true;
	}
}




BitcoinMinerRemoteServer::BitcoinMinerRemoteServer():m_bnExtraNonce(0),m_startuptime(0),m_generatedcount(0),m_distributiontype("connected")
{
#ifdef _WIN32
	if(m_wsastartup==false)
	{
		WSAData wsadata;
		WSAStartup(MAKEWORD(2,2),&wsadata);
		m_wsastartup=true;
	}
#endif
	ReadBanned("banned.txt");

	if(mapArgs.count("-distributiontype")>0)
	{
		m_distributiontype=mapArgs["-distributiontype"];
		if(m_distributiontype!="connected" && m_distributiontype!="contributed")
		{
			m_distributiontype="connected";
		}
	}
	printf("BitcoinMinerRemoteServer distribution method %s\n",m_distributiontype.c_str());

	LoadContributedHashes();

	if(mapArgs.count("-resethashescontributed")>0)
	{
		m_previoushashescontributed.clear();
		m_currenthashescontributed.clear();
	}

};

BitcoinMinerRemoteServer::~BitcoinMinerRemoteServer()
{
	printf("BitcoinMinerRemoteServer::~BitcoinMinerRemoteServer()\n");

	// stop listening
	for(std::vector<SOCKET>::iterator i=m_listensockets.begin(); i!=m_listensockets.end(); i++)
	{
		myclosesocket((*i));
	}

	// disconnect all clients
	for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		(*i)->Disconnect();
		delete (*i);
	}

	SaveContributedHashes();
};

void BitcoinMinerRemoteServer::AddDistributionFromConnected(CBlock *pblock, CBlockIndex *pindexPrev, int64 nFees)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::AddDistributionFromConnected");
	std::map<uint160,int64> addressamountmap;

	// add output for each connected client proportional to their khash
	for(std::vector<RemoteClientConnection *>::const_iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		uint160 ch=0;
		if((*i)->GetRequestedRecipientAddress(ch))
		{
			if((*i)->GetCalculatedKHashRateFromMetaHash()>0 && GetAllClientsCalculatedKHashFromMeta()>0)
			{
				double khashfrac=static_cast<double>((*i)->GetCalculatedKHashRateFromMetaHash())/static_cast<double>(GetAllClientsCalculatedKHashFromMeta());
				int64 thisvalue=GetBlockValue(pindexPrev->nHeight+1, nFees)*khashfrac;
				if(thisvalue>pblock->vtx[0].vout[0].nValue)
				{
					thisvalue=pblock->vtx[0].vout[0].nValue;
				}
				addressamountmap[ch]+=thisvalue;

				pblock->vtx[0].vout[0].nValue-=thisvalue;
				
			}
		}
	}

	for(std::map<uint160,int64>::const_iterator i=addressamountmap.begin(); i!=addressamountmap.end(); i++)
	{
		CTxOut out;
		out.scriptPubKey << OP_DUP << OP_HASH160 << (*i).first << OP_EQUALVERIFY << OP_CHECKSIG;
		out.nValue=(*i).second;
		pblock->vtx[0].vout.push_back(out);
	}
}

void BitcoinMinerRemoteServer::AddDistributionFromContributed(CBlock *pblock, CBlockIndex *pindexPrev, int64 nFees)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::AddDistributionFromContributed");
	CBigNum numerator=0;
	CBigNum denominator=0;
	std::map<uint160,uint256> hashes;
	std::map<uint160,uint256> addressamountmap;

	// add up all contributing hashes
	hashes=m_currenthashescontributed;
	for(std::map<uint160,uint256>::const_iterator i=hashes.begin(); i!=hashes.end(); i++)
	{
		denominator+=CBigNum((*i).second);
	}

	// if we just solved the current block, the current hashes contributed will be empty, so we need to look at the old hashes contributed
	if(denominator<=0)
	{
		hashes=m_previoushashescontributed;
		for(std::map<uint160,uint256>::const_iterator i=hashes.begin(); i!=hashes.end(); i++)
		{
			denominator+=CBigNum((*i).second);
		}
	}

	// now calculate and add distribution
	if(denominator>0)
	{
		for(std::map<uint160,uint256>::const_iterator i=hashes.begin(); i!=hashes.end(); i++)
		{
			if((*i).second>0)
			{
				CBigNum thisvaluebn=GetBlockValue(pindexPrev->nHeight+1, nFees);
				// do it this way so we don't need any decimal numbers
				// * numerator (this clients hashes) / denominator (all clients hashes)
				thisvaluebn*=CBigNum((*i).second);
				thisvaluebn/=CBigNum(denominator);

				// TODO - find better way to go from CBigNum to int64
				int64 thisvalue;
				std::istringstream istr(thisvaluebn.ToString());
				istr >> thisvalue;
				if(thisvalue>pblock->vtx[0].vout[0].nValue)
				{
					thisvalue=pblock->vtx[0].vout[0].nValue;
				}

				pblock->vtx[0].vout[0].nValue-=thisvalue;

				CTxOut out;
				out.scriptPubKey << OP_DUP << OP_HASH160 << (*i).first << OP_EQUALVERIFY << OP_CHECKSIG;
				out.nValue=thisvalue;
				pblock->vtx[0].vout.push_back(out);

			}
		}
	}

}

void BitcoinMinerRemoteServer::BlockToJson(const CBlock *block, json_spirit::Object &obj)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::BlockToJson");
	obj.push_back(json_spirit::Pair("hash", block->GetHash().ToString().c_str()));
	obj.push_back(json_spirit::Pair("ver", block->nVersion));
	obj.push_back(json_spirit::Pair("prev_block", block->hashPrevBlock.ToString().c_str()));
	obj.push_back(json_spirit::Pair("mrkl_root", block->hashMerkleRoot.ToString().c_str()));
	obj.push_back(json_spirit::Pair("time", (uint64_t)block->nTime));
	obj.push_back(json_spirit::Pair("bits", (uint64_t)block->nBits));
	obj.push_back(json_spirit::Pair("nonce", (uint64_t)block->nNonce));
	obj.push_back(json_spirit::Pair("n_tx", (int)block->vtx.size()));

	json_spirit::Array tx;
	for (int i = 0; i < block->vtx.size(); i++) {
		json_spirit::Object txobj;

	txobj.push_back(json_spirit::Pair("hash", block->vtx[i].GetHash().ToString().c_str()));
	txobj.push_back(json_spirit::Pair("ver", block->vtx[i].nVersion));
	txobj.push_back(json_spirit::Pair("vin_sz", (int)block->vtx[i].vin.size()));
	txobj.push_back(json_spirit::Pair("vout_sz", (int)block->vtx[i].vout.size()));
	txobj.push_back(json_spirit::Pair("lock_time", (uint64_t)block->vtx[i].nLockTime));

	json_spirit::Array tx_vin;
	for (int j = 0; j < block->vtx[i].vin.size(); j++) {
		json_spirit::Object vino;

		json_spirit::Object vino_outpt;

		vino_outpt.push_back(json_spirit::Pair("hash",
    		block->vtx[i].vin[j].prevout.hash.ToString().c_str()));
		vino_outpt.push_back(json_spirit::Pair("n", (uint64_t)block->vtx[i].vin[j].prevout.n));

		vino.push_back(json_spirit::Pair("prev_out", vino_outpt));

		if (block->vtx[i].vin[j].prevout.IsNull())
    		vino.push_back(json_spirit::Pair("coinbase", HexStr(
			block->vtx[i].vin[j].scriptSig.begin(),
			block->vtx[i].vin[j].scriptSig.end(), false).c_str()));
		else
    		vino.push_back(json_spirit::Pair("scriptSig", 
			block->vtx[i].vin[j].scriptSig.ToString().c_str()));
		if (block->vtx[i].vin[j].nSequence != UINT_MAX)
    		vino.push_back(json_spirit::Pair("sequence", (uint64_t)block->vtx[i].vin[j].nSequence));

		tx_vin.push_back(vino);
	}

	json_spirit::Array tx_vout;
	for (int j = 0; j < block->vtx[i].vout.size(); j++) {
		json_spirit::Object vouto;

		vouto.push_back(json_spirit::Pair("value",
    		(double)block->vtx[i].vout[j].nValue / (double)COIN));
		vouto.push_back(json_spirit::Pair("scriptPubKey", 
		block->vtx[i].vout[j].scriptPubKey.ToString().c_str()));

		tx_vout.push_back(vouto);
	}

	txobj.push_back(json_spirit::Pair("in", tx_vin));
	txobj.push_back(json_spirit::Pair("out", tx_vout));

	tx.push_back(txobj);
	}

	obj.push_back(json_spirit::Pair("tx", tx));

	json_spirit::Array mrkl;
	for (int i = 0; i < block->vMerkleTree.size(); i++)
		mrkl.push_back(block->vMerkleTree[i].ToString().c_str());

	obj.push_back(json_spirit::Pair("mrkl_tree", mrkl));
}

const bool BitcoinMinerRemoteServer::DecodeBase64(const std::string &encoded, std::vector<unsigned char> &decoded)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::DecodeBase64");
	if(encoded.size()>0)
	{
		int dlen=((encoded.size()*3)/4)+4;
		decoded.resize(dlen,0);
		std::vector<unsigned char> src(encoded.begin(),encoded.end());
		if(base64_decode(&decoded[0],&dlen,&src[0],src.size())==0)
		{
			decoded.resize(dlen);
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		decoded.resize(0);
		return true;
	}
}

const bool BitcoinMinerRemoteServer::EncodeBase64(const std::vector<unsigned char> &data, std::string &encoded)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::EncodeBase64");
	if(data.size()>0)
	{
		int dstlen=((data.size()*4)/3)+4;
		std::vector<unsigned char> dst(dstlen,0);
		if(base64_encode(&dst[0],&dstlen,&data[0],data.size())==0)
		{
			dst.resize(dstlen);
			encoded.assign(dst.begin(),dst.end());
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		encoded=std::string("");
		return true;
	}
}

const int64 BitcoinMinerRemoteServer::GetAllClientsCalculatedKHashFromBest() const
{
	SCOPEDTIME("BitcoinMinerRemoteServer::GetAllClientsCalculatedKHashFromBest");
	int64 rval=0;
	for(std::vector<RemoteClientConnection *>::const_iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		rval+=(*i)->GetCalculatedKHashRateFromBestHash(600);
	}
	return rval;
}

const int64 BitcoinMinerRemoteServer::GetAllClientsCalculatedKHashFromMeta() const
{
	SCOPEDTIME("BitcoinMinerRemoteServer::GetAllClientsCalculatedKHashFromMeta");
	int64 rval=0;
	for(std::vector<RemoteClientConnection *>::const_iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		rval+=(*i)->GetCalculatedKHashRateFromMetaHash();
	}
	return rval;
}

RemoteClientConnection *BitcoinMinerRemoteServer::GetOldestNonVerifiedMetaHashClient()
{
	SCOPEDTIME("BitcoinMinerRemoteServer::GetOldestNonVerifiedMetaHashClient");
	RemoteClientConnection *client=0;
	time_t oldest=time(0);
	for(std::vector<RemoteClientConnection *>::const_iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		if((*i)->GetLastVerifiedMetaHash()<=oldest)
		{
			client=(*i);
			oldest=(*i)->GetLastVerifiedMetaHash();	
		}
	}
	return client;
}

void BitcoinMinerRemoteServer::LoadContributedHashes()
{
	CWalletDB walletdb;
	std::string settingval("");
	std::string::size_type pos=0;

	m_previoushashescontributed.clear();
	m_currenthashescontributed.clear();

	if(walletdb.ReadSetting("rs_previoushashes",settingval)==true)
	{

		printf("Loading previous contributed hashes %s\n",settingval.c_str());

		pos=0;
		while(pos!=std::string::npos)
		{
			uint160 address;
			uint256 count;

			pos=settingval.find("*");
			if(pos!=std::string::npos)
			{
				address.SetHex(settingval.substr(0,pos));
				settingval.erase(0,pos+1);

				pos=settingval.find("|");
				if(pos==std::string::npos)
				{
					count.SetHex(settingval);
				}
				else
				{
					count.SetHex(settingval.substr(0,pos));
					settingval.erase(0,pos+1);
				}

				if(address!=0 && count!=0)
				{
					m_previoushashescontributed[address]=count;
				}

			}
		}

	}

	if(walletdb.ReadSetting("rs_currenthashes",settingval)==true)
	{

		printf("Loading current contributed hashes %s\n",settingval.c_str());
		
		pos=0;
		while(pos!=std::string::npos)
		{
			uint160 address;
			uint256 count;

			pos=settingval.find("*");
			if(pos!=std::string::npos)
			{
				address.SetHex(settingval.substr(0,pos));
				settingval.erase(0,pos+1);

				pos=settingval.find("|");
				if(pos==std::string::npos)
				{
					count.SetHex(settingval);
				}
				else
				{
					count.SetHex(settingval.substr(0,pos));
					settingval.erase(0,pos+1);
				}

				if(address!=0 && count!=0)
				{
					m_currenthashescontributed[address]=count;
				}

			}
		}

	}

}

void BitcoinMinerRemoteServer::ReadBanned(const std::string &filename)
{
	std::vector<char> buff(129,0);
	std::string host("");
	FILE *infile=fopen("banned.txt","r");
	if(infile)
	{
		while(fgets(&buff[0],buff.size()-1,infile))
		{
			host="";
			for(std::vector<char>::iterator i=buff.begin(); i!=buff.end() && (*i)!=0 && (*i)!='\r' && (*i)!='\n'; i++)
			{
				host+=(*i);
			}
			if(host!="")
			{
				m_banned.insert(host);
			}
		}
		fclose(infile);
	}
}

void BitcoinMinerRemoteServer::SaveContributedHashes()
{
	CWalletDB walletdb;

	std::string saveval("");
	for(std::map<uint160,uint256>::const_iterator i=m_previoushashescontributed.begin(); i!=m_previoushashescontributed.end(); i++)
	{
		if(i!=m_previoushashescontributed.begin())
		{
			saveval+="|";
		}
		saveval+=(*i).first.ToString()+"*"+(*i).second.ToString();
	}
	printf("Saving previous contributed hashes %s\n",saveval.c_str());
	walletdb.WriteSetting("rs_previoushashes",saveval);

	saveval="";
	for(std::map<uint160,uint256>::const_iterator i=m_currenthashescontributed.begin(); i!=m_currenthashescontributed.end(); i++)
	{
		if(i!=m_currenthashescontributed.begin())
		{
			saveval+="|";
		}
		saveval+=(*i).first.ToString()+"*"+(*i).second.ToString();
	}
	printf("Saving current contributed hashes %s\n",saveval.c_str());
	walletdb.WriteSetting("rs_currenthashes",saveval);
}

const bool BitcoinMinerRemoteServer::StartListen(const std::string &bindaddr, const std::string &bindport)
{

	SOCKET sock;
	int rval;
	struct addrinfo hint,*result,*current;
	result=current=NULL;
	memset(&hint,0,sizeof(hint));
	hint.ai_socktype=SOCK_STREAM;
	hint.ai_protocol=IPPROTO_TCP;
	hint.ai_flags=AI_PASSIVE;

	m_startuptime=time(0);
	
	rval=getaddrinfo(bindaddr.c_str(),bindport.c_str(),&hint,&result);
	if(rval==0)
	{
		for(current=result; current!=NULL; current=current->ai_next)
		{
			sock=socket(current->ai_family,current->ai_socktype,current->ai_protocol);
			if(sock!=INVALID_SOCKET)
			{
				#ifndef _WIN32
				const int optval=1;
				setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
				#endif
				if(::bind(sock,current->ai_addr,current->ai_addrlen)==0)
				{
					if(listen(sock,10)==0)
					{
						m_listensockets.push_back(sock);
					}
					else
					{
						myclosesocket(sock);
					}
				}
				else
				{
					myclosesocket(sock);
				}
			}
		}
	}

	if(result)
	{
		freeaddrinfo(result);
	}

	if(m_listensockets.size()==0)
	{
		printf("Remote server couldn't listen on any interface\n");
	}
	else
	{
		printf("Remote server listening on %d interfaces\n",m_listensockets.size());
	}

	return (m_listensockets.size()>0);
};

void BitcoinMinerRemoteServer::SendServerHello(RemoteClientConnection *client, const int metahashrate)
{
	json_spirit::Object obj;
	obj.push_back(json_spirit::Pair("type",static_cast<int>(RemoteMinerMessage::MESSAGE_TYPE_SERVERHELLO)));
	obj.push_back(json_spirit::Pair("serverversion",BITCOINMINERREMOTE_SERVERVERSIONSTR));
	obj.push_back(json_spirit::Pair("metahashrate",static_cast<int>(metahashrate)));
	obj.push_back(json_spirit::Pair("distributiontype",m_distributiontype));
	client->SendMessage(RemoteMinerMessage(obj));
}

void BitcoinMinerRemoteServer::SendServerStatus()
{
	json_spirit::Object obj;
	obj.push_back(json_spirit::Pair("type",static_cast<int>(RemoteMinerMessage::MESSAGE_TYPE_SERVERSTATUS)));
	obj.push_back(json_spirit::Pair("time",static_cast<int64>(time(0))));
	obj.push_back(json_spirit::Pair("clients",static_cast<int64>(m_clients.size())));
	obj.push_back(json_spirit::Pair("khashmeta",static_cast<int64>(GetAllClientsCalculatedKHashFromMeta())));
	obj.push_back(json_spirit::Pair("khashbest",static_cast<int64>(GetAllClientsCalculatedKHashFromBest())));
	obj.push_back(json_spirit::Pair("sessionstartuptime",static_cast<int64>(m_startuptime)));
	obj.push_back(json_spirit::Pair("sessionblocksgenerated",static_cast<int64>(m_generatedcount)));
	for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		json_spirit::Object messobj(obj);
		messobj.push_back(json_spirit::Pair("yourkhashmeta",(*i)->GetCalculatedKHashRateFromMetaHash()));
		messobj.push_back(json_spirit::Pair("yourkhashbest",(*i)->GetCalculatedKHashRateFromBestHash()));
		(*i)->SendMessage(RemoteMinerMessage(messobj));
	}
}

void BitcoinMinerRemoteServer::SendWork(RemoteClientConnection *client)
{
	SCOPEDTIME("BitcoinMinerRemoteServer::SendWork");
	const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	// we don't use CReserveKey for now because it removes the reservation when the key goes out of scope
	CKey key;
	key.MakeNewKey();

	CBlockIndex* pindexPrev = pindexBest;
	unsigned int nBits = GetNextWorkRequired(pindexPrev);
	CTransaction txNew;

	txNew.vin.resize(1);
	txNew.vin[0].prevout.SetNull();
	txNew.vin[0].scriptSig << nBits << ++m_bnExtraNonce;
	txNew.vout.resize(1);
	txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

    //
    // Create new block
    //
    CBlock *pblock=new CBlock();
    if(!pblock)
    {
		return;
	}

    // Add our coinbase tx as first transaction
    pblock->vtx.push_back(txNew);

	// Collect memory pool transactions into the block
	int64 nFees = 0;
	CRITICAL_BLOCK(cs_main)
	CRITICAL_BLOCK(cs_mapTransactions)
	{
		CTxDB txdb("r");
		map<uint256, CTxIndex> mapTestPool;
		vector<char> vfAlreadyAdded(mapTransactions.size());
		uint64 nBlockSize = 1000;
		int nBlockSigOps = 100;
		bool fFoundSomething = true;
		while (fFoundSomething)
		{
			fFoundSomething = false;
			unsigned int n = 0;
			for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n)
			{
				if (vfAlreadyAdded[n])
					continue;
				CTransaction& tx = (*mi).second;
				if (tx.IsCoinBase() || !tx.IsFinal())
					continue;
				unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
				if (nBlockSize + nTxSize >= MAX_BLOCK_SIZE_GEN)
					continue;
				int nTxSigOps = tx.GetSigOpCount();
				if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
					continue;

				// Transaction fee based on block size
				int64 nMinFee = tx.GetMinFee(nBlockSize);

				map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
				if (!tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), pindexPrev, nFees, false, true, nMinFee))
					continue;
				swap(mapTestPool, mapTestPoolTmp);

				pblock->vtx.push_back(tx);
				nBlockSize += nTxSize;
				nBlockSigOps += nTxSigOps;
				vfAlreadyAdded[n] = true;
				fFoundSomething = true;
			}
		}
	}
	pblock->nBits = nBits;
	pblock->vtx[0].vout[0].nValue = GetBlockValue(pindexPrev->nHeight+1, nFees);

	if(m_distributiontype=="connected")
	{
		AddDistributionFromConnected(pblock,pindexPrev,nFees);
	}
	else
	{
		AddDistributionFromContributed(pblock,pindexPrev,nFees);
	}

	printf("Sending block to remote client  nBits=%u\n",pblock->nBits);
	pblock->print();

	unsigned int blocksize=::GetSerializeSize(*pblock, SER_NETWORK);
	if(blocksize > MAX_BLOCK_SIZE)
	{
		printf("ERROR - this block is too big to be accepted!\n");
	}
	else
	{
		printf("Serialized block is %u bytes\n",blocksize);
	}

	//
	// Prebuild hash buffer
	//
	struct tmpworkspace
	{
		struct unnamed2
		{
			int nVersion;
			uint256 hashPrevBlock;
			uint256 hashMerkleRoot;
			unsigned int nTime;
			unsigned int nBits;
			unsigned int nNonce;
		}
		block;
		unsigned char pchPadding0[64];
		uint256 hash1;
		unsigned char pchPadding1[64];
	};
	char tmpbuf[sizeof(tmpworkspace)+64];
	tmpworkspace& tmp = *(tmpworkspace*)alignup<16>(tmpbuf);

	tmp.block.nVersion       = pblock->nVersion;
	tmp.block.hashPrevBlock  = pblock->hashPrevBlock  = (pindexPrev ? pindexPrev->GetBlockHash() : 0);
	tmp.block.hashMerkleRoot = pblock->hashMerkleRoot = pblock->BuildMerkleTree();
	tmp.block.nTime          = pblock->nTime          = max((pindexPrev ? pindexPrev->GetMedianTimePast()+1 : 0), GetAdjustedTime());
	tmp.block.nBits          = pblock->nBits          = nBits;
	tmp.block.nNonce         = pblock->nNonce         = 0;

	unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
	unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

	// Byte swap all the input buffer
	for (int i = 0; i < sizeof(tmp)/4; i++)
		((unsigned int*)&tmp)[i] = CryptoPP::ByteReverse(((unsigned int*)&tmp)[i]);

	// Precalc the first half of the first hash, which stays constant
	uint256 midstatebuf[2];
	uint256& midstate = *alignup<16>(midstatebuf);
	SHA256Transform(&midstate, &tmp.block, SHA256InitState);

	uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();

	// create and send the message to the client
	std::string blockstr("");
	std::string midstatestr("");
	std::string targetstr(hashTarget.GetHex());

	std::vector<unsigned char> blockbuff(64,0);
	::memcpy(&blockbuff[0],((char *)&tmp.block)+64,64);
	std::vector<unsigned char> midbuff(32,0);
	::memcpy(&midbuff[0],(char *)&midstate,32);

	EncodeBase64(blockbuff,blockstr);
	EncodeBase64(midbuff,midstatestr);

	json_spirit::Object obj;
	obj.push_back(json_spirit::Pair("blockid",client->NextBlockID()));
	obj.push_back(json_spirit::Pair("type",RemoteMinerMessage::MESSAGE_TYPE_SERVERSENDWORK));
	obj.push_back(json_spirit::Pair("block",blockstr));
	obj.push_back(json_spirit::Pair("midstate",midstatestr));
	obj.push_back(json_spirit::Pair("target",targetstr));

	// send complete block with transactions so client can verify
	json_spirit::Object fullblock;
	BlockToJson(pblock,fullblock);
	obj.push_back(json_spirit::Pair("fullblock",fullblock));

	client->SendMessage(RemoteMinerMessage(obj));

	// save this block with the client connection so we can verify the metahashes generated by the client
	RemoteClientConnection::sentwork sw;
	sw.m_blockid=client->NextBlockID();
	sw.m_key=key;
	sw.m_block=blockbuff;
	sw.m_midstate=midbuff;
	sw.m_target=hashTarget;
	sw.m_senttime=time(0);
	sw.m_pblock=pblock;
	sw.m_indexprev=pindexPrev;
	client->GetSentWork().push_back(sw);

	// clear out old work sent to client (sent 15 minutes or older)
	client->ClearOldSentWork(900);

	client->NextBlockID()++;
}

void BitcoinMinerRemoteServer::SendWorkToAllClients()
{
	SCOPEDTIME("BitcoinMinerRemoteServer::SendWorkToAllClients");
	for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		SendWork((*i));
	}
}

const bool BitcoinMinerRemoteServer::Step()
{
	SCOPEDTIME("BitcoinMinerRemoteServer::Step");
	int rval;
	fd_set readfs;
	fd_set writefs;
	struct timeval tv;
	std::vector<SOCKET>::iterator listeni;
	SOCKET highsocket;

	// reset values
	highsocket=0;
	tv.tv_sec=0;
	tv.tv_usec=0;

	// clear fd set
	FD_ZERO(&readfs);

	// put all listen sockets on the fd set
	for(listeni=m_listensockets.begin(); listeni!=m_listensockets.end(); listeni++)
	{
		FD_SET((*listeni),&readfs);
		if((*listeni)>highsocket)
		{
			highsocket=(*listeni);
		}
	}

	// see if any connections are waiting
	rval=select(highsocket+1,&readfs,0,0,&tv);

	// check for new connections
	if(rval>0)
	{
		for(listeni=m_listensockets.begin(); listeni!=m_listensockets.end(); listeni++)
		{
			if(FD_ISSET((*listeni),&readfs))
			{
				SOCKET newsock;
				struct sockaddr_storage addr;
				socklen_t addrlen=sizeof(addr);
				newsock=accept((*listeni),(struct sockaddr *)&addr,&addrlen);
				if(newsock!=INVALID_SOCKET)
				{
					RemoteClientConnection *newclient=new RemoteClientConnection(newsock,addr,addrlen);
					if(m_banned.find(newclient->GetAddress(false))!=m_banned.end())
					{
						printf("Banned client %s connected.  Disconnecting.\n",newclient->GetAddress().c_str());
						newclient->Disconnect();
						delete newclient;
					}
					else
					{
						m_clients.push_back(newclient);
						printf("Remote client %s connected\n",newclient->GetAddress().c_str());
					}
				}
			}
		}
	}

	// send and receive on existing connections
	highsocket=0;
	FD_ZERO(&readfs);
	FD_ZERO(&writefs);
	for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); i++)
	{
		if((*i)->IsConnected())
		{
			FD_SET((*i)->GetSocket(),&readfs);
			if((*i)->GetSocket()>highsocket)
			{
				highsocket=(*i)->GetSocket();
			}

			if((*i)->SendBufferSize()>0)
			{
				FD_SET((*i)->GetSocket(),&writefs);
			}
		}
	}

	tv.tv_usec=100;
	rval=select(highsocket+1,&readfs,&writefs,0,&tv);

	if(rval>0)
	{
		for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); i++)
		{
			if((*i)->IsConnected() && FD_ISSET((*i)->GetSocket(),&readfs))
			{
				(*i)->SocketReceive();
			}
			if((*i)->IsConnected() && FD_ISSET((*i)->GetSocket(),&writefs))
			{
				(*i)->SocketSend();
			}
		}
	}

	// remove any disconnected clients, or clients with too much data in the receive buffer
	for(std::vector<RemoteClientConnection *>::iterator i=m_clients.begin(); i!=m_clients.end(); )
	{
		if((*i)->IsConnected()==false || (*i)->ReceiveBufferSize()>(1024*1024))
		{
			printf("Remote client %s disconnected\n",(*i)->GetAddress().c_str());
			delete (*i);
			i=m_clients.erase(i);
		}
		else
		{
			i++;
		}
	}

	return true;

}

const bool VerifyBestHash(const RemoteClientConnection::sentwork &work, const uint256 &besthash, const unsigned int besthashnonce)
{
	SCOPEDTIME("VerifyBestHash");
	const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	
	uint256 tempbuff[4];
	uint256 &temphash=*alignup<16>(tempbuff);
	uint256 hashbuff[4];
	uint256 &hash=*alignup<16>(hashbuff);
	unsigned char midbuff[256]={0};
	unsigned char blockbuff[256]={0};
	unsigned char *midbuffptr=alignup<16>(midbuff);
	unsigned char *blockbuffptr=alignup<16>(blockbuff);
	unsigned int *nonce=(unsigned int *)(blockbuffptr+12);

	for(int i=0; i<4; i++)
	{
		tempbuff[i]=0;
		hashbuff[i]=0;
	}
	temphash=0;
	hash=0;

	FormatHashBlocks(&temphash,sizeof(temphash));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)&temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)&temphash)[i]);
	}
	
	::memcpy(blockbuffptr,&(work.m_block[0]),work.m_block.size());
	::memcpy(midbuffptr,&(work.m_midstate[0]),work.m_midstate.size());

	(*nonce)=besthashnonce;

	SHA256Transform(&temphash,blockbuffptr,midbuffptr);
	SHA256Transform(&hash,&temphash,SHA256InitState);

	for (int i = 0; i < sizeof(hash)/4; i++)
	{
		((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);
	}

	return (hash==besthash);
}

const bool VerifyFoundHash(RemoteClientConnection *client, const int64 blockid, const std::vector<unsigned char> &block, const unsigned int foundnonce, bool &accepted)
{
	SCOPEDTIME("VerifyFoundHash");
	const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	
	uint256 tempbuff[4];
	uint256 &temphash=*alignup<16>(tempbuff);
	uint256 hashbuff[4];
	uint256 &hash=*alignup<16>(hashbuff);
	unsigned char midbuff[256]={0};
	unsigned char blockbuff[256]={0};
	unsigned char *midbuffptr=alignup<16>(midbuff);
	unsigned char *blockbuffptr=alignup<16>(blockbuff);
	unsigned int *nonce=(unsigned int *)(blockbuffptr+12);
	bool foundwork=false;

	accepted=false;

	for(int i=0; i<4; i++)
	{
		tempbuff[i]=0;
		hashbuff[i]=0;
	}
	temphash=0;
	hash=0;

	FormatHashBlocks(&temphash,sizeof(temphash));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)&temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)&temphash)[i]);
	}
	
	RemoteClientConnection::sentwork *work;

	// use GetSentWorkByID when blockid is not 0
	if(blockid!=0)
	{
		foundwork=client->GetSentWorkByID(blockid,&work);
	}
	else
	{
		foundwork=client->GetSentWorkByBlock(block,&work);
	}

	if(foundwork==true)
	{
		
		if(work->m_pblock)
		{
			::memset(blockbuffptr,0,64);
			::memset(midbuffptr,0,32);
			::memcpy(blockbuffptr,&work->m_block[0],work->m_block.size());
			::memcpy(midbuffptr,&work->m_midstate[0],work->m_midstate.size());

			(*nonce)=foundnonce;
			
			SHA256Transform(&temphash,blockbuffptr,midbuffptr);
			SHA256Transform(&hash,&temphash,SHA256InitState);

			for (int i = 0; i < sizeof(hash)/4; i++)
			{
				((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);
			}

			work->m_pblock->nNonce=CryptoPP::ByteReverse(foundnonce);

			if(hash==work->m_pblock->GetHash() && hash<=work->m_target)
			{
                CRITICAL_BLOCK(cs_main)
                {
                    if (work->m_indexprev == pindexBest)
                    {
						// save the key
						AddKey(work->m_key);

                        // Track how many getdata requests this block gets
                        CRITICAL_BLOCK(cs_mapRequestCount)
                            mapRequestCount[work->m_pblock->GetHash()] = 0;

                        // Process this block the same as if we had received it from another node
                        if (!ProcessBlock(NULL, work->m_pblock))
						{
                            printf("ERROR in VerifyFoundHash, ProcessBlock, block not accepted\n");
						}
						else	// block accepted
						{
							accepted=true;
						}
                    	
						// pblock is deleted by ProcessBlock, so we need to zero the pointer in work so it won't be deleted twice
						work->m_pblock=0;

                    }
                }
				
				return true;
			}
			else
			{
				printf("VerifyFoundHash, client %s sent data that doesn't hash as expected\n",client->GetAddress().c_str());
			}

		}
	}
	return false;
}

void ThreadBitcoinMinerRemote(void* parg)
{
    try
    {
        vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX]++;
        BitcoinMinerRemote();
        vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX]--;
        PrintException(&e, "ThreadBitcoinMinerRemote()");
    } catch (...) {
        vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX]--;
        PrintException(NULL, "ThreadBitcoinMinerRemote()");
    }
    UIThreadCall(bind(CalledSetStatusBar, "", 0));
    nHPSTimerStart = 0;
    if (vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX] == 0)
        dHashesPerSec = 0;
    printf("ThreadBitcoinMinerRemote exiting, %d threads remaining\n", vnThreadsRunning[BITCOINMINERREMOTE_THREADINDEX]);
}

void BitcoinMinerRemote()
{
	SCOPEDTIME("BitcoinMinerRemote");

	std::string bindaddr("127.0.0.1");
	std::string bindport("8335");
	std::string remotepassword("");
	BitcoinMinerRemoteServer serv;
	time_t laststatusbarupdate=time(0);
	time_t lastverified=time(0);
	time_t lastserverstatus=time(0);
	bool blockaccepted=false;
	MetaHashVerifier metahashverifier;

	if(mapArgs.count("-remotebindaddr"))
	{
		bindaddr=mapArgs["-remotebindaddr"];
	}
	if(mapArgs.count("-remotebindport"))
	{
		bindport=mapArgs["-remotebindport"];
	}
	if(mapArgs.count("-remotepassword"))
	{
		remotepassword=mapArgs["-remotepassword"];
	}

	serv.StartListen(bindaddr,bindport);

	SetThreadPriority(THREAD_PRIORITY_LOWEST);

	while(fGenerateBitcoins)
	{
		serv.Step();

		// handle messages
		{
			SCOPEDTIME("BitcoinMinerRemote Handling Messages");
			for(std::vector<RemoteClientConnection *>::iterator i=serv.Clients().begin(); i!=serv.Clients().end(); i++)
			{
				while((*i)->MessageReady() && !(*i)->ProtocolError())
				{
					RemoteMinerMessage message;
					int type=RemoteMinerMessage::MESSAGE_TYPE_NONE;
					if((*i)->ReceiveMessage(message) && message.GetValue().type()==json_spirit::obj_type)
					{
						json_spirit::Value val=json_spirit::find_value(message.GetValue().get_obj(),"type");
						if(val.type()==json_spirit::int_type)
						{
							type=val.get_int();
							if((*i)->GotClientHello()==false && type!=RemoteMinerMessage::MESSAGE_TYPE_CLIENTHELLO)
							{
								printf("Client sent first message other than clienthello\n");
								(*i)->Disconnect();
							}
							else if((*i)->GotClientHello()==false && type==RemoteMinerMessage::MESSAGE_TYPE_CLIENTHELLO)
							{

								json_spirit::Value pval=json_spirit::find_value(message.GetValue().get_obj(),"address");
								if(pval.type()==json_spirit::str_type)
								{
									uint160 address;
									address.SetHex(pval.get_str());
									(*i)->SetRequestedRecipientAddress(address);
								}

								pval=json_spirit::find_value(message.GetValue().get_obj(),"password");
								if(pval.type()==json_spirit::str_type && pval.get_str()==remotepassword)
								{
									printf("Got clienthello from client %s\n",(*i)->GetAddress().c_str());
									(*i)->SetGotClientHello(true);
									serv.SendServerHello((*i),BITCOINMINERREMOTE_HASHESPERMETA);
									// also send work right away
									serv.SendWork((*i));
								}
								else
								{
									printf("Client %s didn't send correct password.  Disconnecting.\n",(*i)->GetAddress().c_str());
									(*i)->Disconnect();
								}

							}
							else if(type==RemoteMinerMessage::MESSAGE_TYPE_CLIENTGETWORK)
							{
								// only send new work if it has been at least 5 seconds since the last work
								if((*i)->GetSentWork().size()==0 || difftime(time(0),(*i)->GetSentWork()[(*i)->GetSentWork().size()-1].m_senttime)>=5)
								{
									serv.SendWork((*i));
								}
							}
							else if(type==RemoteMinerMessage::MESSAGE_TYPE_CLIENTMETAHASH)
							{
								int64 blockid=0;
								std::vector<unsigned char> block;
								std::vector<unsigned char> digest;
								unsigned int nonce=0;
								uint256 besthash=~0;
								unsigned int besthashnonce=0;
								bool foundwork=false;

								json_spirit::Value val=json_spirit::find_value(message.GetValue().get_obj(),"blockid");
								if(val.type()==json_spirit::int_type)
								{
									blockid=val.get_int();
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"block");
								if(val.type()==json_spirit::str_type)
								{
									BitcoinMinerRemoteServer::DecodeBase64(val.get_str(),block);
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"digest");
								if(val.type()==json_spirit::str_type)
								{
									BitcoinMinerRemoteServer::DecodeBase64(val.get_str(),digest);
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"nonce");
								if(val.type()==json_spirit::int_type)
								{
									nonce=val.get_int64();
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"besthash");
								if(val.type()==json_spirit::str_type)
								{
									besthash.SetHex(val.get_str());
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"besthashnonce");
								if(val.type()==json_spirit::int_type)
								{
									besthashnonce=val.get_int64();
								}
								RemoteClientConnection::sentwork *work;

								// use GetSentWorkByID when blockid is not 0
								if(blockid!=0)
								{
									foundwork=(*i)->GetSentWorkByID(blockid,&work);
								}
								else
								{
									foundwork=(*i)->GetSentWorkByBlock(block,&work);
								}

								if(foundwork==true)
								{
									if(work->CheckNonceOverlap(nonce,BITCOINMINERREMOTE_HASHESPERMETA)==false && besthashnonce>=nonce && besthashnonce<nonce+BITCOINMINERREMOTE_HASHESPERMETA)
									{
										if(VerifyBestHash(*work,besthash,besthashnonce)==true)
										{
											RemoteClientConnection::metahash mh;
											//mh.m_metahash=digest;
											mh.m_metahash.swap(digest);
											mh.m_senttime=time(0);
											mh.m_startnonce=nonce;
											mh.m_verified=false;
											mh.m_besthash=besthash;
											mh.m_besthashnonce=besthashnonce;
											work->m_metahashes.push_back(mh);

											// only accumulate hashes if client specified address to send to and we have successfully verified at least 1 metahash
											// this will prevent a client from connecting and disconnecting rapidly to increase their hash count
											if((*i)->GetRecipientAddress()!=0 && (*i)->GetVerifiedMetaHashCount()>0)
											{
												serv.AddContributedHashes((*i)->GetRecipientAddress(),BITCOINMINERREMOTE_HASHESPERMETA);
											}
										}
										else
										{
											printf("Couldn't verify best hash from client %s\n",(*i)->GetAddress().c_str());
										}
									}
									else
									{
										printf("Detected nonce overlap from client %s\n",(*i)->GetAddress().c_str());
									}
								}
								else
								{
									printf("Client %s sent metahash for block we don't know about!\n",(*i)->GetAddress().c_str());
								}
							}
							else if(type==RemoteMinerMessage::MESSAGE_TYPE_CLIENTFOUNDHASH)
							{
								SetThreadPriority(THREAD_PRIORITY_NORMAL);

								int64 blockid=0;
								std::vector<unsigned char> block;
								int64 nonce=0;
								bool foundwork=false;

								json_spirit::Value val=json_spirit::find_value(message.GetValue().get_obj(),"blockid");
								if(val.type()==json_spirit::int_type)
								{
									blockid=val.get_int();
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"block");
								if(val.type()==json_spirit::str_type)
								{
									BitcoinMinerRemoteServer::DecodeBase64(val.get_str(),block);
								}
								val=json_spirit::find_value(message.GetValue().get_obj(),"nonce");
								if(val.type()==json_spirit::int_type)
								{
									nonce=val.get_int();
								}
								
								if(VerifyFoundHash((*i),blockid,block,nonce,blockaccepted)==true)
								{
									if(blockaccepted==true)
									{
										serv.GeneratedCount()++;
										serv.ClearCurrentHashesContributed();
									}
									// send ALL clients new block to work on
									serv.SendWorkToAllClients();
								}
								SetThreadPriority(THREAD_PRIORITY_LOWEST);
							}
							else
							{
								printf("Unhandled message type (%d) from client %s\n",type,(*i)->GetAddress().c_str());
							}
						}
						else
						{
							printf("Unexpected json type detected when finding message type.  Disconnecting client %s.\n",(*i)->GetAddress().c_str());
							(*i)->Disconnect();
						}
					}
					else
					{
						printf("There was an error receiving a message from a client.  Disconnecting the client %s.\n",(*i)->GetAddress().c_str());
						(*i)->Disconnect();
					}

				}

				if((*i)->ProtocolError())
				{
					printf("There was protcol error from client %s.  Disconnecting.\n",(*i)->GetAddress().c_str());
					(*i)->Disconnect();
				}

				// send new block every 2 minutes
				if((*i)->GetSentWork().size()>0)
				{
					if(difftime(time(0),(*i)->GetSentWork()[(*i)->GetSentWork().size()-1].m_senttime)>=120)
					{
						serv.SendWork((*i));
					}
				}
			}

		}

		if(serv.Clients().size()==0)
		{
			Sleep(100);
		}

		if(difftime(time(0),laststatusbarupdate)>=10)
		{
			int64 clients=serv.Clients().size();
			int64 khashmeta=serv.GetAllClientsCalculatedKHashFromMeta();
			int64 khashbest=serv.GetAllClientsCalculatedKHashFromBest();
			//std::string strStatus = strprintf(" %"PRI64d" clients    %"PRI64d" khash/s meta     %"PRI64d" khash/s best",clients,khashmeta,khashbest);
			std::string strStatus = strprintf(" %"PRI64d" clients    %"PRI64d" khash/s meta",clients,khashmeta);
			UIThreadCall(bind(CalledSetStatusBar, strStatus, 0));
			laststatusbarupdate=time(0);
		}

		// check metahash of a client every 10 seconds
		if(serv.Clients().size()>0 && difftime(time(0),lastverified)>=10)
		{
			RemoteClientConnection *client=serv.GetOldestNonVerifiedMetaHashClient();

			if(metahashverifier.GetClient()!=client || (client!=0 && metahashverifier.GetClient()==0))
			{
				RemoteClientConnection::sentwork work;
				if(client->GetNewestSentWorkWithMetaHash(work))
				{
					metahashverifier.Start(client,work);
				}
			}

			if(client!=0 && metahashverifier.GetClient()==client && metahashverifier.Done()==false)
			{
				metahashverifier.Step();
			}

			if(client!=0 && metahashverifier.GetClient()==client && metahashverifier.Done()==true)
			{
				if(metahashverifier.Verified()==true)
				{
					client->SetWorkVerified(metahashverifier.GetWorkID(),metahashverifier.GetMetaHashIndex(),true);
					printf("Client %s passed metahash verification\n",client->GetAddress().c_str());
				}
				else
				{
					client->SetWorkVerified(metahashverifier.GetWorkID(),metahashverifier.GetMetaHashIndex(),false);
					printf("Client %s failed metahash verification\n",client->GetAddress().c_str());
				}

				metahashverifier.ClearClient();
				lastverified=time(0);

				client->SetLastVerifiedMetaHash(lastverified);
				
			}

		}
		
		// send server status to all connected clients every minute
		// also save contributed hashes in case of a server crash
		if(difftime(time(0),lastserverstatus)>=60)
		{
			serv.SendServerStatus();
			lastserverstatus=time(0);
			serv.SaveContributedHashes();
		}

	}	// while fGenerateBitcoins

}
