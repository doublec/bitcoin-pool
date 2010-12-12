#ifndef _remoteminer_thread_
#define _remoteminer_thread_

#define NOMINMAX

#include "remotebitcoinheaders.h"
#include "../cryptopp/sha.h"
#include <limits>

class RemoteMinerThread
{
public:
	RemoteMinerThread()
	{
		m_threaddata.m_done=true;
		m_threaddata.m_havework=false;
	}
	virtual ~RemoteMinerThread()
	{
		while(Done()==false)
		{
			Stop();
		}

		FreeMetaHashPointers();

	}

	struct hashresult
	{
		hashresult():m_blockid(0),m_besthash(0),m_besthashnonce(0),m_metahashstartnonce(0),m_metahashptr(0)	{ }
		//hashresult(int64 blockid, uint256 besthash, unsigned int besthashnonce, std::vector<unsigned char> &metahashdigest, unsigned int metahashstartnonce):m_blockid(blockid),m_besthash(besthash),m_besthashnonce(besthashnonce),m_metahashdigest(metahashdigest),m_metahashstartnonce(metahashstartnonce)	{ }
		hashresult(int64 blockid, uint256 besthash, unsigned int besthashnonce, unsigned char *metahashptr, unsigned int metahashstartnonce):m_blockid(blockid),m_besthash(besthash),m_besthashnonce(besthashnonce),m_metahashptr(metahashptr),m_metahashstartnonce(metahashstartnonce) { }

		int64 m_blockid;
		uint256 m_besthash;
		unsigned int m_besthashnonce;
		std::vector<unsigned char> m_metahashdigest;
		unsigned int m_metahashstartnonce;

		unsigned char *m_metahashptr;
	};

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
		FreeMetaHashPointers();
		if(!CreateThread(RemoteMinerThread::Run,&m_threaddata))
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

	const bool HaveHashResult()
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		return (m_threaddata.m_hashresults.size()>0);
	}

	const bool GetHashResult(hashresult &result)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		if(m_threaddata.m_hashresults.size()>0)
		{
			result=m_threaddata.m_hashresults[0];
			m_threaddata.m_hashresults.erase(m_threaddata.m_hashresults.begin());
			return true;
		}
		return false;
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

	void SetNextBlock(const int64 blockid, uint256 target, std::vector<unsigned char> &block, std::vector<unsigned char> &midstate)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		m_threaddata.m_nextblock.m_blockid=blockid;
		m_threaddata.m_nextblock.m_target=target;
		m_threaddata.m_nextblock.m_block=block;
		m_threaddata.m_nextblock.m_midstate=midstate;
		m_threaddata.m_havework=true;
	}

	void SetMetaHashSize(const unsigned int size)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		m_threaddata.m_metahashsize=size;
	}

	void AddMetaHashPointer(unsigned char *ptr)
	{
		CRITICAL_BLOCK(m_threaddata.m_cs);
		m_threaddata.m_metahashptrs.push_back(ptr);
	}

protected:
	static void Run(void *arg)	{ CRITICAL_BLOCK(((threaddata *)arg)->m_cs); ((threaddata *)arg)->m_done=true; }

	void FreeMetaHashPointers()
	{
		for(std::vector<unsigned char *>::iterator i=m_threaddata.m_metahashptrs.begin(); i!=m_threaddata.m_metahashptrs.end(); i++)
		{
			delete [] (*i);
		}
	}

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
	};

	struct threaddata
	{
		CCriticalSection m_cs;
		bool m_generate;
		bool m_done;
		bool m_havework;
		int m_metahashsize;
		nextblock m_nextblock;
		std::vector<hashresult> m_hashresults;
		std::vector<foundhash> m_foundhashes;
		std::vector<unsigned char *> m_metahashptrs;
	};

	threaddata m_threaddata;

};

class RemoteMinerThreads
{
public:
	RemoteMinerThreads():m_metahashsize(0)		{ }
	~RemoteMinerThreads()
	{
		Stop();
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			delete (*i);
		}
	}
	
	void Start(RemoteMinerThread *thread)		{ m_minerthreads.push_back(thread); thread->Start(); }
	void Stop()
	{
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			(*i)->Stop();
		}
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			delete (*i);
		}
		m_minerthreads.clear();
		m_metahashsize=0;
	}

	const int RunningThreadCount() const
	{
		int count=0;
		for(std::vector<RemoteMinerThread *>::const_iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
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
		for(std::vector<RemoteMinerThread *>::const_iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->HasWork()==false)
			{
				return true;
			}
		}
		return false;
	}

	const bool HaveHashResult()
	{
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->HaveHashResult()==true)
			{
				return true;
			}
		}
		return false;
	}

	const bool GetHashResult(RemoteMinerThread::hashresult &hashresult)
	{
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->GetHashResult(hashresult))
			{
				hashresult.m_metahashdigest.resize(SHA256_DIGEST_LENGTH,0);

				SHA256(hashresult.m_metahashptr,m_metahashsize,&hashresult.m_metahashdigest[0]);
				(*i)->AddMetaHashPointer(hashresult.m_metahashptr);
				return true;
			}
		}
		return false;
	}

	const bool HaveFoundHash()
	{
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->HaveFoundHash())
			{
				return true;
			}
		}
		return false;
	}

	const bool GetFoundHash(RemoteMinerThread::foundhash &hash)
	{
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			if((*i)->GetFoundHash(hash))
			{
				return true;
			}
		}
		return false;
	}

	void SetMetaHashSize(const unsigned int size)
	{
		m_metahashsize=size;
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
		{
			(*i)->SetMetaHashSize(size);
		}
	}

	void SetNextBlock(const int64 blockid, uint256 target, std::vector<unsigned char> &block, std::vector<unsigned char> &midstate)
	{
		RemoteMinerThread *earliest=0;
		int64 earliesttime=(std::numeric_limits<int64>::max)();
		for(std::vector<RemoteMinerThread *>::iterator i=m_minerthreads.begin(); i!=m_minerthreads.end(); i++)
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
			earliest->SetMetaHashSize(m_metahashsize);
			earliest->SetNextBlock(blockid,target,block,midstate);
			m_lastwork[earliest]=GetTimeMillis();
		}
	}

private:
	std::vector<RemoteMinerThread *> m_minerthreads;
	std::map<RemoteMinerThread *,int64> m_lastwork;
	int m_metahashsize;
};

#endif	// _remoteminer_thread_
