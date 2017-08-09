#ifndef CC_USER_DEFINITIONS_H
#define CC_USER_DEFINITIONS_H

#include <XTools/XToolsPub.h>
#include "rtpproxy_ecode.h"
//本文件定义用户api上会用到的数据类型和定义

#define DEFAULT_TIMEOUT 5000

enum CALL_TYPE{
	CT_DIRECT=0,
	CT_TRANSFORM,
	CT_RTPDIRECT,
};


struct PortPair{
	bool operator==(const PortPair & right)const{
		 return rtp_recv_port==right.rtp_recv_port && rtp_sent_port==right.rtp_sent_port;
	}

	bool operator!=(const PortPair & right)const{
		return rtp_recv_port!=right.rtp_recv_port || rtp_sent_port!=right.rtp_sent_port;
	}
	int rtp_recv_port;//rtpproxy接收端口,rtcp端口等于rtp_recv_port+1
	int rtp_sent_port;//rtpproxy发送端口,rtcp端口等于rtp_send_port+1
};

struct CallInfo{
	const char* caller;			//主叫,len：16
	const char* caller_appkey;		
	const char* callee;			//被叫
	const char* callee_appkey;
	bool is_caller;				//是否主叫
	const char* call_id;			//呼叫id len:16
	CALL_TYPE call_type;			//呼叫类型,
	U32 session_h;				//len:16
	U32 session_l;
	GMAddrEx* report_addr;			//呼叫统计汇报地址
	GMAddrEx* dest_rtp_addr;		//对端rtp地址；中转直连方式才有该值
};

struct PathInfo{
	const char* path_info[3];		//建路信息，len:128
	int cid[3];
	int path_num;					//path_info有效值,0表示不用建路
};

struct ShortLinkInfo{
	GMAddrEx relay_addr[3];		//短链信息，len:128
	int cid[3];
	int relay_num;					//relay_addr有效值,0表示不用短链保活
};

#endif
