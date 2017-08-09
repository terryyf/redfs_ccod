#ifndef AGENT_CMD_H
#define AGENT_CMD_H

#include "../common/user_definitions.h"
#include <XTools/XToolsPub.h>
#include "rtpproxy_ecode.h"

#define RTPPROXY_VERSION 170727     
const U32 PURE_CMD_MAX_SIZE = 1200;//命令不含数据的最大长度
const U32 MAX_MEDIA_DATA_SIZE = 1500;//最大媒体数据长度
using namespace std;


#define CALLER_MAX_LEN 16+1
#define CALLER_APPKEY_MAX_LEN 64+1
#define CALL_ID_MAX_LEN 16+1
#define CALL_PATH_MAX_LEN 128+1
#define CALL_SESSION_MAX_LEN 16+1

class CallInfoInner{
public:
	CallInfoInner(){}
	void operator=(CallInfo& info){
		memset(this,0,sizeof(CallInfoInner));
		m_is_caller=info.is_caller;
		m_call_type=info.call_type;
		m_report_addr=*info.report_addr;
		memcpy(m_caller,info.caller,strlen(info.caller));
		memcpy(m_callee,info.callee,strlen(info.callee));
		memcpy(m_caller_appkey,info.caller_appkey,strlen(info.caller_appkey));
		memcpy(m_callee_appkey,info.callee_appkey,strlen(info.callee_appkey));
		memcpy(m_call_id,info.call_id,strlen(info.call_id));
		m_session_h=info.session_h;
		m_session_l=info.session_l;
		if(info.dest_rtp_addr){
			m_dest_rtp_addr = *info.dest_rtp_addr;
		}
	}

	void operator=(CallInfoInner& info)
		{
			memset(this,0,sizeof(CallInfoInner));
			memcpy(m_caller,info.m_caller,strlen(info.m_caller));
			memcpy(m_callee,info.m_callee,strlen(info.m_callee));
			memcpy(m_caller_appkey,info.m_caller_appkey,strlen(info.m_caller_appkey));
			memcpy(m_callee_appkey,info.m_callee_appkey,strlen(info.m_callee_appkey));
			memcpy(m_call_id,info.m_call_id,strlen(info.m_call_id));
			m_session_h=info.m_session_h;
			m_session_l=info.m_session_l;
			m_is_caller=info.m_is_caller;
			m_call_type=info.m_call_type;
			m_report_addr=info.m_report_addr;
			m_dest_rtp_addr=info.m_dest_rtp_addr;
	}

	XS_String<CALLER_MAX_LEN>			m_caller;			//主叫,len：16
	XS_String<CALLER_APPKEY_MAX_LEN>	m_caller_appkey;		
	XS_String<CALLER_MAX_LEN>			m_callee;			//被叫
	XS_String<CALLER_APPKEY_MAX_LEN>	m_callee_appkey;
	XS_String<CALL_ID_MAX_LEN>			m_call_id;			//呼叫id len:16
	U32									m_session_h;		//len:16
	U32									m_session_l;
	bool								m_is_caller;		//是否主叫
	CALL_TYPE							m_call_type;		//呼叫类型,
	GMAddrEx							m_report_addr;		//非话务汇报地址
	GMAddrEx							m_dest_rtp_addr;	//目的rtp地址，中转直连方式用
};

class PathInfoInner{
public:
	PathInfoInner(){}
	void operator=(PathInfoInner& info){
		memset(this,0,sizeof(PathInfoInner));
		m_path_num=info.m_path_num;
		for(int i=0;i<info.m_path_num;++i){
			memcpy(m_path_info[i],info.m_path_info[i],strlen(info.m_path_info[i]));
			m_cid[i]=info.m_cid[i];
		}
	}

	void operator=(PathInfo& info){
		memset(this,0,sizeof(PathInfoInner));
		m_path_num=info.path_num;
		for(int i=0;i<info.path_num;++i){
			memcpy(m_path_info[i],info.path_info[i],strlen(info.path_info[i]));
			m_cid[i]=info.cid[i];
		}
	}

	XS_String<CALL_PATH_MAX_LEN> m_path_info[3];		//建路信息，len:128
	int m_cid[3];
	int m_path_num;
};

class CmdKey
{
public:
	CmdKey(U32 cmdid,U32 passwd):m_cmd_id(cmdid),m_passwd(passwd){}
	CmdKey(){}
	bool operator<(const CmdKey& r)const
	{
		if (m_cmd_id != r.m_cmd_id)
			return m_cmd_id < r.m_cmd_id;

		return m_passwd < r.m_passwd;
	}

	U32 m_cmd_id;
	U32 m_passwd;
};

/*新加的命令只能加在最后，并且修改MAX_CMD_ID*/
enum RA_CMD_TYPE
{
    RA_UNKNOWN                 = 0,
	RA_GET_PORTS_REQ,
	RA_GET_PORTS_RESP,
    RA_START_CALL_REQ,
	RA_START_CALL_RESP,
	RA_FREE_PORTS_REQ,
	RA_FREE_PORTS_RESP,
	RA_STOP_CALL_REQ,
	RA_STOP_CALL_RESP,
	RA_BUILD_PATH_REQ,
	RA_BUILD_PATH_RESP,
	RA_ERROR_CALL_RESP,
	RA_SHAKE_HAND_REQ,
	RA_SHAKE_HAND_RESP,
	RA_SIPPROXY_OFFLINE_REQ,
	RA_SIPPROXY_OFFLINE_RESP,
	RA_KEEP_ALIVE_REQ,
	
};
/*新加的命令只能加在最后，并且修改MAX_CMD_ID,同时需要修改MAX_CMD_NUM的定义*/

const U32 MAX_CMD_ID = RA_KEEP_ALIVE_REQ;
const char* const CMD_NAME_ARRAY[MAX_CMD_ID+1]=
{
    "RA_UNKNOWN"                    ,

	"RA_GET_PORTS_REQ"				,
	"RA_GET_PORTS_RESP"				,
	"RA_START_CALL_REQ"				,
	"RA_START_CALL_RESP"			,
	"RA_FREE_PORTS_REQ"				,
	"RA_FREE_PORTS_RESP"			,
	"RA_STOP_CALL_REQ"				,
	"RA_STOP_CALL_RESP"				,
	"RA_BUILD_PATH_REQ"				,
	"RA_BUILD_PATH_RESP"			,
	"RA_ERROR_CALL_RESP"			,
	"RA_SIPPROXY_OFFLINE_REQ"	    ,
	"RA_SIPPROXY_OFFLINE_RESP"		,
	"RA_KEEP_ALIVE_REQ"				,
};

inline const char* cmd_name(RA_CMD_TYPE ct){
    return CMD_NAME_ARRAY[(U32)ct];
}

inline const char* cmd_name(U32 ct){
    return CMD_NAME_ARRAY[ct];
}

class CmdHead:public FromXMemPool<CmdHead>
{
public:
	CmdHead():m_body_len(0),m_cmd_type(RA_UNKNOWN),m_cmd_id(0),m_agent_total_queue_time(0),
		m_usr_req_time(0),m_send_req_time(0),m_send_resp_time(0),m_recv_req_time(0),m_got_cnn_times(0),
		m_try_get_cnn_times(0),m_agent_queue_times(0),m_version(RTPPROXY_VERSION){m_passwd=GMRandNum();}
	
	virtual ~CmdHead(){};

	static U32 genCmdID();

	U32 getMetaLen()//获取除内容外的长度。
	{
		char buf[1024]={0};
		XSpace xs(buf,sizeof(buf));
		ser(xs);
		return xs.used_size();
	}

	void set_body_len(XSpace& xs,U32 appendDataLen=0)
	{
		m_body_len = xs.used_size()+appendDataLen-SA_CMD_HEAD_LEN;
		xs.replace(m_body_len,0);
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		xs<<m_body_len<<m_cmd_type<<m_version<<m_cmd_id<<m_passwd<<m_send_resp_time<<m_recv_req_time;
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		xs>>m_body_len>>m_cmd_type>>m_version>>m_cmd_id>>m_passwd>>m_send_resp_time>>m_recv_req_time;
	}

	/*copy information from request and set receiving req time*/
	void constuct_resp_from_req(const CmdHead& req,RA_CMD_TYPE tp = RA_UNKNOWN)
	{
		m_passwd = req.m_passwd;
		m_version = req.m_version;
		m_cmd_id = req.m_cmd_id;
		m_recv_req_time = GMGetTickCount();
		if (tp != RA_UNKNOWN)
			m_cmd_type = tp;
	}

	static const U32 SA_CMD_HEAD_LEN = 7*sizeof(U32);

public:
	U32 m_body_len;					/*必须在最前面*/
	U32 m_cmd_type;			        /*命令类型*/
	U32 m_version;				    /*版本号,版本不对，服务器拒绝服务*/

	U32 m_cmd_id;
	U32 m_passwd;                   /*回应必须和请求的passwd配对*/
	
	U32 m_send_resp_time;          
	U32 m_recv_req_time;

	//不序列化，只是请求的上下文
	U32 m_usr_req_time;
	U32 m_send_req_time;           
	U32 m_agent_total_queue_time;   /*agent等待队列排队总时间*/
	U8 m_got_cnn_times;             /*获取到连接次数*/
	U8 m_try_get_cnn_times;         /*尝试获取连接次数*/
	U8 m_agent_queue_times;         /*agent等待队列排队次数*/
};

class GetPortsReq:public CmdHead{
public:
	GetPortsReq(){m_cmd_type=RA_GET_PORTS_REQ;}
	GetPortsReq(int user_id,GMAddrEx* rtp_addr):m_user_id(user_id){
		m_cmd_type=RA_GET_PORTS_REQ;
		if(rtp_addr){
			m_rtp_addr = *rtp_addr;
		}
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_user_id<<m_sipproxy_id<<m_sipproxy_fake_id;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_user_id>>m_sipproxy_id>>m_sipproxy_fake_id;
	}
	int m_user_id;
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	
	//不参与序列化
	GMAddrEx m_rtp_addr;
};
class GetPortsResp :public CmdHead
{
public:
	GetPortsResp(){
		m_cmd_type = RA_GET_PORTS_RESP;
	}
	GetPortsResp(RA_ECODE ec ,int user_id,PortPair& portpair)
		:m_ec(ec),m_user_id(user_id),m_portpair(portpair){
		m_cmd_type=RA_GET_PORTS_RESP;
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec<<m_user_id<<m_portpair;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec>>m_user_id>>m_portpair;
	}

	RA_ECODE m_ec;
	int m_user_id;
	PortPair m_portpair;
};

class FreePortsReq:public CmdHead
{
public:
	FreePortsReq(){m_cmd_type=RA_FREE_PORTS_REQ;}
	FreePortsReq(int user_id,PortPair portpair,GMAddrEx* rtp_addr)
		:m_user_id(user_id),m_portpair(portpair){
		m_cmd_type=RA_FREE_PORTS_REQ;
		if(rtp_addr){
			m_rtp_addr = *rtp_addr;
		}
	}
	
	virtual void ser(XSpace& xs) throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_user_id<<m_portpair<<m_sipproxy_id<<m_sipproxy_fake_id;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs) throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_user_id>>m_portpair>>m_sipproxy_id>>m_sipproxy_fake_id;
	}
	
	int m_user_id;
	PortPair m_portpair;
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	
	//不参与序列化
	GMAddrEx m_rtp_addr;
};
class FreePortResp:public CmdHead
{
public:
	FreePortResp(){
		m_cmd_type = RA_FREE_PORTS_RESP;
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec;
	}

	RA_ECODE m_ec;	
};

class SipproxyOfflineReq:public CmdHead
{
public:
	SipproxyOfflineReq(){
		m_cmd_type = RA_SIPPROXY_OFFLINE_REQ;
	}
virtual void ser(XSpace& xs)
        throw (XS_NoSpaceException)
    {
        CmdHead::ser(xs);
        xs<<m_sipproxy_id<<m_sipproxy_fake_id;
        set_body_len(xs);
    }

    virtual void unser(XSpace& xs)
        throw (XS_NoSpaceException,XS_BadSapceException)
    {
        CmdHead::unser(xs);
        xs>>m_sipproxy_id>>m_sipproxy_fake_id;
    }

	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	//不参与序列化
	GMAddrEx m_rtp_addr;
};

class SipproxyOfflineResp:public CmdHead
{
public:
	SipproxyOfflineResp(){
		m_cmd_type = RA_SIPPROXY_OFFLINE_RESP;
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec;
	}

	RA_ECODE m_ec;	
};

class KeepConnetionAlive:public CmdHead
{
public:
    KeepConnetionAlive(){
        m_cmd_type = RA_KEEP_ALIVE_REQ;
    }
    virtual void ser(XSpace& xs)
        throw (XS_NoSpaceException)
    {
        CmdHead::ser(xs);
        xs<<m_sipproxy_id<<m_sipproxy_fake_id;
        set_body_len(xs);
    }

    virtual void unser(XSpace& xs)
        throw (XS_NoSpaceException,XS_BadSapceException)
    {
        CmdHead::unser(xs);
        xs>>m_sipproxy_id>>m_sipproxy_fake_id;
    }
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
};


inline XSpace& operator<<(XSpace& xs, CallInfoInner& v)
	throw (XS_NoSpaceException)
{
	xs << v.m_is_caller << v.m_call_type<<v.m_caller<<v.m_caller_appkey<<v.m_callee
	   <<v.m_callee_appkey<<v.m_call_id<<v.m_session_h<<v.m_session_l<<v.m_report_addr
	   <<v.m_dest_rtp_addr;
	
	return xs;
}

inline XSpace& operator>>(XSpace& xs, CallInfoInner& v)
	throw (XS_NoSpaceException)
{
	xs >> v.m_is_caller >> v.m_call_type>>v.m_caller>>v.m_caller_appkey>>v.m_callee
	   >>v.m_callee_appkey>>v.m_call_id>>v.m_session_h>>v.m_session_l>>v.m_report_addr
	   >>v.m_dest_rtp_addr;

	return xs;
}

//pathinfo
inline XSpace& operator<<(XSpace& xs, PathInfoInner& v)
	throw (XS_NoSpaceException)
{
	xs << v.m_path_num;
	for(int i=0;i<v.m_path_num;++i){
		xs<<v.m_path_info[i]<<v.m_cid[i];
	}

	return xs;
}

inline XSpace& operator>>(XSpace& xs, PathInfoInner& v)
	throw (XS_NoSpaceException)
{
	xs >> v.m_path_num;
	for(int i=0;i<v.m_path_num;++i){
		xs>>v.m_path_info[i]>>v.m_cid[i];
	}
	return xs;
}

//ShortLinkInfo
inline XSpace& operator<<(XSpace& xs, ShortLinkInfo& v)
	throw (XS_NoSpaceException)
{
	xs << v.relay_num;
	for(int i=0;i<v.relay_num;++i){
		xs<<v.relay_addr[i]<<v.cid[i];
	}

	return xs;
}

inline XSpace& operator>>(XSpace& xs, ShortLinkInfo& v)
	throw (XS_NoSpaceException)
{
	xs >> v.relay_num;
	for(int i=0;i<v.relay_num;++i){
		xs>>v.relay_addr[i]>>v.cid[i];
	}

	return xs;
}

class StartCallReq:public CmdHead
{
public:
	StartCallReq(){m_cmd_type=RA_START_CALL_REQ;}
	StartCallReq(U64 sipproxy_id,U64 sipproxy_fake_id,int user_id, PortPair portpair,
				 GMAddrEx* rtp_addr)
	:m_user_id(user_id),m_portpair(portpair),m_sipproxy_id(sipproxy_id),
	 m_sipproxy_fake_id(sipproxy_fake_id){
		m_cmd_type=RA_START_CALL_REQ;
		if(rtp_addr){
			m_rtp_addr = *rtp_addr;
		}
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_sipproxy_id<<m_sipproxy_fake_id<<m_user_id<<m_portpair<<m_call_info<<m_path_info<<
			m_short_link_info;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_sipproxy_id>>m_sipproxy_fake_id>>m_user_id>>m_portpair>>m_call_info>>m_path_info>>
			m_short_link_info;
	}

	int m_user_id;
	PortPair m_portpair;
	CallInfoInner m_call_info;
	PathInfoInner m_path_info;
	ShortLinkInfo m_short_link_info;
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	
	//不参与序列化
	GMAddrEx m_rtp_addr;
};

class StartCallResp:public CmdHead{
public:
	StartCallResp():m_ec(RA_FAIL),m_path_suc(false){m_cmd_type = RA_START_CALL_RESP;}
	StartCallResp(RA_ECODE ec, bool path_suc):m_ec(ec),m_path_suc(path_suc){
		m_cmd_type = RA_START_CALL_RESP;
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec<<m_path_suc;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec>>m_path_suc;
	}

	RA_ECODE m_ec;
	bool m_path_suc;
};

class BuildPathReq:public CmdHead
{
public:
	BuildPathReq(){m_cmd_type = RA_BUILD_PATH_REQ;}
	BuildPathReq(U64 sipproxy_id, U64 sipproxy_fake_id, int user_id, PortPair ports,
				 GMAddrEx* rtp_addr)
	:m_sipproxy_id(sipproxy_id),m_sipproxy_fake_id(sipproxy_fake_id),m_user_id(user_id),
	 m_ports(ports)
	{
		if(rtp_addr){
			m_rtp_addr=*rtp_addr;
		}
		m_cmd_type = RA_BUILD_PATH_REQ;
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_sipproxy_id<<m_sipproxy_fake_id<<m_user_id<<m_ports<<m_call_info<<m_path_info;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_sipproxy_id>>m_sipproxy_fake_id>>m_user_id>>m_ports>>m_call_info>>m_path_info;
	}


	int m_user_id;
	CallInfoInner m_call_info;
	PathInfoInner m_path_info;
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	PortPair m_ports;

	//不参与序列化
	GMAddrEx m_rtp_addr;
};

class BuildPathResp:public CmdHead{
public:
	BuildPathResp():m_ec(RA_FAIL),m_path_suc(false){m_cmd_type = RA_BUILD_PATH_RESP;}
	BuildPathResp(RA_ECODE ec, bool path_suc):m_ec(ec),m_path_suc(path_suc){
		m_cmd_type = RA_BUILD_PATH_RESP;
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec<<m_path_suc;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec>>m_path_suc;
	}

	RA_ECODE m_ec;
	bool m_path_suc;
};

class StopCallReq:public CmdHead
{
public:
	StopCallReq(){m_cmd_type = RA_STOP_CALL_REQ;}
	StopCallReq(U64 sipproxy_id,U64 sipproxy_fake_id,int user_id,  PortPair ports,GMAddrEx* rtp_addr)
	:m_sipproxy_id(sipproxy_id),m_sipproxy_fake_id(sipproxy_fake_id),m_ports(ports)
	{
		m_cmd_type = RA_STOP_CALL_REQ;
		if(rtp_addr){
			m_rtp_addr=*rtp_addr;
		}
	}
	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_sipproxy_id<<m_sipproxy_fake_id<<m_user_id<<m_ports<<m_call_info;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_sipproxy_id>>m_sipproxy_fake_id>>m_user_id>>m_ports>>m_call_info;
	}

	int m_user_id;
	PortPair m_ports;
	CallInfoInner m_call_info;
	U64 m_sipproxy_id;
	U64 m_sipproxy_fake_id;
	
	//不参与序列化
	GMAddrEx m_rtp_addr;
};

class StopCallResp:public CmdHead{
public:
	StopCallResp():m_ec(RA_SUC){
		m_cmd_type = RA_STOP_CALL_RESP;
	}
	StopCallResp(RA_ECODE ec):m_ec(ec){
		m_cmd_type = RA_STOP_CALL_RESP;
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_ec;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_ec;
	}

	RA_ECODE m_ec;
};

class ErrorCallResp:public CmdHead{
public:
	ErrorCallResp(){
		m_cmd_type = RA_ERROR_CALL_RESP;
	}

	ErrorCallResp(GMAddrEx* rtp_addr,const char* call_id):m_rtp_addr(*rtp_addr){
		m_cmd_type = RA_ERROR_CALL_RESP;
		memcpy(m_call_id,call_id,strlen(call_id));
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_rtp_addr<<m_call_id;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_rtp_addr>>m_call_id;
	}

	GMAddrEx m_rtp_addr;
	XS_String<CALL_ID_MAX_LEN> m_call_id;			//呼叫id len:16
};

class ShakeHandReq:public CmdHead
{
public:
	ShakeHandReq(){
		m_cmd_type = RA_SHAKE_HAND_REQ;
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_key_addr<<m_rudp_addr;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_key_addr>>m_rudp_addr;
	}

public:
	GMAddrEx m_key_addr;
	GMAddrEx m_rudp_addr;
};


class ShakeHandResp:public CmdHead
{
public:
	ShakeHandResp(){
		m_cmd_type = RA_SHAKE_HAND_RESP;
	}

	virtual void ser(XSpace& xs)
		throw (XS_NoSpaceException)
	{
		CmdHead::ser(xs);
		xs<<m_key_addr<<m_rudp_addr<<m_remoteAddrs;
		set_body_len(xs);
	}

	virtual void unser(XSpace& xs)
		throw (XS_NoSpaceException,XS_BadSapceException)
	{
		CmdHead::unser(xs);
		xs>>m_key_addr>>m_rudp_addr>>m_remoteAddrs;
	}

public:
	GMAddrEx m_key_addr;
	GMAddrEx m_rudp_addr;

	XList<GMAddrEx> m_remoteAddrs;
};

#endif
