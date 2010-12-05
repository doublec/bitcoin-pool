#include "remoteminerclientcuda.h"

#ifdef _BITCOIN_MINER_CUDA_

#include "cuda/bitcoinminercuda.h"

RemoteMinerClientCUDA::RemoteMinerClientCUDA():RemoteMinerClient()
{

}

RemoteMinerClientCUDA::~RemoteMinerClientCUDA()
{

}

void RemoteMinerClientCUDA::Run(const std::string &server, const std::string &port, const std::string &password, const std::string &address)
{
	static const unsigned int SHA256InitState[8] ={0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	uint256 tempbuff[4];
	uint256 &temphash=*alignup<16>(tempbuff);
	uint256 hashbuff[4];
	uint256 &hash=*alignup<16>(hashbuff);
	uint256 lasttarget=0;
	RemoteCUDARunner gpu;

	uint256 besthash=~(uint256(0));
	unsigned int besthashnonce=0;

	SetThreadPriority(THREAD_PRIORITY_LOWEST);

	FormatHashBlocks(&temphash,sizeof(temphash));
	for(int i=0; i<64/4; i++)
	{
		((unsigned int*)&temphash)[i] = CryptoPP::ByteReverse(((unsigned int*)&temphash)[i]);
	}

	gpu.FindBestConfiguration();

	while(true)
	{
		if(IsConnected()==false)
		{
			std::cout << "Attempting to connect to " << server << ":" << port << std::endl;
			Sleep(1000);
			if(Connect(server,port))
			{
				m_gotserverhello=false;
				m_havework=false;
				m_metahashpos=0;
				lasttarget=0;
				(*m_nonce)=0;
				m_metahashstartnonce=0;
				besthash=~(uint256(0));
				besthashnonce=0;
				std::cout << "Connected to " << server << ":" << port << std::endl;
				SendClientHello(password,address);
			}
		}
		else
		{
			//if(m_gotserverhello==false)
			{
				Update();
				while(MessageReady() && !ProtocolError())
				{
					RemoteMinerMessage message;
					if(ReceiveMessage(message))
					{
						if(message.GetValue().type()==json_spirit::obj_type)
						{
							HandleMessage(message);
						}
						else
						{
							std::cout << "Unexpected json type sent by server.  Disconnecting." << std::endl;
							Disconnect();
						}
					}
				}
				if(ProtocolError())
				{
					std::cout << "Protocol error.  Disconnecting." << std::endl;
					Disconnect();
				}
			}

			if(m_havework)
			{

				for(int i=0; i<8; i++)
				{
					gpu.GetIn()->m_AH[i]=((unsigned int *)m_midbuffptr)[i];
				}
				gpu.GetIn()->m_merkle=((unsigned int *)m_blockbuffptr)[0];
				gpu.GetIn()->m_ntime=((unsigned int *)m_blockbuffptr)[1];
				gpu.GetIn()->m_nbits=((unsigned int *)m_blockbuffptr)[2];
				gpu.GetIn()->m_nonce=(*m_nonce);

				const unsigned int best=gpu.RunStep();

				// find hash solve / best hashes first
				for(int i=0; i<gpu.GetNumThreads()*gpu.GetNumBlocks(); i++)
				{
					memcpy(&hash,&(gpu.GetOut()[i].m_bestAH[0]),32);

					if(gpu.GetOut()[i].m_bestnonce!=0 && hash!=0 && hash<=m_currenttarget)
					{
						std::cout << "Found block!" << std::endl;

						// send hash found to server
						SendFoundHash(m_currentblockid,m_metahashstartblock,gpu.GetOut()[i].m_bestnonce);
					}
					// the best hash might be in the next metahash group - so we need to check for this, and for now ignore it
					if(gpu.GetOut()[i].m_bestnonce!=0 && hash!=0 && hash<=besthash && gpu.GetOut()[i].m_bestnonce<m_metahashstartnonce+m_metahash.size())
					{
						besthash=hash;
						besthashnonce=gpu.GetOut()[i].m_bestnonce;
					}

					for(int j=0; j<gpu.GetStepIterations(); j++)
					{
						(*m_nonce)++;
						m_metahash[m_metahashpos++]=gpu.GetMetaHash()[(i*gpu.GetStepIterations())+j];

						if(m_metahashpos>=m_metahash.size())
						{
							std::vector<unsigned char> digest(SHA256_DIGEST_LENGTH,0);
							SHA256(&m_metahash[0],m_metahash.size(),&digest[0]);

							// send the metahash to the server and request a new block if the nonce is approaching the limit
							SendMetaHash(m_currentblockid,m_metahashstartblock,m_metahashstartnonce,digest,besthash,besthashnonce);

							if((*m_nonce)>4000000000)
							{
								std::cout << "Requesting a new block" << std::endl;
								SendWorkRequest();
							}

							// reset the nonce to 0 if this is a new block
							if(m_nextblock!=m_metahashstartblock)
							{
								(*m_nonce)=0;
							}
							// set meta nonce first because memcpy will overwrite it
							m_metahashstartnonce=(*m_nonce);
							m_currentblockid=m_nextblockid;
							m_currenttarget=m_nexttarget;
							::memcpy(m_midbuffptr,&m_nextmidstate[0],32);
							::memcpy(m_blockbuffptr,&m_nextblock[0],64);
							m_metahashstartblock=m_nextblock;
							m_metahashpos=0;
							// set nonce again because it was overwritten by memcpy
							(*m_nonce)=m_metahashstartnonce;

							if(lasttarget!=m_currenttarget)
							{
								std::cout << "Target Hash : " << m_currenttarget.ToString() << std::endl;
								lasttarget=m_currenttarget;
							}

							std::cout << "Best : " << besthash.ToString() << std::endl;
							besthash=~(uint256(0));
							besthashnonce=0;
						}
					}

				}

				// increment metahash
				//for(int i=0; i<gpu.GetNumThreads()*gpu.GetNumBlocks()*gpu.GetStepIterations(); i++)
				{

				}

				/*
				// do 10000 hashes at a time
				for(unsigned int i=0; i<10000 && m_metahashpos<m_metahash.size(); i++)
				{
					SHA256Transform(&temphash,m_blockbuffptr,m_midbuffptr);
					SHA256Transform(&hash,&temphash,SHA256InitState);

					m_metahash[m_metahashpos++]=((unsigned char *)&hash)[0];

					if((((unsigned short*)&hash)[14]==0) && (((unsigned short*)&hash)[15]==0))
					{
						for (int i = 0; i < sizeof(hash)/4; i++)
						{
							((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);
						}
						
						if(hash<=m_currenttarget)
						{
							std::cout << "Found block!" << std::endl;

							// send hash found to server
							SendFoundHash(m_currentblockid,m_metahashstartblock,(*m_nonce));
						}

						if(hash<besthash)
						{
							besthash=hash;
							besthashnonce=(*m_nonce);
						}
					}
					// hash isn't bytereversed yet, but besthash already is
					else if(CryptoPP::ByteReverse(((unsigned int*)&hash)[7])<=((unsigned int *)&besthash)[7])
					{
						for (int i = 0; i < sizeof(hash)/4; i++)
						{
							((unsigned int*)&hash)[i] = CryptoPP::ByteReverse(((unsigned int*)&hash)[i]);
						}
						if(hash<besthash)
						{
							besthash=hash;
							besthashnonce=(*m_nonce);
						}
					}

					(*m_nonce)++;
				}

				if(m_metahashpos>=m_metahash.size())
				{

					std::vector<unsigned char> digest(SHA256_DIGEST_LENGTH,0);
					SHA256(&m_metahash[0],m_metahash.size(),&digest[0]);

					// send the metahash to the server and request a new block if the nonce is approaching the limit
					SendMetaHash(m_currentblockid,m_metahashstartblock,m_metahashstartnonce,digest,besthash,besthashnonce);
					if((*m_nonce)>4000000000)
					{
						std::cout << "Requesting a new block" << std::endl;
						SendWorkRequest();
					}

					// reset the nonce to 0 if this is a new block
					if(m_nextblock!=m_metahashstartblock)
					{
						(*m_nonce)=0;
					}
					// set meta nonce first because memcpy will overwrite it
					m_metahashstartnonce=(*m_nonce);
					m_currentblockid=m_nextblockid;
					m_currenttarget=m_nexttarget;
					::memcpy(m_midbuffptr,&m_nextmidstate[0],32);
					::memcpy(m_blockbuffptr,&m_nextblock[0],64);
					m_metahashstartblock=m_nextblock;
					m_metahashpos=0;
					// set nonce again because it was overwritten by memcpy
					(*m_nonce)=m_metahashstartnonce;

					if(lasttarget!=m_currenttarget)
					{
						std::cout << "Target Hash : " << m_currenttarget.ToString() << std::endl;
						lasttarget=m_currenttarget;
					}

					std::cout << "Best : " << besthash.ToString() << std::endl;
					besthash=~(uint256(0));
					besthashnonce=0;
				}
				*/
			}

		}
	}
}

#endif	// _BITCOIN_MINER_CUDA_
