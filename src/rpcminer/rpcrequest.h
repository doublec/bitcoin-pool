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

#ifndef _rpc_request_
#define _rpc_request_

#include <string>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include "../headers.h"

#include <vector>

class RPCRequest
{
public:
	RPCRequest(const std::string &url, const std::string &user, const std::string &password);
	~RPCRequest();

	const bool DoRequest(const std::string &data, std::string &result);

private:

	static size_t WriteData(void *ptr, size_t size, size_t nmemb, void *user_data);
	static size_t ReadData(void *ptr, size_t size, size_t nmemb, void *user_data);

	std::string m_url;
	std::string m_user;
	std::string m_password;

	std::vector<char> m_writebuff;
	std::vector<char> m_readbuff;
};

#endif	// _rpc_request_
