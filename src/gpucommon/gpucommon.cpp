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

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)

#define NOMINMAX

#include "gpucommon.h"
#include "../cryptopp/misc.h"
#ifdef _BITCOIN_MINER_CUDA_
#include "../cuda/bitcoinminercuda.h"
#endif	// _BITCOIN_MINER_CUDA_
#ifdef _BITCOIN_MINER_OPENCL_
#include "../opencl/bitcoinmineropencl.h"
#endif	// _BITCOIN_MINER_OPENCL_

#include <sstream>

void ThreadBitcoinMinerGPU(void* parg)
{
    try
    {
        vnThreadsRunning[3]++;
        BitcoinMinerGPU();
        vnThreadsRunning[3]--;
    }
    catch (std::exception& e) {
        vnThreadsRunning[3]--;
        PrintException(&e, "ThreadBitcoinMinerGPU()");
    } catch (...) {
        vnThreadsRunning[3]--;
        PrintException(NULL, "ThreadBitcoinMinerGPU()");
    }
    UIThreadCall(bind(CalledSetStatusBar, "", 0));
    nHPSTimerStart = 0;
    if (vnThreadsRunning[3] == 0)
        dHashesPerSec = 0;
    printf("ThreadBitcoinMinerGPU exiting, %d threads remaining\n", vnThreadsRunning[3]);
}

void BitcoinMinerGPU()
{
	printf("BitcoinMinerGPU started\n");
	SetThreadPriority(THREAD_PRIORITY_NORMAL);

	static const unsigned int pSHA256InitState[8] =
	{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    
#ifdef _BITCOIN_MINER_CUDA_
	typedef CUDARunner GPURUNNERTYPE;
#endif
#ifdef _BITCOIN_MINER_OPENCL_
	typedef OpenCLRunner GPURUNNERTYPE;
#endif

	GPURUNNERTYPE gpurunner;

	if(gpurunner.GetDeviceIndex()<=-1)
	{
		printf("BitcoinMinerGPU unable to find a usable device.  Shutting down generation and falling back to CPU.\n");
		mapArgs.erase("-gpu");
		fGenerateBitcoins=false;
		return;
	}

	printf("BitcoinMinerGPU finding best configuration\n");
	gpurunner.FindBestConfiguration();
	printf("BitcoinMinerGPU found best configuration (%d,%d)\n",gpurunner.GetNumBlocks(),gpurunner.GetNumThreads());
	printf("BitcoinMinerGPU GPU iterations=%u bits=%u\n",gpurunner.GetStepIterations(),gpurunner.GetStepBitShift());

	CKey key;
	key.MakeNewKey();
	CBigNum bnExtraNonce=0;
	while(fGenerateBitcoins)
	{
		Sleep(50);
		if(fShutdown)
			return;

		while(vNodes.empty() || IsInitialBlockDownload())
		{
			Sleep(1000);
			if(fShutdown)
				return;
			if(!fGenerateBitcoins)
				return;	
		}

		unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
		CBlockIndex* pindexPrev = pindexBest;
		unsigned int nBits = GetNextWorkRequired(pindexPrev);

		//
		// Create coinbase tx
		//
		CTransaction txNew;
		txNew.vin.resize(1);
		txNew.vin[0].prevout.SetNull();
		txNew.vin[0].scriptSig << nBits << ++bnExtraNonce;
		txNew.vout.resize(1);
		txNew.vout[0].scriptPubKey << key.GetPubKey() << OP_CHECKSIG;

		//
		// Create new block
		//
		auto_ptr<CBlock> pblock(new CBlock());
		if (!pblock.get())
			return;

		// Add our coinbase tx as first transaction
		pblock->vtx.push_back(txNew);

		// Collect the latest transactions into the block
		int64 nFees = 0;
		CRITICAL_BLOCK(cs_main)
		CRITICAL_BLOCK(cs_mapTransactions)
		{
			CTxDB txdb("r");
			map<uint256, CTxIndex> mapTestPool;
			vector<char> vfAlreadyAdded(mapTransactions.size());
			bool fFoundSomething = true;
			unsigned int nBlockSize = 0;
			while (fFoundSomething && nBlockSize < MAX_SIZE/2)
			{
				fFoundSomething = false;
				unsigned int n = 0;
				for (map<uint256, CTransaction>::iterator mi = mapTransactions.begin(); mi != mapTransactions.end(); ++mi, ++n)
				{
					if (vfAlreadyAdded[n])
						continue;
					CTransaction& tx = (*mi).second;
					if (tx.IsCoinBase() || !tx.IsFinal())
						continue;
					unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK);
					if (nBlockSize + nTxSize >= MAX_BLOCK_SIZE - 10000)
						continue;

					// Transaction fee based on block size
					int64 nMinFee = tx.GetMinFee(nBlockSize);

					map<uint256, CTxIndex> mapTestPoolTmp(mapTestPool);
					if (!tx.ConnectInputs(txdb, mapTestPoolTmp, CDiskTxPos(1,1,1), 0, nFees, false, true, nMinFee))
						continue;
					swap(mapTestPool, mapTestPoolTmp);

					pblock->vtx.push_back(tx);
					nBlockSize += nTxSize;
					vfAlreadyAdded[n] = true;
					fFoundSomething = true;
				}
			}
		}
		pblock->nBits = nBits;
		pblock->vtx[0].vout[0].nValue = GetBlockValue(pindexPrev->nHeight+1, nFees);

		printf("Running BitcoinMinerGPU with %d transactions in block\n", pblock->vtx.size());

        //
        // Prebuild hash buffer
        //
        struct tmpworkspace
        {
            struct unnamed2
            {
                int nVersion;
                uint256 hashPrevBlock;
                uint256 hashMerkleRoot;
                unsigned int nTime;
                unsigned int nBits;
                unsigned int nNonce;
            }
            block;
            unsigned char pchPadding0[64];
            uint256 hash1;
            unsigned char pchPadding1[64];
        };
        char tmpbuf[sizeof(tmpworkspace)+16];
        tmpworkspace& tmp = *(tmpworkspace*)alignup<16>(tmpbuf);

        tmp.block.nVersion       = pblock->nVersion;
        tmp.block.hashPrevBlock  = pblock->hashPrevBlock  = (pindexPrev ? pindexPrev->GetBlockHash() : 0);
        tmp.block.hashMerkleRoot = pblock->hashMerkleRoot = pblock->BuildMerkleTree();
        tmp.block.nTime          = pblock->nTime          = max((pindexPrev ? pindexPrev->GetMedianTimePast()+1 : 0), GetAdjustedTime());
        tmp.block.nBits          = pblock->nBits          = nBits;
        tmp.block.nNonce         = pblock->nNonce         = 0;

		/*
		// test with genesis block
        // Genesis Block:
        // GetHash()      = 0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f
        // hashMerkleRoot = 0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b
        // txNew.vin[0].scriptSig     = 486604799 4 0x736B6E616220726F662074756F6C69616220646E6F63657320666F206B6E697262206E6F20726F6C6C65636E61684320393030322F6E614A2F33302073656D695420656854
        // txNew.vout[0].nValue       = 5000000000
        // txNew.vout[0].scriptPubKey = 0x5F1DF16B2B704C8A578D0BBAF74D385CDE12C11EE50455F3C438EF4C3FBCF649B6DE611FEAE06279A60939E028A8D65C10B73071A6F16719274855FEB0FD8A6704 OP_CHECKSIG
        // block.nVersion = 1
        // block.nTime    = 1231006505
        // block.nBits    = 0x1d00ffff
        // block.nNonce   = 2083236893
        // CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
        //   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        //     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
        //     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
        //   vMerkleTree: 4a5e1e
        tmp.block.nVersion       = pblock->nVersion=1;
        tmp.block.hashPrevBlock  = pblock->hashPrevBlock  = 0;
        tmp.block.hashMerkleRoot = pblock->hashMerkleRoot = uint256("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b");
        tmp.block.nTime          = pblock->nTime          = 1231006505;
        tmp.block.nBits          = pblock->nBits          = 0x1d00ffff;
        //tmp.block.nNonce         = pblock->nNonce         = 2083236800;
		//tmp.block.nNonce         = pblock->nNonce         = 100000000;
		tmp.block.nNonce         = pblock->nNonce         = 2080000000;
		*/

        unsigned int nBlocks0 = FormatHashBlocks(&tmp.block, sizeof(tmp.block));
        unsigned int nBlocks1 = FormatHashBlocks(&tmp.hash1, sizeof(tmp.hash1));

        // Byte swap all the input buffer
        for (int i = 0; i < sizeof(tmp)/4; i++)
			((unsigned int*)&tmp)[i] = CryptoPP::ByteReverse(((unsigned int*)&tmp)[i]);

        // Precalc the first half of the first hash, which stays constant
        uint256 midstatebuf[2];
        uint256& midstate = *alignup<16>(midstatebuf);
        SHA256Transform(&midstate, &tmp.block, pSHA256InitState);

		//
		// Search
		//
		int64 nStart = GetTime();
		uint256 hashTarget = CBigNum().SetCompact(pblock->nBits).getuint256();
		uint256 hashbuf[2];
		uint256& hash = *alignup<16>(hashbuf);

		printf("BitcoinMinerGPU searching.  Hash target=%s\n",hashTarget.GetHex().c_str());

		gpurunner.GetIn()->m_AH[0]=((long *)&midstate)[0];
		gpurunner.GetIn()->m_AH[1]=((long *)&midstate)[1];
		gpurunner.GetIn()->m_AH[2]=((long *)&midstate)[2];
		gpurunner.GetIn()->m_AH[3]=((long *)&midstate)[3];
		gpurunner.GetIn()->m_AH[4]=((long *)&midstate)[4];
		gpurunner.GetIn()->m_AH[5]=((long *)&midstate)[5];
		gpurunner.GetIn()->m_AH[6]=((long *)&midstate)[6];
		gpurunner.GetIn()->m_AH[7]=((long *)&midstate)[7];
		gpurunner.GetIn()->m_merkle=(CryptoPP::word32)((unsigned long *)((char *)&tmp.block+64))[0];
		gpurunner.GetIn()->m_nbits=(CryptoPP::word32)((unsigned long *)((char *)&tmp.block+64))[2];

		loop
		{
			gpurunner.GetIn()->m_ntime=(CryptoPP::word32)((unsigned long *)((char *)&tmp.block+64))[1];
			gpurunner.GetIn()->m_nonce=CryptoPP::ByteReverse((CryptoPP::word32)((unsigned long *)((char *)&tmp.block+64))[3]);

			GPURUNNERTYPE::StepType best=gpurunner.RunStep();

			if(best!=0)
			{
		
				tmp.block.nNonce=CryptoPP::ByteReverse((CryptoPP::word32)best);

				SHA256Transform(&tmp.hash1, (char*)&tmp.block + 64, &midstate);
				SHA256Transform(&hash, &tmp.hash1, pSHA256InitState);

				printf("BitcoinMinerGPU found candidate nonce %u  h14=%u h15=%u\n",best,((unsigned short*)&hash)[14],((unsigned short*)&hash)[15]);

				if (((unsigned short*)&hash)[14] == 0)
				{
					// Byte swap the result after preliminary check
					for (int i = 0; i < sizeof(hash)/4; i++)
						((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);

					if (hash <= hashTarget)
					{
						pblock->nNonce = CryptoPP::ByteReverse(tmp.block.nNonce);
						assert(hash == pblock->GetHash());

							//// debug print
							printf("BitcoinMiner:\n");
							printf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex().c_str(), hashTarget.GetHex().c_str());
							pblock->print();
							printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
							printf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue).c_str());

						SetThreadPriority(THREAD_PRIORITY_NORMAL);
						CRITICAL_BLOCK(cs_main)
						{
							if (pindexPrev == pindexBest)
							{
								// Save key
								if (!AddKey(key))
									return;

								key.MakeNewKey();

								// Track how many getdata requests this block gets
								CRITICAL_BLOCK(cs_mapRequestCount)
									mapRequestCount[pblock->GetHash()] = 0;

								// Process this block the same as if we had received it from another node
								if (!ProcessBlock(NULL, pblock.release()))
								{
									printf("ERROR in BitcoinMiner, ProcessBlock, block not accepted\n");
								}
							}
						}
						SetThreadPriority(THREAD_PRIORITY_LOWEST);

						Sleep(500);
						break;
					}
				}
			}


			// Update nTime every few seconds
			//const unsigned int nMask = 0xffff;
			tmp.block.nNonce=(CryptoPP::ByteReverse(tmp.block.nNonce)+(gpurunner.GetNumBlocks()*gpurunner.GetNumThreads()*gpurunner.GetStepIterations()));
			tmp.block.nNonce=CryptoPP::ByteReverse(tmp.block.nNonce);
			//if ((tmp.block.nNonce & nMask) == 0)
			{
				// Meter hashes/sec
				static int64 nTimerStart;
				static int nGPUHashCounter;
				if (nTimerStart == 0)
					nTimerStart = GetTimeMillis();
				else
					nGPUHashCounter++;
				if (GetTimeMillis() - nTimerStart > 4000)
				{
					static CCriticalSection cs;
					CRITICAL_BLOCK(cs)
					{
						if (GetTimeMillis() - nTimerStart > 4000)
						{
							double dHashesPerSec = 1000.0 * (gpurunner.GetNumBlocks()*gpurunner.GetNumThreads()*gpurunner.GetStepIterations()) * nGPUHashCounter / (GetTimeMillis() - nTimerStart);
							nTimerStart = GetTimeMillis();
							nGPUHashCounter = 0;
							string strStatus = strprintf(" GPU %.0f khash/s", dHashesPerSec/1000.0);
							UIThreadCall(bind(CalledSetStatusBar, strStatus, 0));
							static int64 nLogTime;
							if (GetTime() - nLogTime > 30 * 60)
							{
								nLogTime = GetTime();
								printf("%s ", DateTimeStrFormat("%x %H:%M", GetTime()).c_str());
								printf("hashmeter %3d GPUs %6.0f khash/s\n", vnThreadsRunning[3], dHashesPerSec/1000.0);
							}
						}
					}
				}

				// Check for stop or if block needs to be rebuilt
				if (fShutdown)
					return;
				if (!fGenerateBitcoins)
					return;
				if (fLimitProcessors && vnThreadsRunning[3] > nLimitProcessors)
					return;
				if (vNodes.empty())
					break;
				if (tmp.block.nNonce == 0)
					break;
				if (nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 60)
					break;
				if (pindexPrev != pindexBest)
					break;

				pblock->nTime = max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());
				tmp.block.nTime = CryptoPP::ByteReverse(pblock->nTime);
			}
		}
		
	}	// while(fGenerateBitcoins)

}

#endif
