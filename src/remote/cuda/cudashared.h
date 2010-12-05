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

#ifndef _remote_cuda_shared_
#define _remote_cuda_shared_

#ifdef _BITCOIN_MINER_CUDA_

typedef struct
{
	unsigned int m_AH[8];
	unsigned int m_merkle;
	unsigned int m_ntime;
	unsigned int m_nbits;
	unsigned int m_nonce;
}remote_cuda_in;

typedef struct
{
	unsigned int m_bestnonce;
	unsigned int m_bestAH[8];
	unsigned int m_nonce;
}remote_cuda_out;

void remote_cuda_process_helper(remote_cuda_in *in, remote_cuda_out *out, unsigned char *metahash, const unsigned int loops, const unsigned int bits, const int grid, const int threads);

#endif	// _BITCOIN_MINER_CUDA_

#endif	// _remote_cuda_shared_
