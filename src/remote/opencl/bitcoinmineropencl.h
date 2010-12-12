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

#ifndef _bitcoin_remote_miner_opencl_
#define _bitcoin_remote_miner_opencl_

#ifdef _BITCOIN_MINER_OPENCL_

#include "../../gpucommon/gpurunner.h"
#include "openclshared.h"
#include <string>

class RemoteOpenCLRunner:public GPURunner<cl_uint,cl_uint>
{
public:
	RemoteOpenCLRunner();
	~RemoteOpenCLRunner();

	void FindBestConfiguration();

	const cl_uint RunStep();

	void SetMetaHashSize(const long size)	{ m_metahashsize=size; }
	remote_opencl_in *GetIn()				{ return m_in; }
	remote_opencl_out *GetOut()				{ return m_out; }
	cl_uchar *GetMetaHash()					{ return m_metahash; }

private:
	void DeallocateResources();
	void AllocateResources(const int numb, const int numt);

	const std::string ReadFileContents(const std::string &filename) const;

	long m_metahashsize;
	remote_opencl_in *m_in;
	cl_mem m_devin;
	remote_opencl_out *m_out;
	cl_mem m_devout;
	cl_uchar *m_metahash;
	cl_mem m_devmetahash;

	int m_platform;
	cl_device_id m_device;
	cl_context m_context;
	cl_command_queue m_commandqueue;
	cl_program m_program;
	cl_kernel m_kernel;

};

#endif	// _BITCOIN_MINER_OPENCL_

#endif	// _bitcoin_remote_miner_opencl_
