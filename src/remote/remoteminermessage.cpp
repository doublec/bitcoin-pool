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
	char flags=0;
	std::string jsonstr=json_spirit::write(m_value);
	std::vector<char> data;
	data.push_back(REMOTEMINER_PROTOCOL_VERSION);			// protocol version;

	if(jsonstr.size()>=65535)
	{
		flags|=FLAG_4BYTESIZE;
	}

	data.push_back(flags);

	if((flags & FLAG_4BYTESIZE)!=FLAG_4BYTESIZE)
	{
		data.push_back((jsonstr.size() >> 8) & 0xff);			// size
		data.push_back(jsonstr.size() & 0xff);					// size
	}
	else
	{
		data.push_back((jsonstr.size() >> 24) & 0xff);
		data.push_back((jsonstr.size() >> 16) & 0xff);
		data.push_back((jsonstr.size() >> 8) & 0xff);
		data.push_back(jsonstr.size() & 0xff);
	}

	data.insert(data.end(),jsonstr.begin(),jsonstr.end());	// json object

	return data;
}

bool RemoteMinerMessage::MessageReady(const std::vector<char> &buffer)
{
	if(buffer.size()>4 && buffer[0]==REMOTEMINER_PROTOCOL_VERSION)
	{
		char flags=buffer[1];
		unsigned long messagesize=0;
		unsigned long headersize=4;

		if((flags & FLAG_4BYTESIZE)!=FLAG_4BYTESIZE)
		{
			headersize=4;
			messagesize|=(buffer[2] << 8) & 0xff00;
			messagesize|=(buffer[3]) & 0xff;
		}
		else
		{
			if(buffer.size()>=6)
			{
				headersize=6;
				messagesize|=(buffer[2] << 24) & 0xff000000;
				messagesize|=(buffer[3] << 16) & 0xff0000;
				messagesize|=(buffer[4] << 8) & 0xff00;
				messagesize|=buffer[5] & 0xff;
			}
			else
			{
				return false;
			}
		}

		if(buffer.size()>=headersize+messagesize)
		{
			return true;
		}
	}

	return false;
}

bool RemoteMinerMessage::ProtocolError(const std::vector<char> &buffer)
{
	if(buffer.size()>0 && buffer[0]!=REMOTEMINER_PROTOCOL_VERSION)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void RemoteMinerMessage::PushWireData(std::vector<char> &buffer) const
{
	char flags=0;
	std::string jsonstr=json_spirit::write(m_value);

	buffer.push_back(REMOTEMINER_PROTOCOL_VERSION);

	if(jsonstr.size()>=65535)
	{
		flags|=FLAG_4BYTESIZE;
	}

	buffer.push_back(flags);

	if((flags & FLAG_4BYTESIZE)!=FLAG_4BYTESIZE)
	{
		buffer.push_back((jsonstr.size() >> 8) & 0xff);			// size
		buffer.push_back(jsonstr.size() & 0xff);					// size
	}
	else
	{
		buffer.push_back((jsonstr.size() >> 24) & 0xff);
		buffer.push_back((jsonstr.size() >> 16) & 0xff);
		buffer.push_back((jsonstr.size() >> 8) & 0xff);
		buffer.push_back(jsonstr.size() & 0xff);
	}

	buffer.insert(buffer.end(),jsonstr.begin(),jsonstr.end());
}

bool RemoteMinerMessage::ReceiveMessage(std::vector<char> &buffer, RemoteMinerMessage &message)
{
	if(MessageReady(buffer)==true)
	{
		char flags=buffer[1];
		unsigned long messagesize=0;
		unsigned long headersize=4;

		if((flags & FLAG_4BYTESIZE)!=FLAG_4BYTESIZE)
		{
			headersize=4;
			messagesize|=(buffer[2] << 8) & 0xff00;
			messagesize|=(buffer[3]) & 0xff;
		}
		else
		{
			if(buffer.size()>=6)
			{
				headersize=6;
				messagesize|=(buffer[2] << 24) & 0xff000000;
				messagesize|=(buffer[3] << 16) & 0xff0000;
				messagesize|=(buffer[4] << 8) & 0xff00;
				messagesize|=buffer[5] & 0xff;
			}
			else
			{
				return false;
			}
		}

		if(buffer.size()>=headersize+messagesize)
		{
			std::string objstr(buffer.begin()+headersize,buffer.begin()+headersize+messagesize);
			json_spirit::Value value;
			bool jsonread=json_spirit::read(objstr,value);
			if(jsonread)
			{
				message=RemoteMinerMessage(value);
			}
			buffer.erase(buffer.begin(),buffer.begin()+headersize+messagesize);
			return jsonread;
		}
	}
	return false;
}
