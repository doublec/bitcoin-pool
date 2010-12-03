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

#include "remoteminermessage.h"

RemoteMinerMessage::RemoteMinerMessage()
{

}

RemoteMinerMessage::RemoteMinerMessage(const json_spirit::Value &value):m_value(value)
{

}

RemoteMinerMessage::~RemoteMinerMessage()
{

}

const std::vector<char> RemoteMinerMessage::GetWireData() const
{
	std::string jsonstr=json_spirit::write(m_value);
	std::vector<char> data;
	data.push_back(REMOTEMINER_PROTOCOL_VERSION);			// protocol version;
	data.push_back((jsonstr.size() >> 8) & 0xff);			// size
	data.push_back(jsonstr.size() & 0xff);					// size
	data.insert(data.end(),jsonstr.begin(),jsonstr.end());	// json object

	return data;
}

void RemoteMinerMessage::PushWireData(std::vector<char> &buffer) const
{
	std::string jsonstr=json_spirit::write(m_value);
	buffer.push_back(REMOTEMINER_PROTOCOL_VERSION);
	buffer.push_back((jsonstr.size() >> 8) & 0xff);
	buffer.push_back(jsonstr.size() & 0xff);
	buffer.insert(buffer.end(),jsonstr.begin(),jsonstr.end());
}
