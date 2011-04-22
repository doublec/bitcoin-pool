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

#include "rpcminerthreadcpu.h"

extern void DoubleBlockSHA256(const void* pin, void* pout, const void* pinit, unsigned int hash[8][32], const void* init2);

RPCMinerThreadCPU::RPCMinerThreadCPU()
{
}

RPCMinerThreadCPU::~RPCMinerThreadCPU()
{
}

void RPCMinerThreadCPU::Run(void *arg)
{
	threaddata *td=(threaddata *)arg;
	int64 currentblockid=-1;

	static const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	uint256 tempbuff[4];
	uint256 &temphash=*alignup<16>(tempbuff);
	uint256 hashbuff[4];
	uint256 &hash=*alignup<16>(hashbuff);
	uint256 currenttarget;

	unsigned char currentmidbuff[256];
	unsigned char currentblockbuff[256];
	volatile unsigned char *midbuffptr;
	volatile unsigned char *blockbuffptr;
	volatile unsigned int *nonce;
	midbuffptr=alignup<16>(currentmidbuff);
	blockbuffptr=alignup<16>(currentblockbuff);
	nonce=(unsigned int *)(blockbuffptr+12);

#ifdef FOURWAYSSE2
	unsigned int fourwayhashbuf[9][32];
	unsigned int (&fourwayhash)[9][32]=*alignup<16>(&fourwayhashbuf);
#endif

	SetThreadPriority(THREAD_PRIORITY_LOWEST);

	FormatHashBlocks(&temphash,sizeof(temphash));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)&temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)&temphash)[i]);
	}

	(*nonce)=0;

	while(td->m_generate)
	{
		if(td->m_havework)
		{
			CRITICAL_BLOCK(td->m_cs);
			if(currentblockid!=td->m_nextblock.m_blockid)
			{
				currenttarget=td->m_nextblock.m_target;
				currentblockid=td->m_nextblock.m_blockid;
				::memcpy((void *)midbuffptr,&td->m_nextblock.m_midstate[0],32);
				::memcpy((void *)blockbuffptr,&td->m_nextblock.m_block[0],64);
				::memcpy(&temphash,&td->m_nextblock.m_hash1[0],64);
				(*nonce)=0;
			}

#ifdef FOURWAYSSE2

			for(int iter=0; iter<100; iter++)
			{
				DoubleBlockSHA256((void *)blockbuffptr,&temphash,(void *)midbuffptr,fourwayhash,SHA256InitState);

				for(int i=0; i<32; i++)
				{
					if(fourwayhash[7][i]==0)
					{
						for (int j = 0; j < sizeof(hash)/4; j++)
						{
							((unsigned int*)&hash)[j] = CryptoPP::ByteReverse(fourwayhash[j][i]);
						}
						
						if(hash<=currenttarget)
						{
							CRITICAL_BLOCK(td->m_cs);
							td->m_foundhashes.push_back(foundhash(currentblockid,(*nonce)+i));
						}
					}
				}

				(*nonce)+=32;
			}

			{
				CRITICAL_BLOCK(td->m_cs);
				td->m_hashcount+=32*100;
			}

#else	// FOURWAYSSE2

			// do 10000 hashes at a time
			for(unsigned int i=0; i<10000; i++)
			{
				SHA256Transform(&temphash,(void *)blockbuffptr,(void *)midbuffptr);
				SHA256Transform(&hash,&temphash,SHA256InitState);

				if((((unsigned int*)&hash)[7]==0))
				{
					for (int i = 0; i < sizeof(hash)/4; i++)
					{
						((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);
					}

					if(hash<=currenttarget)
					{
						CRITICAL_BLOCK(td->m_cs);

						td->m_foundhashes.push_back(foundhash(currentblockid,(*nonce)));
					}
				}

				(*nonce)++;
			}

			{
				CRITICAL_BLOCK(td->m_cs);
				td->m_hashcount+=10000;
			}

#endif	// FOURWAYSSE2

		}
		else
		{
			Sleep(100);
		}
	}

	{
		CRITICAL_BLOCK(td->m_cs);
		td->m_done=true;
	}
}
