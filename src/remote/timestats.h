#ifndef _timestats_
#define _timestats_

#include "remotebitcoinheaders.h"
#include <map>
#include <string>
#include <fstream>

class TimeStats
{
public:
	TimeStats(const std::string &filename, const int64 writedelay):m_lastwritemillis(GetTimeMillis()),
	m_filename(filename),m_writedelay(writedelay)
	{
		
	}
	
	~TimeStats()
	{
		WriteStats(); 
	}
	
	void Add(const std::string &section, const int64 callcount, const int64 usec)
	{
		m_stats[section].first+=callcount;
		m_stats[section].second+=usec;
		if((m_lastwritemillis+m_writedelay)<GetTimeMillis())
		{
			WriteStats();
			m_lastwritemillis=GetTimeMillis();
		}
	}
	
	void ClearStats()			{ m_stats.clear(); }
	
	void WriteStats()
	{
		std::ofstream outfile(m_filename.c_str(),std::ios::out | std::ios::trunc);
		if(outfile.is_open())
		{
			outfile << "section\tcount\ttime" << std::endl;
			for(std::map<std::string,std::pair<int64,int64> >::const_iterator i=m_stats.begin(); i!=m_stats.end(); i++)
			{
				outfile << (*i).first << "\t" << (*i).second.first << "\t" << (*i).second.second << std::endl;	
			}
			outfile.close();
		}
	}

private:
	std::string m_filename;
	int64 m_writedelay;
	int64 m_lastwritemillis;
	std::map<std::string,std::pair<int64,int64> > m_stats;
};

class ScopedTimer
{
public:
	ScopedTimer(TimeStats &ts,const std::string &section):m_ts(ts),m_section(section)	{ m_startmicros=boost::posix_time::microsec_clock::local_time(); }
	~ScopedTimer()																		{ m_ts.Add(m_section,1,(boost::posix_time::microsec_clock::local_time()-m_startmicros).total_microseconds()); }

private:

	TimeStats &m_ts;
	boost::posix_time::ptime m_startmicros;
	std::string m_section;

};

#endif	// _timestats_
