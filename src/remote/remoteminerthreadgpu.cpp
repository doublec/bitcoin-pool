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

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)

#include "remoteminerthreadgpu.h"

RemoteMinerThreadGPU::RemoteMinerThreadGPU()
{
}

RemoteMinerThreadGPU::~RemoteMinerThreadGPU()
{
}

void RemoteMinerThreadGPU::Run(void *arg)
{
	threaddata *td=(threaddata *)arg;
	int64 currentblockid=-1;

	static const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	uint256 tempbuff[4];
	uint256 &temphash=*alignup<16>(tempbuff);
	uint256 hashbuff[4];
	uint256 &hash=*alignup<16>(hashbuff);
	uint256 currenttarget;
	gpurunnertype gpu;

	unsigned char currentmidbuff[256];
	unsigned char currentblockbuff[256];
	unsigned char *midbuffptr;
	unsigned char *blockbuffptr;
	unsigned int *nonce;
	midbuffptr=alignup<16>(currentmidbuff);
	blockbuffptr=alignup<16>(currentblockbuff);
	nonce=(unsigned int *)(blockbuffptr+12);

	uint256 besthash=~(uint256(0));
	unsigned int besthashnonce=0;

	unsigned char *metahash=0;
	unsigned int metahashsize=td->m_metahashsize;
	unsigned int metahashpos=0;
	unsigned int metahashstartnonce=0;

	SetThreadPriority(THREAD_PRIORITY_LOWEST);

	FormatHashBlocks(&temphash,sizeof(temphash));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)&temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)&temphash)[i]);
	}

	gpu.FindBestConfiguration();

	while(td->m_generate)
	{
		if(td->m_havework)
		{
			if(metahashstartnonce==0)
			{
				CRITICAL_BLOCK(td->m_cs);
				if(currentblockid!=td->m_nextblock.m_blockid)
				{
					currenttarget=td->m_nextblock.m_target;
					currentblockid=td->m_nextblock.m_blockid;
					::memcpy(midbuffptr,&td->m_nextblock.m_midstate[0],32);
					::memcpy(blockbuffptr,&td->m_nextblock.m_block[0],64);
					metahashpos=0;
					metahashstartnonce=0;
					(*nonce)=0;
					besthash=~(uint256(0));
					besthashnonce=0;
					if(metahash==0 || td->m_metahashsize!=metahashsize)
					{
						metahashsize=td->m_metahashsize;
						if(metahash)
						{
							delete [] metahash;
						}
						metahash=new unsigned char[metahashsize];
					}

					for(int i=0; i<8; i++)
					{
						gpu.GetIn()->m_AH[i]=((unsigned int *)midbuffptr)[i];
					}
					gpu.GetIn()->m_merkle=((unsigned int *)blockbuffptr)[0];
					gpu.GetIn()->m_ntime=((unsigned int *)blockbuffptr)[1];
					gpu.GetIn()->m_nbits=((unsigned int *)blockbuffptr)[2];
				}
			}

			gpu.GetIn()->m_nonce=(*nonce);

			gpu.RunStep();

			for(int i=0; i<gpu.GetNumThreads()*gpu.GetNumBlocks(); i++)
			{
				memcpy(&hash,&(gpu.GetOut()[i].m_bestAH[0]),32);

				if(gpu.GetOut()[i].m_bestnonce!=0 && hash!=0 && hash<=currenttarget)
				{
					CRITICAL_BLOCK(td->m_cs);
					td->m_foundhashes.push_back(foundhash(currentblockid,gpu.GetOut()[i].m_bestnonce));
				}

				if(gpu.GetOut()[i].m_bestnonce!=0 && hash!=0 && hash<besthash && gpu.GetOut()[i].m_bestnonce<metahashstartnonce+metahashsize)
				{
					besthash=hash;
					besthashnonce=gpu.GetOut()[i].m_bestnonce;
				}

				for(int j=0; j<gpu.GetStepIterations(); j++)
				{
					(*nonce)++;
					metahash[metahashpos++]=gpu.GetMetaHash()[(i*gpu.GetStepIterations())+j];

					if(metahashpos>=metahashsize)
					{

						{
							CRITICAL_BLOCK(td->m_cs);
							td->m_hashresults.push_back(hashresult(currentblockid,besthash,besthashnonce,metahash,metahashstartnonce));

							if(td->m_metahashptrs.size()>0)
							{
								metahash=td->m_metahashptrs[td->m_metahashptrs.size()-1];
								td->m_metahashptrs.erase(td->m_metahashptrs.end()-1);
							}
							else
							{
								metahash=new unsigned char[metahashsize];
							}
						}

						metahashpos=0;
						metahashstartnonce=(*nonce);
						besthash=~(uint256(0));
						besthashnonce=0;

						{
							CRITICAL_BLOCK(td->m_cs);
							if(currentblockid!=td->m_nextblock.m_blockid)
							{
								currenttarget=td->m_nextblock.m_target;
								currentblockid=td->m_nextblock.m_blockid;
								::memcpy(midbuffptr,&td->m_nextblock.m_midstate[0],32);
								::memcpy(blockbuffptr,&td->m_nextblock.m_block[0],64);
								metahashpos=0;
								metahashstartnonce=0;
								(*nonce)=0;
								for(int i=0; i<8; i++)
								{
									gpu.GetIn()->m_AH[i]=((unsigned int *)midbuffptr)[i];
								}
								gpu.GetIn()->m_merkle=((unsigned int *)blockbuffptr)[0];
								gpu.GetIn()->m_ntime=((unsigned int *)blockbuffptr)[1];
								gpu.GetIn()->m_nbits=((unsigned int *)blockbuffptr)[2];
							}
						}

					}
				}	// j

			}

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

#endif	// defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)
