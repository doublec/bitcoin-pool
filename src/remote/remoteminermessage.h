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

#ifndef _remote_miner_message_
#define _remote_miner_message_

#include "../json/json_spirit.h"
#include <vector>

const int REMOTEMINER_PROTOCOL_VERSION=1;

class RemoteMinerMessage
{
public:
	RemoteMinerMessage();
	RemoteMinerMessage(const json_spirit::Value &value);
	~RemoteMinerMessage();

	const json_spirit::Value GetValue() const		{ return m_value; }

	const std::vector<char> GetWireData() const;
	void PushWireData(std::vector<char> &buffer) const;

	enum RemoteMinerMessageType
	{
		MESSAGE_TYPE_NONE=0,
		MESSAGE_TYPE_CLIENTHELLO=1,
		MESSAGE_TYPE_SERVERHELLO=2,
		MESSAGE_TYPE_CLIENTGETWORK=3,
		MESSAGE_TYPE_SERVERSENDWORK=4,
		MESSAGE_TYPE_SERVERSTOPPED=5,
		MESSAGE_TYPE_CLIENTSTOPPED=6,
		MESSAGE_TYPE_CLIENTHASHRATE=7,
		MESSAGE_TYPE_CLIENTMETAHASH=8,
		MESSAGE_TYPE_CLIENTFOUNDHASH=9,
		MESSAGE_TYPE_SERVERSTATUS=10,
		MESSAGE_TYPE_MAX
	};

private:
	json_spirit::Value m_value;
};

#endif	// _remote_miner_message_
