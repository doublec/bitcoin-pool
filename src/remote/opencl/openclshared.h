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

#ifndef _remote_opencl_shared_
#define _remote_opencl_shared_

#ifdef _BITCOIN_MINER_OPENCL_

#include <CL/opencl.h>

typedef struct
{
	cl_uint m_AH[8];
	cl_uint m_merkle;
	cl_uint m_ntime;
	cl_uint m_nbits;
	cl_uint m_nonce;
}remote_opencl_in;

typedef struct
{
	cl_uint m_bestnonce;
	cl_uint m_bestAH[8];
//debug
	cl_uint m_nonce;
	cl_uint m_myid;
	cl_uint m_loops;
}remote_opencl_out;

void remote_cuda_process_helper(remote_opencl_in *in, remote_opencl_out *out, unsigned char *metahash, const unsigned int loops, const unsigned int bits, const int grid, const int threads);

#endif	// _BITCOIN_MINER_OPENCL_

#endif	// _remote_opencl_shared_
