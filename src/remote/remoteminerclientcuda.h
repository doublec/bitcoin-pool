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

#ifndef _remote_miner_client_cuda_
#define _remote_miner_client_cuda_

#include "remoteminerclient.h"

class RemoteMinerClientCUDA:public RemoteMinerClient
{
public:
	RemoteMinerClientCUDA();
	virtual ~RemoteMinerClientCUDA();

	virtual void Run(const std::string &server, const std::string &port, const std::string &password, const std::string &address);

};

#endif	// _remote_miner_client_cuda_
