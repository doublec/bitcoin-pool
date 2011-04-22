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

#include "hex.h"

namespace Hex
{

static const std::string hexchars="0123456789abcdef";

const bool Encode(const std::vector<unsigned char> &data, std::string &encoded)
{
	encoded.reserve(data.size()*2);
	for(std::vector<unsigned char>::const_iterator i=data.begin(); i!=data.end(); i++)
	{
		encoded.push_back(hexchars[(((*i)>>4) & 0x0F)]);
		encoded.push_back(hexchars[((*i) & 0x0F)]);
	}
	return true;
}

const bool Decode(const std::string &encoded, std::vector<unsigned char> &data)
{

	std::string::size_type pos=0;
	unsigned char byte;
	int bytepart=0;
	std::string enc(encoded);

	// make upper case lower case
	for(std::string::iterator i=enc.begin(); i!=enc.end(); i++)
	{
		if((*i)>=65 && (*i)<71)
		{
			(*i)=(*i)+32;
		}
	}

	data.reserve(enc.size()/2);
	
	pos=enc.find_first_of(hexchars);

	while(pos!=std::string::npos)
	{
		if(bytepart==0)
		{
			byte=(hexchars.find(enc[pos]) << 4) & 0xF0;
			bytepart=1;
		}
		else
		{
			byte|=hexchars.find(enc[pos]) & 0x0F;
			data.push_back(byte);
			bytepart=0;
		}
		pos=enc.find_first_of(hexchars,pos+1);
	}
	return true;
}

}	// namespace
