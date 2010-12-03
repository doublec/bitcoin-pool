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

#ifndef _gpu_common_
#define _gpu_common_

#include "../headers.h"

#include <sstream>

int FormatHashBlocks(void* pbuffer, unsigned int len);
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast);
void SHA256Transform(void* pstate, void* pinput, const void* pinit);
bool ProcessBlock(CNode* pfrom, CBlock* pblock);

extern CCriticalSection cs_mapTransactions;

void ThreadBitcoinMinerGPU(void* parg);
void BitcoinMinerGPU();

template <class STEPTYPE,class DEVICECOUNTTYPE>
class GPURunner
{
public:
	GPURunner(const int type=TYPE_NONE);
	virtual ~GPURunner();

	typedef STEPTYPE StepType;

	enum Type
	{
		TYPE_NONE=0,
		TYPE_CUDA=1,
		TYPE_OPENCL=2
	};

	virtual void FindBestConfiguration()=0;

	const unsigned long GetNumBlocks() const		{ return m_numb; }
	const unsigned long GetNumThreads() const		{ return m_numt; }
	const DEVICECOUNTTYPE GetDeviceCount() const	{ return m_devicecount; }

	const unsigned int GetStepIterations() const	{ return (1u << (m_bits-1)); }
	const unsigned int GetStepBitShift() const		{ return m_bits; }

	const int GetType() const						{ return m_type; }
	const int GetDeviceIndex() const				{ return m_deviceindex; }

	virtual const STEPTYPE RunStep()=0;

protected:
	virtual void DeallocateResources()=0;
	virtual void AllocateResources(const int numb, const int numt)=0;

	DEVICECOUNTTYPE m_devicecount;
	int m_deviceindex;
	unsigned long m_requestedgrid;
	unsigned long m_requestedthreads;
	unsigned long m_numb;
	unsigned long m_numt;
	int m_mode;
	int m_bits;

private:
	int m_type;

};

template <class STEPTYPE,class DEVICECOUNTTYPE>
GPURunner<STEPTYPE,DEVICECOUNTTYPE>::GPURunner(const int type):m_type(type),m_devicecount(0),m_deviceindex(-1),m_numb(16),m_numt(16),m_bits(6),m_requestedgrid(-1),m_requestedthreads(-1)//m_mode(MODE_REGULAR)
{

	if(mapArgs.count("-aggression")>0)
	{
		std::istringstream istr(mapArgs["-aggression"]);
		if((istr >> m_bits))
		{
			if(m_bits>32)
			{
				m_bits=32;
			}
			else if(m_bits<1)
			{
				m_bits=1;
			}
		}
	}

	if(mapArgs.count("-gpu")!=0)
	{
		std::istringstream istr(mapArgs["-gpu"]);
		if(!(istr >> m_deviceindex))
		{
			m_deviceindex=-1;
		}
	}

	if(mapArgs.count("-gpugrid")!=0)
	{
		std::istringstream istr(mapArgs["-gpugrid"]);
		if(!(istr >> m_requestedgrid))
		{
			m_requestedgrid=-1;
		}
	}

	if(mapArgs.count("-gputhreads")!=0)
	{
		std::istringstream istr(mapArgs["-gputhreads"]);
		if(!(istr >> m_requestedthreads))
		{
			m_requestedthreads=-1;
		}
	}

}

template <class STEPTYPE,class DEVICECOUNTTYPE>
GPURunner<STEPTYPE,DEVICECOUNTTYPE>::~GPURunner()
{
	
}

#endif	// _gpu_common_
