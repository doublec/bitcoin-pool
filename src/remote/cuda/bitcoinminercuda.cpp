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
#include "../remotebitcoinheaders.h"		// util.h for int64
#include <cutil_inline.h>
#include <limits>
#include <iostream>

RemoteCUDARunner::RemoteCUDARunner():GPURunner<unsigned long,int>(TYPE_CUDA),m_metahashsize(0)
{
	m_in=0;
	m_devin=0;
	m_out=0;
	m_devout=0;
	m_metahash=0;
	m_devmetahash=0;

	cutilSafeCall(cudaGetDeviceCount(&m_devicecount));

	if(m_devicecount>0)
	{
		if(m_deviceindex<0 || m_deviceindex>=m_devicecount)
		{
			m_deviceindex=cutGetMaxGflopsDeviceId();
			std::cout << "Setting CUDA device to Max GFlops device at index " << m_deviceindex << std::endl;
		}
		else
		{
			std::cout << "Setting CUDA device to device at index " << m_deviceindex << std::endl;
		}
		
		cudaDeviceProp props;
		cudaGetDeviceProperties(&props,m_deviceindex);

		std::cout << "Device info for " << props.name << " :" << std::endl;
		std::cout << "Compute Capability : " << props.major << "." << props.minor << std::endl;
		std::cout << "Clock Rate (hz) : " << props.clockRate << std::endl;

		if(props.major>999)
		{
			std::cout << "CUDA seems to be running in CPU emulation mode" << std::endl;
		}

		cutilSafeCall(cudaSetDevice(m_deviceindex));

	}
	else
	{
		m_deviceindex=-1;
		std::cout << "No CUDA capable device detected" << std::endl;
	}
}

RemoteCUDARunner::~RemoteCUDARunner()
{
	DeallocateResources();
	cutilSafeCall(cudaThreadExit());
}

void RemoteCUDARunner::AllocateResources(const int numb, const int numt)
{
	DeallocateResources();

	m_in=(remote_cuda_in *)malloc(sizeof(remote_cuda_in));
	m_out=(remote_cuda_out *)malloc(numb*numt*sizeof(remote_cuda_out));
	m_metahash=(unsigned char *)malloc(numb*numt*GetStepIterations());

	cutilSafeCall(cudaMalloc((void **)&m_devin,sizeof(remote_cuda_in)));
	cutilSafeCall(cudaMalloc((void **)&m_devout,numb*numt*sizeof(remote_cuda_out)));
	cutilSafeCall(cudaMalloc((void **)&m_devmetahash,numb*numt*GetStepIterations()));

	std::cout << "Done allocating CUDA resources for (" << numb << "," << numt << ")" << std::endl;
}

void RemoteCUDARunner::DeallocateResources()
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
	if(m_metahash)
	{
		free(m_metahash);
		m_metahash=0;
	}
	if(m_devmetahash)
	{
		cutilSafeCall(cudaFree(m_devmetahash));
		m_devmetahash=0;
	}
}

void RemoteCUDARunner::FindBestConfiguration()
{
	unsigned long lowb=16;
	unsigned long highb=128;
	unsigned long lowt=16;
	unsigned long hight=256;
	unsigned long bestb=16;
	unsigned long bestt=16;
	int64 besttime=std::numeric_limits<int64>::max();
	int m_savebits=m_bits;

	m_bits=7;

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

	std::cout << "CUDA finding best kernel configuration" << std::endl;

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
				cutilSafeCall(cudaMemcpy(m_devin,m_in,sizeof(remote_cuda_in),cudaMemcpyHostToDevice));

				remote_cuda_process_helper(m_devin,m_devout,m_devmetahash,64,6,numb,numt);

				cutilSafeCall(cudaMemcpy(m_out,m_devout,numb*numt*sizeof(remote_cuda_out),cudaMemcpyDeviceToHost));

				err=cudaGetLastError();
				if(err!=cudaSuccess)
				{
					std::cout << "CUDA error " << err << std::endl;
				}
			}

			int64 et=GetTimeMillis();

			std::cout << "Finding best configuration step end (" << numb << "," << numt << ") " << et-st << "ms  prev best=" << besttime << "ms" << std::endl;

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

	m_bits=m_savebits;

	AllocateResources(m_numb,m_numt);

}

const unsigned long RemoteCUDARunner::RunStep()
{

	if(m_in==0 || m_out==0 || m_devin==0 || m_devout==0)
	{
		AllocateResources(m_numb,m_numt);
	}

	cutilSafeCall(cudaMemcpy(m_devin,m_in,sizeof(remote_cuda_in),cudaMemcpyHostToDevice));

	remote_cuda_process_helper(m_devin,m_devout,m_devmetahash,GetStepIterations(),GetStepBitShift(),m_numb,m_numt);

	cutilSafeCall(cudaMemcpy(m_out,m_devout,m_numb*m_numt*sizeof(remote_cuda_out),cudaMemcpyDeviceToHost));
	cutilSafeCall(cudaMemcpy(m_metahash,m_devmetahash,m_numb*m_numt*GetStepIterations(),cudaMemcpyDeviceToHost));

	return 0;

}

#endif	// _BITCOIN_MINER_CUDA_
