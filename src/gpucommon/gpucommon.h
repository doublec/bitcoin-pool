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
#include "gpurunner.h"

#include <sstream>

int FormatHashBlocks(void* pbuffer, unsigned int len);
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast);
void SHA256Transform(void* pstate, void* pinput, const void* pinit);
bool ProcessBlock(CNode* pfrom, CBlock* pblock);

extern CCriticalSection cs_mapTransactions;

void ThreadBitcoinMinerGPU(void* parg);
void BitcoinMinerGPU();

#endif	// _gpu_common_
