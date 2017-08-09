#include "StdAfx.h"
#include "RtpproxyClientTester.h"

const char* call_id = "test_call_id";
const char* caller = "test_caller";
const char* callee = "test_callee";
const char* caller_app_key = "test_caller_app_key";
const char* callee_app_key = "test_callee_app_key";
const char* path_info_0 = "test_path_info_0";
const char* path_info_1 = "test_path_info_1";
const char* path_info_2 = "test_path_info_2";

RtpproxyClientTester::RtpproxyClientTester(void):m_event(true),m_rtp_addr("192.168.0.108",4188),
	m_user_id(1001),m_report_addr("192.168.0.108",4189),m_dest_rtp_addr("192.168.0.108",4187)
{
	m_call_info.call_id = call_id;
	m_call_info.call_type = CT_DIRECT;
	m_call_info.caller = caller;
	m_call_info.callee = callee;
	m_call_info.caller_appkey = caller_app_key;
	m_call_info.callee_appkey = callee_app_key;
	m_call_info.is_caller = true;
	m_call_info.session_h = 2017;
	m_call_info.session_l =808;
	m_call_info.report_addr = &m_report_addr;
	m_call_info.dest_rtp_addr = &m_dest_rtp_addr;
	for(int i=0;i<3;i++){
		m_path_info.cid[i]=i*2+1;
		m_short_link.cid[i]=i*2+1;
	}
	m_path_info.path_info[0]=path_info_0;
	m_path_info.path_info[1]=path_info_1;
	m_path_info.path_info[2]=path_info_2;
	m_path_info.path_num = 3;
	m_short_link.relay_num = 3;

	m_short_link.relay_addr[0]=GMAddrEx("127.0.0.1",1234);
	m_short_link.relay_addr[1]=GMAddrEx("127.0.0.1",1235);
	m_short_link.relay_addr[2]=GMAddrEx("127.0.0.1",1236);
}


RtpproxyClientTester::~RtpproxyClientTester(void)
{
}
