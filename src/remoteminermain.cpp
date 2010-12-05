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
#include "remote/remoteminerclientcuda.h"

bool fTestNet=false;
std::map<std::string,std::string> mapArgs;
std::map<std::string,std::vector<std::string> > mapMultiArgs;

void ParseParameters(int argc, char* argv[])
{
    mapArgs.clear();
    mapMultiArgs.clear();
    for (int i = 1; i < argc; i++)
    {
        char psz[10000];
        strlcpy(psz, argv[i], sizeof(psz));
        char* pszValue = (char*)"";
        if (strchr(psz, '='))
        {
            pszValue = strchr(psz, '=');
            *pszValue++ = '\0';
        }
        #ifdef __WXMSW__
        _strlwr(psz);
        if (psz[0] == '/')
            psz[0] = '-';
        #endif
        if (psz[0] != '-')
            break;
        mapArgs[psz] = pszValue;
        mapMultiArgs[psz].push_back(pszValue);
    }
}

int main(int argc, char *argv[])
{
	std::string server("127.0.0.1");
	std::string port("8335");
	std::string password("");
	std::string address("");

	ParseParameters(argc,argv);

	if(mapArgs.count("-server")>0)
	{
		server=mapArgs["-server"];
	}
	if(mapArgs.count("-port")>0)
	{
		port=mapArgs["-port"];
	}
	if(mapArgs.count("-password")>0)
	{
		password=mapArgs["-password"];
	}
	if(mapArgs.count("-address")>0)
	{
		address=mapArgs["-address"];
		uint160 h160;
		if(AddressToHash160(address.c_str(),h160)==false)
		{
			std::cout << "Address is invalid" << std::endl;
			address="";
		}
	}


#if  defined(_BITCOIN_MINER_CUDA_)
	RemoteMinerClientCUDA client;
#elif defined(_BITCOIN_MINER_OPENCL_)
	RemoteMinerClientOpenCL client;
#else
	RemoteMinerClient client;
#endif

	client.Run(server,port,password,address);


	return 0;
}
