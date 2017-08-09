#pragma once
#include <BaseLibrary\GMHelper\gmhelper_pub.h>
#include <string>
using namespace std;

class AgentConfig : public GMSingleTon<AgentConfig>
{
public:
	
	AgentConfig(void);
	bool parse();

	//tcp参数
	string m_local_ip;
	U32 m_tcp_cnn_num;
	U32 m_tcp_work_thr_num;
	U32 m_tcp_current_thr_num;
	U32 m_tcp_current_packet_num;
	U32 m_tcp_post_cp_thr_num;
	U32 m_tcp_notify_thr_num;
	
	//保活时间间隔
	U32 keep_alive_time_interval;
};

