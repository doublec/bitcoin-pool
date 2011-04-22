#ifndef _rpcminer_thread_
#define _rpcminer_thread_

#define NOMINMAX

#include "../minercommon/minerheaders.h"
#include "../cryptopp/sha.h"
#include <limits>

class RPCMinerThread
{
public:
	RPCMinerThread()
	{
		m_threaddata.m_done=true;
		m_threaddata.m_havework=false;
	}
	virtual ~RPCMinerThread()
	{
		while(Done()==false)
		{
			Stop();
		}
	}

	struct foundhash
	{
		foundhash():m_blockid(0),m_nonce(0)												{ }
		foundhash(int64 blockid, unsigned int nonce):m_blockid(blockid),m_nonce(nonce)	{ }

		int64 m_blockid;
		unsigned int m_nonce;
	};

	virtual const bool Start()
	{
		m_threaddata.m_done=false;
		m_threaddata.m_havework=false;
		m_threaddata.m_generate=true;
		m_threaddata.m_nextblock.m_blockid=0;
		m_threaddata.m_hashcount=0;
		if(!CreateThread(RPCMinerThread::Run,&m_threaddata))
		{
			m_threaddata.m_done=true;
			return false;
		}
		return true;
	}

	void Stop()		{ CRITICAL_BLOCK(m_threaddata.m_cs); m_threaddata.m_generate=false; }

	const bool Done()
	{
		return m_threaddata.m_done;
	}

	const bool HaveFoundHash()
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		return (m_threaddata.m_foundhashes.size()>0);
	}

	const bool GetFoundHash(foundhash &hash)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		if(m_threaddata.m_foundhashes.size()>0)
		{
			hash=m_threaddata.m_foundhashes[0];
			m_threaddata.m_foundhashes.erase(m_threaddata.m_foundhashes.begin());
			return true;
		}
		return false;
	}

	const bool HasWork()
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		return m_threaddata.m_havework;
	}

	void SetNextBlock(const int64 blockid, uint256 target, std::vector<unsigned char> &block, std::vector<unsigned char> &midstate, std::vector<unsigned char> &hash1)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		m_threaddata.m_nextblock.m_blockid=blockid;
		m_threaddata.m_nextblock.m_target=target;
		m_threaddata.m_nextblock.m_block=block;
		m_threaddata.m_nextblock.m_midstate=midstate;
		m_threaddata.m_nextblock.m_hash1=hash1;
		m_threaddata.m_havework=true;
	}

	const int64 GetHashCount(const bool reset=false)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		if(reset==false)
		{
			return m_threaddata.m_hashcount;
		}
		else
		{
			int64 hcount=m_threaddata.m_hashcount;
			m_threaddata.m_hashcount=0;
			return hcount;
		}
	}

protected:
	static void Run(void *arg)	{ CRITICAL_BLOCK(((threaddata *)arg)->m_cs); ((threaddata *)arg)->m_done=true; }

	static inline void SHA256Transform(void* pstate, void* pinput, const void* pinit)
	{
		::memcpy(pstate, pinit, 32);
		CryptoPP::SHA256::Transform((CryptoPP::word32*)pstate, (CryptoPP::word32*)pinput);
	}

	static int FormatHashBlocks(void* pbuffer, unsigned int len)
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

	struct nextblock
	{
		int64 m_blockid;
		uint256 m_target;
		std::vector<unsigned char> m_midstate;
		std::vector<unsigned char> m_block;
		std::vector<unsigned char> m_hash1;
	};

	struct threaddata
	{
		CCriticalSection m_cs;
		bool m_generate;
		bool m_done;
		bool m_havework;
		nextblock m_nextblock;
		std::vector<foundhash> m_foundhashes;
		int64 m_hashcount;
	};

	threaddata m_threaddata;

};

class RPCMinerThreads
{
public:
	RPCMinerThreads()		{ }
	~RPCMinerThreads()
	{
		Stop();
		while(RunningThreadCount()>0)
		{
			Sleep(100);
		}
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			delete (*i);
		}
	}
	
	void Start(RPCMinerThread *thread)		{ m_minerthreads.push_back(thread); thread->Start(); }
	void Stop()
	{
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			(*i)->Stop();
		}
		while(RunningThreadCount()>0)
		{
			Sleep(100);
		}
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			delete (*i);
		}
		m_minerthreads.clear();
	}

	const int RunningThreadCount() const
	{
		int count=0;
		for(std::vector<RPCMinerThread *>::const_iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->Done()==false)
			{
				count++;
			}
		}
		return count;
	}

	const bool NeedWork() const
	{
		for(std::vector<RPCMinerThread *>::const_iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->HasWork()==false)
			{
				return true;
			}
		}
		return false;
	}

	const bool HaveFoundHash()
	{
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->HaveFoundHash())
			{
				return true;
			}
		}
		return false;
	}

	const bool GetFoundHash(RPCMinerThread::foundhash &hash)
	{
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->GetFoundHash(hash))
			{
				return true;
			}
		}
		return false;
	}

	void SetNextBlock(const int64 blockid, uint256 target, std::vector<unsigned char> &block, std::vector<unsigned char> &midstate, std::vector<unsigned char> &hash1)
	{
		RPCMinerThread *earliest=0;
		int64 earliesttime=(std::numeric_limits<int64>::max)();
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if(m_lastwork.find((*i))==m_lastwork.end())
			{
				earliest=(*i);
				earliesttime=0;
			}
			else if(m_lastwork[(*i)]<earliesttime && (*i)->Done()==false)
			{
				earliest=(*i);
				earliesttime=m_lastwork[(*i)];
			}
		}
		if(earliest)
		{
			earliest->SetNextBlock(blockid,target,block,midstate,hash1);
			m_lastwork[earliest]=GetTimeMillis();
		}
	}

	const int64 GetHashCount(const bool reset=false)
	{
		int64 hashcount=0;
		for(std::vector<RPCMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			hashcount+=(*i)->GetHashCount(reset);
		}
		return hashcount;
	}

private:
	std::vector<RPCMinerThread *> m_minerthreads;
	std::map<RPCMinerThread *,int64> m_lastwork;
};

#endif	// _rpcminer_thread_
