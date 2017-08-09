#include "stdafx.h"
#include <list>
#include <iostream>
#include "agent_config.h"
using namespace std;


AgentConfig::AgentConfig(void)
{
	//TCP
	m_tcp_work_thr_num = 0;
	m_tcp_current_thr_num = 0;
	m_tcp_current_packet_num = 1;
	m_tcp_post_cp_thr_num = 0;
	m_tcp_notify_thr_num = 0;
	m_tcp_cnn_num = 0;
}

bool AgentConfig::parse()
{
	GMMarkupSTL xml;

	list<string> config_list;

	string filename;
	if(GMFileSystemUtility::getModulePath(filename))
		filename.append(PATHSPLIT_STRING);
	filename.append("rtpproxy_client.cfg");

	config_list.push_back(filename);

	char msg[1024]={0};
	bool load_suc = false;
	for(list<string>::iterator it = config_list.begin();it!=config_list.end();++it){
		if(!xml.Load(it->c_str())){ 
			strcat(msg,cz("try to load %s failed\n",it->c_str()));
		}else{
			load_suc = true;
			LGW("load %s sucessfully",it->c_str());
			break;
		}
	}

	if (!load_suc){
		LGE(msg);
		return false;
	}

	//到root elem
	xml.ResetPos();
	if(!xml.FindElem()){
		Log::writeError(LOG_SA,1,"AgentConfig::parse->fail to FindElem root");
		return false;
	}

	string tmp;
	tmp.reserve(100);
	tmp = xml.GetAttrib("VERSION");
	if (tmp != "1.0"){
		printf("AgentConfig::parse->the version of rtpproxy.cfg does not match. need version:1.0 but this is version:%s",tmp.c_str());
		Log::writeError(LOG_SA,1,"AgentConfig::parse->the version of rtpproxy.cfg does not match. need version:1.0 but this is version:%s",tmp.c_str());
		return false;
	}

	//进入RtpProxyClient elem
	if(!xml.IntoElem()){
		Log::writeError(LOG_SA,1,"AgentConfig::parse->fail to IntoElem root");
		return false;
	}

	if(xml.FindElem("TCP")){
		tmp = xml.GetAttrib("SVR_IP");
		if (0 == tmp.compare(""))
			return false;
		m_local_ip = tmp;

		tmp = xml.GetAttrib("CONNECTION_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_cnn_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("WORK_THREAD_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_work_thr_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_THREAD_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_current_thr_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_PACKET_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_current_packet_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("POST_CP_THREAD_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_post_cp_thr_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("NOTIFY_THREAD_NUM");
		if (0 == tmp.compare(""))
			return false;
		m_tcp_notify_thr_num = atoi(tmp.c_str());
	}

	if(xml.FindElem("KeepAlive")){
		tmp = xml.GetAttrib("keepAliveTimeInterval");
		if (0 == tmp.compare(""))
			return false;
		keep_alive_time_interval = atoi(tmp.c_str());
	}
	return true;
}
