#ifndef _RTP_PROXY_CONFIG_H_
#define _RTP_PROXY_CONFIG_H_
#include <BaseLibrary\GMHelper\gmhelper_pub.h>
#include <string>
using namespace std;

class RtpProxyConfig : public GMSingleTon<RtpProxyConfig>
{
public:
	bool parse();
	RtpProxyConfig(void);

	//系统特性参数
	U32 m_precoess_deadtime;
	U32 m_dead_check_interval;
	U32 m_error_call_check_interval;
	U32 m_report_time;

	//tcp参数
	U32 m_tcp_work_thread_num;
	U32 m_tcp_concurrent_thread_num;
	U32 m_tcp_concurrent_packet_num;
	U32 m_tcp_post_cp_thread_num;
	U32 m_tcp_notify_thread_num;
	std::list<GMAddrEx> m_svr_ips;
	string m_tcp_svr_ip;
	U32 m_tcp_svr_port;
	
	//udp参数
	U32 m_udp_min_port;
	U32 m_udp_max_port;
	U32 m_udp_work_thread_num;
	U32 m_udp_concurrent_thread_num;
	U32 m_udp_concurrent_packet_num;

	//RelayClient
	string m_relay_svr_ip;
	U32 m_relay_svr_port;

	//RouterCenter
	U32 m_router_id;
	U32 m_router_type;
	string m_router_svr_ip;
	U32 m_router_svr_port;
	string m_router_center_ip;
	U32 m_router_center_port;
	string m_router_center_back_ip;
	U32 m_router_center_back_port;

	//ReportCenter
	string m_report_center_ip;
	U32 m_report_center_port;
	
	//报警参数
	U32 m_max_cpu;
	U32 m_max_system_mem;
};
#endif //_RTP_PROXY_CONFIG_H_