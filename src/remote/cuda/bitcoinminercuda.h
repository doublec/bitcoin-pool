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

#ifndef _bitcoin_remote_miner_cuda_
#define _bitcoin_remote_miner_cuda_

#ifdef _BITCOIN_MINER_CUDA_

#include "../../gpucommon/gpurunner.h"
#include "cudashared.h"

class RemoteCUDARunner:public GPURunner<unsigned long,int>
{
public:
	RemoteCUDARunner();
	~RemoteCUDARunner();

	void FindBestConfiguration();

	const unsigned long RunStep();

	void SetMetaHashSize(const long size)	{ m_metahashsize=size; }
	remote_cuda_in *GetIn()					{ return m_in; }
	remote_cuda_out *GetOut()				{ return m_out; }
	unsigned char *GetMetaHash()			{ return m_metahash; }

private:
	void DeallocateResources();
	void AllocateResources(const int numb, const int numt);

	long m_metahashsize;
	remote_cuda_in *m_in;
	remote_cuda_in *m_devin;
	remote_cuda_out *m_out;
	remote_cuda_out *m_devout;
	unsigned char *m_metahash;
	unsigned char *m_devmetahash;

};

#endif	// _BITCOIN_MINER_CUDA_

#endif	// _bitcoin_remote_miner_cuda_
