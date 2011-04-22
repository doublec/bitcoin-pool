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

#ifndef _rpcthread_gpu_
#define _rpcthread_gpu_

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)

#include "rpcminerthread.h"
#include "../cuda/bitcoinminercuda.h"
#include "../opencl/bitcoinmineropencl.h"

class RPCMinerThreadGPU:public RPCMinerThread
{
public:
	RPCMinerThreadGPU();
	~RPCMinerThreadGPU();

	virtual const bool Start()
	{
		m_threaddata.m_done=false;
		m_threaddata.m_havework=false;
		m_threaddata.m_generate=true;
		m_threaddata.m_nextblock.m_blockid=0;
		m_threaddata.m_hashcount=0;
		if(!CreateThread(RPCMinerThreadGPU::Run,&m_threaddata))
		{
			m_threaddata.m_done=true;
			return false;
		}
		return true;
	}

private:
	static void Run(void *arg);

#ifdef _BITCOIN_MINER_CUDA_
	typedef CUDARunner gpurunnertype;
#elif defined(_BITCOIN_MINER_OPENCL_)
	typedef OpenCLRunner gpurunnertype;
#endif
};

#endif	//  defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)

#endif	// _rpcthread_gpu_
