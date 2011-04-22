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

#include "httprequest.h"
#include <curl/curl.h>
#include <sstream>
#include <cstring>

HTTPRequest::HTTPRequest(const std::string &url):m_url(url)
{
	
}

HTTPRequest::~HTTPRequest()
{

}

const bool HTTPRequest::DoRequest(std::string &result)
{
	m_writebuff.clear();
	//m_readbuff.assign(data.begin(),data.end());
	m_readbuff.clear();

	CURL *curl=0;
	struct curl_slist *headers=0;
	int rval;

	curl=curl_easy_init();

	curl_easy_setopt(curl,CURLOPT_URL,m_url.c_str());
	curl_easy_setopt(curl,CURLOPT_ENCODING,"");
	curl_easy_setopt(curl,CURLOPT_FAILONERROR,1);
	curl_easy_setopt(curl,CURLOPT_TCP_NODELAY,1);
	curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,HTTPRequest::WriteData);
	curl_easy_setopt(curl,CURLOPT_WRITEDATA,this);
	curl_easy_setopt(curl,CURLOPT_READFUNCTION,HTTPRequest::ReadData);
	curl_easy_setopt(curl,CURLOPT_READDATA,this);
	/*
	if(m_user!="" && m_password!="")
	{
		std::string userpass(m_user+":"+m_password);
		curl_easy_setopt(curl,CURLOPT_USERPWD,userpass.c_str());
		curl_easy_setopt(curl,CURLOPT_HTTPAUTH,CURLAUTH_BASIC);
	}
	curl_easy_setopt(curl,CURLOPT_POST,1);
	*/

	/*
	std::ostringstream istr;
	istr << m_readbuff.size();
	std::string headersize("Content-Length: "+istr.str());
	*/

	//headers=curl_slist_append(headers,"Content-type: application/json");
	//headers=curl_slist_append(headers,headersize.c_str());
	headers=curl_slist_append(headers,"Expect:");

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	rval=curl_easy_perform(curl);
	
	if(m_writebuff.size()>0)
	{
		result.assign(m_writebuff.begin(),m_writebuff.end());
	}
	else
	{
		result="";
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return (rval==0);
}

size_t HTTPRequest::ReadData(void *ptr, size_t size, size_t nmemb, void *user_data)
{
	size_t readlen=size*nmemb;
	HTTPRequest *req=(HTTPRequest *)user_data;

	readlen=(std::min)(req->m_readbuff.size(),readlen);

	if(readlen>0)
	{
		::memcpy(ptr,&req->m_readbuff[0],readlen);

		req->m_readbuff.erase(req->m_readbuff.begin(),req->m_readbuff.begin()+readlen);
	}

	return readlen;
}

size_t HTTPRequest::WriteData(void *ptr, size_t size, size_t nmemb, void *user_data)
{
	size_t writelen=size*nmemb;
	HTTPRequest *req=(HTTPRequest *)user_data;

	if(writelen>0)
	{
		std::vector<char>::size_type startpos=req->m_writebuff.size();
		req->m_writebuff.resize(req->m_writebuff.size()+writelen);
		::memcpy(&req->m_writebuff[startpos],ptr,writelen);
	}

	return writelen;
}
