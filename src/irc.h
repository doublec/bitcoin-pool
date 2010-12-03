// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef _bitcoin_irc_h_
#define _bitcoin_irc_h_

bool RecvLine(SOCKET hSocket, string& strLine);
void ThreadIRCSeed(void* parg);

extern int nGotIRCAddresses;

#endif	// _bitcoin_irc_h_
