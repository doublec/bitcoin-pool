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

#include "rpcminerclient.h"

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

void printhelp()
{
	std::cout << "RPC miner arguments" << std::endl << std::endl;

	std::cout << "-url=http://example.com:8332" << std::endl;
	std::cout << "  The URL of the RPC server." << std::endl;
	std::cout << std::endl;

	std::cout << "-user=username" << std::endl;
	std::cout << "  The username used to connect to the RPC server." << std::endl;
	std::cout << std::endl;

	std::cout << "-password=password" << std::endl;
	std::cout << "  The password used to connect to the RPC server." << std::endl;
	std::cout << std::endl;

	std::cout << "-threads=x" << std::endl;
	std::cout << "  Start this number of miner threads.  The default value is the number of cores" << std::endl;
	std::cout << "  on your processor if using a CPU miner, or 1 if using a GPU miner." << std::endl;
	std::cout << std::endl;

	std::cout << "-statusurl=http://example.com/stats/json/" << std::endl;
	std::cout << "  The URL of a server that will respond with a json object of the server stats." << std::endl;
	std::cout << "  Currently, only slush's server stats are available.  The stats will be" << std::endl;
	std::cout << "  printed by the client every minute.  Not specifying a url will result in no" << std::endl;
	std::cout << "  stats being displayed." << std::endl;
	std::cout << std::endl;

	std::cout << "-workrefreshms=xxxx" << std::endl;
	std::cout << "  Work will be refreshed from the server this often.  Each thread that is" << std::endl;
	std::cout << "  started needs its own work.  The default value is 4000ms.  If you have a" << std::endl;
	std::cout << "  fast miner, or are using lots of threads, you might want to reduce this." << std::endl;
	std::cout << std::endl;

#if defined(_BITCOIN_MINER_CUDA_) || defined(_BITCOIN_MINER_OPENCL_)
	std::cout << "-aggression=xxx" << std::endl;
	std::cout << "  Specifies how many hashes (2^(X-1)) per kernel thread will be calculated." << std::endl;
	std::cout << "  The default is 6.  It starts at 1 and goes to 32, with each successive" << std::endl;
	std::cout << "  number meaning double the number of hashes.  Sane values are 1 to 12 or" << std::endl;
	std::cout << "  maybe 14 if you have some super card." << std::endl;
	std::cout << std::endl;

	std::cout << "-gpu | -gpu=x" << std::endl;
	std::cout << "  Turns on GPU processing on specific GPU device.  Indexes start at 0.  If you" << std::endl;
	std::cout << "  just use -gpu without =X it will pick the device with the max GFlops when" << std::endl;
	std::cout << "  using the CUDA miner and the first device when using OpenCL." << std::endl;
	std::cout << std::endl;

	std::cout << "-gpugrid=x" << std::endl;
	std::cout << "  Specifies what the grid size of the kernel should be.  Useful for fine tuning" << std::endl;
	std::cout << "  hash rate." << std::endl;
	std::cout << std::endl;

	std::cout << "-gputhreads=x" << std::endl;
	std::cout << "  Specifies how many threads per kernel invocation should run.  Useful for fine" << std::endl;
	std::cout << "  tuning hash rate." << std::endl;
	std::cout << std::endl;
#endif

#if defined(_BITCOIN_MINER_OPENCL_)
	std::cout << "-platform=x" << std::endl;
	std::cout << "  Use specific OpenCL platform at index X.  Indexes start at 0.  Default is 0." << std::endl;
	std::cout << std::endl;
#endif

}

int main(int argc, char *argv[])
{
	std::string url("http://127.0.0.1:8332");
	std::string user("");
	std::string password("");
	std::string statsurl("");
	int workrefreshms=4000;
	int threadcount=1;

	ParseParameters(argc,argv);

	RPCMinerClient client;

	if(mapArgs.count("-url")>0)
	{
		url=mapArgs["-url"];
	}
	if(mapArgs.count("-user")>0)
	{
		user=mapArgs["-user"];
	}
	if(mapArgs.count("-password")>0)
	{
		password=mapArgs["-password"];
	}
	if(mapArgs.count("-threads")>0)
	{
		std::istringstream istr(mapArgs["-threads"]);
		istr >> threadcount;
	}
	else
	{
#if !defined(_BITCOIN_MINER_CUDA_) && !defined(_BITCOIN_MINER_OPENCL_)
		threadcount=boost::thread::hardware_concurrency();
#endif
	}
	if(mapArgs.count("-statsurl")>0)
	{
		statsurl=mapArgs["-statsurl"];
		client.SetServerStatsURL(mapArgs["-statsurl"]);
	}
	if(mapArgs.count("-workrefreshms")>0)
	{
		int ms=4000;
		std::istringstream istr(mapArgs["-workrefreshms"]);
		if(istr >> ms)
		{
			client.SetWorkRefreshMS(ms);
		}
	}

	if(mapArgs.count("-help")>0 || mapArgs.count("-?")>0 || mapArgs.count("/?")>0)
	{
		printhelp();
	}
	else
	{
		client.Run(url,user,password,threadcount);
	}

	return 0;
}
