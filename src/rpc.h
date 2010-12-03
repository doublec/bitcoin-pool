// Copyright (c) 2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _bitcoin_rpc_h_
#define _bitcoin_rpc_h_

void ThreadRPCServer(void* parg);
int CommandLineRPC(int argc, char *argv[]);

#endif	// _bitcoin_rpc_h_
