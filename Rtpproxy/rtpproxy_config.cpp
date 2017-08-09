#include "stdafx.h"
#include <list>
#include <iostream>
#include "rtpproxy_config.h"
using namespace std;

RtpProxyConfig::RtpProxyConfig()
{
	//TODO default param

	//TCP
	m_tcp_work_thread_num = 0;
	m_tcp_concurrent_thread_num = 0;
	m_tcp_concurrent_packet_num = 1;
	m_tcp_post_cp_thread_num = 0;
	m_tcp_notify_thread_num = 0;

	//UDP
	m_udp_min_port = 6000;
	m_udp_max_port = 65500;
	m_udp_work_thread_num = 0;
	m_udp_concurrent_thread_num = 0;
	m_udp_concurrent_packet_num = 1;

	//报警
	m_max_cpu = 90;
	m_max_system_mem = 90;
}

bool RtpProxyConfig::parse()
{
	GMMarkupSTL xml;

	list<string> config_list;

    string filename;
    if(GMFileSystemUtility::getModulePath(filename))
        filename.append(PATHSPLIT_STRING);
    filename.append("rtpproxy.cfg");

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

	//到rootelem
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

	//进入rtpproxy elem
	if(!xml.IntoElem()){
        Log::writeError(LOG_SA,1,"AgentConfig::parse->fail to IntoElem root");
        return false;
    }

	if(xml.FindElem("Trait")){
		tmp = xml.GetAttrib("PRECOESS_DEAD_CHECK_INTERVAL");
		if(tmp==""){
			return false;
		}
		m_dead_check_interval = atoi(tmp.c_str());

        tmp = xml.GetAttrib("PRECOESS_DEADTIME");
        if (0 == tmp.compare(""))
            return false;
		m_precoess_deadtime = atoi(tmp.c_str());

        tmp = xml.GetAttrib("REPORT_TIME");
        if (0 == tmp.compare(""))
			return false;
		m_report_time = atoi(tmp.c_str());
		tmp = xml.GetAttrib("ERROR_CALL_CHECK_INTERVAL");
		if(tmp==""){
			return false;
		}
		m_error_call_check_interval = atoi(tmp.c_str());
	}

	if(!xml.FindElem("RemoteConnection"))
		return false;

	//进入RemoteConnection
	if(!xml.IntoElem())
		return false;

	if(xml.FindElem("TCP")){
		tmp = xml.GetAttrib("WORK_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_tcp_work_thread_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_tcp_concurrent_thread_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_PACKET_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_tcp_concurrent_packet_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("POST_CP_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_tcp_post_cp_thread_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("NOTIFY_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_tcp_notify_thread_num = atoi(tmp.c_str());
	}

	//进入TCP
	if(!xml.IntoElem())
		return false;

	if(xml.FindElem("ListenAddr")){
		 string ip;
		while(xml.FindChildElem("ITEM"))
		{
            ip = xml.GetChildAttrib("SVR_IP"); 			
			U16 port ;
            tmp = xml.GetChildAttrib("SVR_PORT");
			if (0 == tmp.compare(""))
				return false;
			port = atoi(tmp.c_str());
			if(ip!=""){
				GMAddrEx addr;
				addr.m_ip = GMConvertIP_a2n((char*)ip.c_str());
				addr.m_port = port;
				m_svr_ips.push_back(addr);
			}
        }
	}

	//出TCP
	xml.OutOfElem(); 

	if(xml.FindElem("UDP")){
		tmp = xml.GetAttrib("MIN_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_udp_min_port = atoi(tmp.c_str());

		tmp = xml.GetAttrib("MAX_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_udp_max_port = atoi(tmp.c_str());

		tmp = xml.GetAttrib("WORK_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_udp_work_thread_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_THREAD_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_udp_concurrent_thread_num = atoi(tmp.c_str());

		tmp = xml.GetAttrib("CONCURRENT_PACKET_NUM");
        if (0 == tmp.compare(""))
			return false;
		m_udp_concurrent_packet_num = atoi(tmp.c_str());
	}

	//进入UDP
	if(!xml.IntoElem())
		return false;

	if(xml.FindElem("RelayClient")){
		tmp = xml.GetAttrib("SVR_IP");
        if (0 == tmp.compare(""))
			return false;
		m_relay_svr_ip = tmp;

		tmp = xml.GetAttrib("SVR_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_relay_svr_port = atoi(tmp.c_str());
	}

	if(xml.FindElem("RouterClient")){
		tmp = xml.GetAttrib("ROUTER_ID");
        if (0 == tmp.compare(""))
			return false;
		m_router_id = atoi(tmp.c_str());

		tmp = xml.GetAttrib("ROUTER_TYPE");
        if (0 == tmp.compare(""))
			return false;
		m_router_type = atoi(tmp.c_str());

		tmp = xml.GetAttrib("SVR_IP");
        if (0 == tmp.compare(""))
			return false;
		m_router_svr_ip = tmp;

		tmp = xml.GetAttrib("SVR_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_router_svr_port = atoi(tmp.c_str());

		tmp = xml.GetAttrib("ROUTER_CENTER_IP");
        if (0 == tmp.compare(""))
			return false;
		m_router_center_ip = tmp;

		tmp = xml.GetAttrib("ROUTER_CENTER_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_router_center_port = atoi(tmp.c_str());

		tmp = xml.GetAttrib("ROUTER_CENTER_BACK_IP");
        if (0 == tmp.compare(""))
			return false;
		m_router_center_back_ip = tmp;

		tmp = xml.GetAttrib("ROUTER_CENTER_BACK_PORT");
        if (0 == tmp.compare(""))
			return false;
		m_router_center_back_port = atoi(tmp.c_str());
	}

	if(xml.FindElem("ReportCenter")){
		tmp = xml.GetAttrib("SVR_IP");
        if (0 == tmp.compare(""))
			return false;
		m_report_center_ip = tmp;

		tmp = xml.GetAttrib("PORT");
        if (0 == tmp.compare(""))
			return false;
		m_report_center_port = atoi(tmp.c_str());
	}

	//出UDP
	xml.OutOfElem();
	//出RemoteConnection
	xml.OutOfElem();

	if(xml.FindElem("FaultTolerant")){
		tmp = xml.GetAttrib("MAX_CPU");
        if (0 == tmp.compare(""))
			return false;
		m_max_cpu = atoi(tmp.c_str());

		tmp = xml.GetAttrib("MAX_SYSTEM_MEM");
        if (0 == tmp.compare(""))
			return false;
		m_max_system_mem = atoi(tmp.c_str());
	}

	return true;
}