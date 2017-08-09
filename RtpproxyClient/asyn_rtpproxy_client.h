#ifndef RTPPROXY_ASYN_CLIENT_H__
#define RTPPROXY_ASYN_CLIENT_H__

#include "cnn_mixer.h"
#include "agent_config.h"
#include "channel.h"
#include "rtpproxy_cmd.h"
#include "rtpproxy_asyn_api.h"
class RtpproxyAsynClient : public IAsynRtpProxyClient, public CnnMixer,public GMSingleTon<RtpproxyAsynClient>{
	class InnerNotifyTask:public ITask//进程内部通知任务，用于同步接口的返回
	{
	public:
		InnerNotifyTask(RA_ECODE ec, CmdHead* req, U64 ctx, void* cb)
			: m_ec(ec),m_req(req),m_ctx(ctx),m_cb(cb){}

		  void excute();

		  RA_ECODE m_ec;
		  void* m_cb;
		  CmdHead* m_req;
		  U64 m_ctx;
	};
	friend class ConGroup;
	friend class Channel;
	friend class ChannelManager;

public:
	RtpproxyAsynClient();

	bool init(U64 sipproxy_id,ErrorCallCB error_call_cb,ErrorRtpProxysCB error_rtpproxy_cb);

	void uninit();

	void asyn_get_ports(int user_id, GMAddrEx* rtp_addr,IRAGetPortCB* cb,U32 timeout=DEFAULT_TIMEOUT,
						U64 ctx=0);

	void asyn_free_ports(int user_id, GMAddrEx* rtp_addr, PortPair ports, IRAFreePortsCB* cb,
						 U32 timeout =DEFAULT_TIMEOUT , U64 ctx =0 );

	void asyn_start_call(int user_id, PortPair ports,CallInfo* call_info, PathInfo* path_info,
						 ShortLinkInfo* short_link_info, GMAddrEx* rtp_addr, IRAStartCallCB* cb,
						 U32 timeout =DEFAULT_TIMEOUT , U64 ctx =0 );

	void asyn_stop_call(int user_id, PortPair ports, CallInfo* call_info,GMAddrEx* rtp_addr,
						IRAStopCallCB* cb, U32 timeout =DEFAULT_TIMEOUT , U64 ctx =0 );

	void asyn_build_path(int user_id,PortPair ports,CallInfo* call_info, PathInfo* path_info,
						 GMAddrEx* rtp_addr,IRABuildPathCB* cb, U32 timeout =DEFAULT_TIMEOUT ,
						 U64 ctx =0  );

	virtual void asyn_sipproxy_off_line(GMAddrEx* rtp_addr, IRASipproxyOffLineCB* cb,
										U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0);

	bool init_remote_channel(const GMAddrEx& key_remote_addr);
	
    void write_cnn(const SACnnID& conid,SendTask* st);
private:
	//会删除req
	void notify_by_req(RA_ECODE nErr, CmdHead* req, U64 ctx,void* cb);

	//会删除st,和st->m_cmd
	void notify_by_task(RA_ECODE nErr,SendTask* st);

	virtual void on_accept_connection(const SACnnID& id){};

	void on_read_connection(CnnMixerError cme,const SACnnID& cid,const GMAddrEx& remote_addr, char* buf, U32 willReadLen, 
        U32 actualReadLen);

	void on_write_connection(CnnMixerError cme,const SACnnID& cid, GMBUF* buf_array, U32 array_num, 
        U32 actuallen, U64 ctx1, U64 ctx2);

	void dotask(const GMAddrEx*raddr,CmdHead* cmd,U32 timeout,U64 ctx,void* cb);	
	
	//当收到回应后，请求已经超时返回了，此函数做相应的清理工作
	void clear_timeout_task(SACnnID cid,RA_CMD_TYPE ct,char* buf,U32 buflen);

	void asyn_notice_error_task(RA_ECODE nErr, CmdHead* req, U64 ctx,void* cb);

public:
	bool m_runing;
private:
	U64 m_sipproxy_id; 
	U64 m_sipporxy_fake_id;
	ErrorCallCB m_error_call_cb;
	ErrorRtpProxysCB m_error_rtpproxy_cb;
	ConnType m_remote_conncet_type;
	U32 m_remote_cnn_num;
	GMAddrEx m_local_key_addr;
	ChannelManager m_channel_manager;
	TaskDealer m_swich_thread_noticer;

	//当一定时间内m_error_num次数超过一定值，认为rtpproxy异常
	U64 m_begin_error_time;		//发送失败的开始时间
	volatile U32 m_error_num;			//发送失败次数
};

inline void RtpproxyAsynClient::write_cnn(const SACnnID& conid,SendTask* st)
{
	GMBUF data(st->m_send_buf,st->m_send_len);
	asynWriteConnection(conid, &data, 1, st->m_cmd->m_cmd_id,st->m_cmd->m_passwd);
}

inline void RtpproxyAsynClient::notify_by_task(RA_ECODE ec,SendTask* st)
{
    asyn_notice_error_task(ec,st->m_cmd,st->m_ctx,st->m_cb);
	delete st;
}

#endif // RTPPROXY_ASYN_CLIENT_H__
