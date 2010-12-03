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

#ifndef _bitcoin_miner_cuda_
#define _bitcoin_miner_cuda_

#ifdef _BITCOIN_MINER_CUDA_

#include "../headers.h"
#include "../gpucommon/gpucommon.h"
#include "cudashared.h"

class CUDARunner:public GPURunner<unsigned long,int>
{
public:
	CUDARunner();
	~CUDARunner();

	void FindBestConfiguration();

	const unsigned long RunStep();

	cuda_in *GetIn()		{ return m_in; }

private:
	void DeallocateResources();
	void AllocateResources(const int numb, const int numt);

	cuda_in *m_in;
	cuda_in *m_devin;
	cuda_out *m_out;
	cuda_out *m_devout;

};

#endif	// _BITCOIN_MINER_CUDA_

#endif	// _bitcoin_miner_cuda_
