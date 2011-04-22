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

#ifndef _miner_headers_
#define _miner_headers_

#ifdef _WIN32

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0500
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif

#include <winsock2.h>
#include <windows.h>
#endif	// _WIN32

#ifndef _WIN32
#include <sys/time.h>			// for setpriority
#include <sys/resource.h>		// for setpriority
#endif

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_recursive_mutex.hpp>
#include <cassert>
#include <utility>
#include <algorithm>
#include <map>
#include <vector>
#include <string>
#include <cstdio>
#include <climits>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include "base64.h"
#include "../serialize.h"
#include "../uint256.h"
#include "../util.h"
#include "../bignum.h"
#include "../base58.h"
#include "../strlcpy.h"

#endif	// _miner_headers_
