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

#ifndef _remote_bitcoin_headers_
#define _remote_bitcoin_headers_

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <cassert>
#include <map>
#include <vector>
#include <string>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include "../serialize.h"
#include "../uint256.h"
#include "../util.h"
#include "../bignum.h"
#include "../base58.h"
#include "../strlcpy.h"

#endif	// _remote_bitcoin_headers_
