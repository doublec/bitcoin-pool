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

#ifdef _BITCOIN_MINER_CUDA_

#define NOMINMAX

#include "bitcoinminercuda.h"
#include "cudashared.h"
#include <cutil_inline.h>
#include <limits>

CUDARunner::CUDARunner():GPURunner<unsigned long,int>(TYPE_CUDA)
{
	m_in=0;
	m_devin=0;
	m_out=0;
	m_devout=0;

	cutilSafeCall(cudaGetDeviceCount(&m_devicecount));

	if(m_devicecount>0)
	{
		if(m_deviceindex<0 || m_deviceindex>=m_devicecount)
		{
			m_deviceindex=cutGetMaxGflopsDeviceId();
			printf("Setting CUDA device to Max GFlops device at index %u\n",m_deviceindex);
		}
		else
		{
			printf("Setting CUDA device to device at index %u\n",m_deviceindex);
		}
		
		cudaDeviceProp props;
		cudaGetDeviceProperties(&props,m_deviceindex);

		printf("Device info for %s :\nCompute Capability : %d.%d\nClock Rate (hz) : %d\n",props.name,props.major,props.minor,props.clockRate);

		if(props.major>999)
		{
			printf("CUDA seems to be running in CPU emulation mode\n");
		}

		cutilSafeCall(cudaSetDevice(m_deviceindex));

	}
	else
	{
		m_deviceindex=-1;
		printf("No CUDA capable device detected\n");
	}
}

CUDARunner::~CUDARunner()
{
	DeallocateResources();
	cutilSafeCall(cudaThreadExit());
}

void CUDARunner::AllocateResources(const int numb, const int numt)
{
	DeallocateResources();

	m_in=(cuda_in *)malloc(sizeof(cuda_in));
	m_out=(cuda_out *)malloc(numb*numt*sizeof(cuda_out));

	cutilSafeCall(cudaMalloc((void **)&m_devin,sizeof(cuda_in)));
	cutilSafeCall(cudaMalloc((void **)&m_devout,numb*numt*sizeof(cuda_out)));

	printf("Done allocating CUDA resources for (%d,%d)\n",numb,numt);
}

void CUDARunner::DeallocateResources()
{
	if(m_in)
	{
		free(m_in);
		m_in=0;
	}
	if(m_devin)
	{
		cutilSafeCall(cudaFree(m_devin));
		m_devin=0;
	}
	if(m_out)
	{
		free(m_out);
		m_out=0;
	}
	if(m_devout)
	{
		cutilSafeCall(cudaFree(m_devout));
		m_devout=0;
	}
}

void CUDARunner::FindBestConfiguration()
{
	unsigned long lowb=16;
	unsigned long highb=128;
	unsigned long lowt=16;
	unsigned long hight=256;
	unsigned long bestb=16;
	unsigned long bestt=16;
	int64 besttime=std::numeric_limits<int64>::max();

	if(m_requestedgrid>0 && m_requestedgrid<=65536)
	{
		lowb=m_requestedgrid;
		highb=m_requestedgrid;
	}

	if(m_requestedthreads>0 && m_requestedthreads<=65536)
	{
		lowt=m_requestedthreads;
		hight=m_requestedthreads;
	}

	for(int numb=lowb; numb<=highb; numb*=2)
	{
		for(int numt=lowt; numt<=hight; numt*=2)
		{
			AllocateResources(numb,numt);
			// clear out any existing error
			cudaError_t err=cudaGetLastError();
			err=cudaSuccess;

			int64 st=GetTimeMillis();

			for(int it=0; it<128*256*2 && err==0; it+=(numb*numt))
			{
				cutilSafeCall(cudaMemcpy(m_devin,m_in,sizeof(cuda_in),cudaMemcpyHostToDevice));

				cuda_process_helper(m_devin,m_devout,64,6,numb,numt);

				cutilSafeCall(cudaMemcpy(m_out,m_devout,numb*numt*sizeof(cuda_out),cudaMemcpyDeviceToHost));

				err=cudaGetLastError();
				if(err!=cudaSuccess)
				{
					printf("CUDA error %d\n",err);
				}
			}

			int64 et=GetTimeMillis();

			printf("Finding best configuration step end (%d,%d) %"PRI64d"ms  prev best=%"PRI64d"ms\n",numb,numt,et-st,besttime);

			if((et-st)<besttime && err==cudaSuccess)
			{
				bestb=numb;
				bestt=numt;
				besttime=et-st;
			}
		}
	}

	m_numb=bestb;
	m_numt=bestt;

	AllocateResources(m_numb,m_numt);

}

const unsigned long CUDARunner::RunStep()
{
	unsigned long best=0;
	unsigned long bestg=~0;

	if(m_in==0 || m_out==0 || m_devin==0 || m_devout==0)
	{
		AllocateResources(m_numb,m_numt);
	}

	cutilSafeCall(cudaMemcpy(m_devin,m_in,sizeof(cuda_in),cudaMemcpyHostToDevice));

	cuda_process_helper(m_devin,m_devout,GetStepIterations(),GetStepBitShift(),m_numb,m_numt);

	cutilSafeCall(cudaMemcpy(m_out,m_devout,m_numb*m_numt*sizeof(cuda_out),cudaMemcpyDeviceToHost));

	for(int i=0; i<m_numb*m_numt; i++)
	{
		if(m_out[i].m_bestnonce!=0 && m_out[i].m_bestg<bestg)
		{
			best=m_out[i].m_bestnonce;
			bestg=m_out[i].m_bestg;
		}
	}

	return best;

}

#endif	// _BITCOIN_MINER_CUDA_
