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

#include "remote/remotebitcoinheaders.h"
#include "remote/remoteminerclient.h"

bool fTestNet=false;

int main(int argc, char *argv[])
{
	std::string server("127.0.0.1");
	std::string port("8335");
	std::string password("");
	std::string address("");

	for(int i=1; i<argc;)
	{
		if(argv[i])
		{
			std::string arg(argv[i]);
			i++;
			if(arg=="-server" && i<argc)
			{
				server=std::string(argv[i]);
			}
			if(arg=="-port" && i<argc)
			{
				port=std::string(argv[i]);
			}
			if(arg=="-password" && i<argc)
			{
				password=std::string(argv[i]);
			}
			if(arg=="-address" && i<argc)
			{
				address=std::string(argv[i]);
				uint160 h160;
				if(AddressToHash160(address.c_str(),h160)==false)
				{
					std::cout << "Address is invalid" << std::endl;
					address="";
				}
			}
		}
		else
		{
			i++;
		}
	}

	RemoteMinerClient client;
	client.Run(server,port,password,address);

	return 0;
}
