Distributed binaries may or may not have one or more of the following features 
enabled.


*********************
* REMOTE MINER SERVER
*********************
Place a text file named banned.txt with IP addresses of banned clients in the 
bitcoin working directory.  One IP address should be entered per line, so each
IP address is separated by a newline.

Remote miner server arguments

-remoteserver
	Turns on the remote server.
	
-remotebindaddr=x.x.x.x
	Bind server to specific adapter.  The default is 127.0.0.1.  Note that this 
	will only accept connections from the local computer.
	
-remotebindport=xxxxx
	Bind server to specific port.  The default is 8335.
	
-remotepassword=xxxxx
	Set a password to access the server.  The default is a blank password.
	
-distributiontype=connected|contributed
	Sets method used to distribute bitcoins.  "connected" will distribute coins
	only to those clients that were connected when the block being solved was
	created.  The distribution is based on each connected clients calculated hash 
	rate against the total hash rate at the time a new block is created. 
	"contributed" will accrue all hashes sent to the server for a given address 
	since the last generated block.  A client may freely disconnect and reconnect 
	and will continue accumulating hashes to whatever address the client specified.
	The distribution of coins with this method is based on the hashes accrued by 
	each address against the total hashes accured by everyone.  The server will
	save the values when it shuts down and load them back up on startup.

-distributionpercent=0.0 to 1.0
        Sets the percentage of the generated coins that will be distributed to
        contributers. The remaining is collected by the server. The value must
        be between 0.0 and 1.0 and default to 1.0.
	
-resethashescontributed
	Resets the count of hashes contributed from each address.


*********************
* REMOTE MINER CLIENT 
*********************
(ARGUMENTS MAY BE COMBINED WITH CUDA/OPENCL ARGUMENTS)
Remote miner client arguments

-server=x.x.x.x
	The address of the server to connect to.  The default is 127.0.0.1.
	
-port=xxxxx
	The port of the server.  The default is 8335.
	
-password=xxxxx
	The password to use when connecting to the server.  The default is a blank 
	password.
	
-address=xxxxxxx
	The bitcoin address you want generated coins sent to.  The default is blank.  
	If the server is using the "connected" ditribution type, a blank address will 
	make the client's share of generated coins be kept by the server.  If the 
	server is using the "contributed" distribution type, a blank address will
	mean the client contributes as normal, but the contribution is ignored when
	determining how to distribute any coins.

-threads=x
	Start this number of miner threads.  The default value is the number of cores
	on your processor if using a CPU miner, or 1 if using a GPU miner.


*********************
* RPC MINER CLIENT 
*********************
(ARGUMENTS MAY BE COMBINED WITH CUDA/OPENCL ARGUMENTS)
RPC miner client arguments

-url=http://example.com:8332
	The URL of the RPC server.
	
-user=username
	The username used to connect to the RPC server.
	
-password=password
	The password used to connect to the RPC server.
	
-threads=x
	Start this number of miner threads.  The default value is the number of cores
	on your processor if using a CPU miner, or 1 if using a GPU miner.
	
-workrefreshms=xxxx
	Work will be refreshed from the server this often.  Each thread that is 
	started needs its own work.  The default value is 4000ms.  If you have a
	fast miner, or are using lots of threads, you might want to reduce this.

-statsurl=http://example.com/stats/json
	The URL of a server that will respond with a json object of the server stats.
	Currently, only slush's server stats are available.  The stats will be
	printed by the client every minute.  Not specifying a url will result in no
	stats being displayed.


*********************
* CUDA MINER
*********************
CUDA miner arguments

-gpu=X
	Turns on GPU processing on specific GPU device.  Indexes start at 0.  If you 
	just use -gpu without =X it will pick the device with the max GFlops.

-aggression=X
	Specifies how many hashes (2^(X-1)) per kernel thread will be calculated.  
	The default is 6.  It starts at 1 and goes to 32, with each successive
	number meaning double the number of hashes.  Sane values are 1 to 12 or
	maybe 14 if you have some super card.

-gpugrid=X
	Specifies what the grid size of the kernel should be.  Useful for fine tuning 
	hash rate.

-gputhreads=X
	Specifies how many threads per kernel invocation should run.  Useful for fine 
	tuning hash rate.

-port=X
	Specifies the port that bitcoin will listen on.  (When run in GUI or daemon)

-rpcport=X
	Specifies the port that the rpc server will listen on.  (When run in GUI or daemon)



*********************
* OPENCL MINER
*********************
Make sure bitcoinmineropencl.cl is in the bitcoin working directory for the server and 
rpc miner, and remotebitcoinmineropencl.cl for the remote miner.

OpenCL miner arguments

-platform=X
	Use specific OpenCL platform at index X.  Indexes start at 0.  Default is 0.
	
-gpu=X
	Turns on GPU processing on specific GPU device on specified platform.
	Indexes start at 0.  If you just use -gpu without =X it will pick the first 
	device found.

-aggression=X
	Specifies how many hashes (2^(X-1)) per kernel thread will be calculated.  
	The default is 6.  It starts at 1 and goes to 32, with each successive
	number meaning double the number of hashes.  Sane values are 1 to 12 or
	maybe 14 if you have some super card.

-gpugrid=X
	Specifies what the grid size of the kernel should be.  Useful for fine tuning 
	hash rate.

-gputhreads=X
	Specifies how many threads per kernel invocation should run.  Useful for fine 
	tuning hash rate.

-port=X
	Specifies the port that bitcoin will listen on.  (When run in GUI or daemon)

-rpcport=X
	Specifies the port that the rpc server will listen on.  (When run in GUI or daemon)
