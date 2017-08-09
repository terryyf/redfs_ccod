#include "stdafx.h"
#include <list>
#include <string>
#include "asyn_rtpproxy_client.h"
#include "agent_config.h"
#include "rtpproxy_cmd.h"

#ifdef WIN32
#include "objbase.h"
#endif

#include <ctime>
using namespace std;
using namespace aio;

#pragma warning(disable:4355)
RtpproxyAsynClient::RtpproxyAsynClient()
	:m_channel_manager(this),m_runing(false),m_error_num(0),m_begin_error_time(tickcount64()){}

bool RtpproxyAsynClient::init(U64 sipproxy_id,ErrorCallCB error_call_cb,
							  ErrorRtpProxysCB error_rtpproxy_cb)
{
	if (m_runing)
		return true;
	if(NULL==error_call_cb||NULL==error_rtpproxy_cb){
		LGE("ErrorCallCB || ErrorRtpProxysCB is NULL,bad param");
		return false;
	}
	if (!AgentConfig::Obj()->parse()){
		LGE("->Fail to parse configuration file.");
		return false;
	}

	//获取本地IP地址
	list<GMIP> normal_addrs;
	if (!get_normalips(normal_addrs)){
		LGE("->Failed to get_normalips");
		return m_runing;
	}

	if (normal_addrs.empty()){
		LGE("->Find no network device");
		return m_runing;
	}       

	bool ip_validate = false;
	GMIP local_ip = ntohl(inet_addr(AgentConfig::Obj()->m_local_ip.c_str()));
	if(AgentConfig::Obj()->m_local_ip!=""){            
		list<GMIP>::iterator it = normal_addrs.begin();           
		for(;it!=normal_addrs.end();it++){
			if(local_ip==*it){
				GMAddrEx addr;
				addr.m_ip = local_ip;
				m_local_key_addr = addr;
				ip_validate = true;
				LGW(" local_ip %llu %s Succ",m_local_key_addr.m_ip,GMAddress(addr.m_ip).toStr());
				break;
			}
		}
	}

	if(!ip_validate){            
		GMAddrEx addr;
		addr.m_ip = normal_addrs.front();
		m_local_key_addr = addr;
		LGW("assign ip %llu %s not exist, use default ip %llu %s ",local_ip,
			GMAddress(local_ip).toStr(),m_local_key_addr.m_ip,GMAddress(addr.m_ip).toStr());
	}

	//初始化参数
	time_t t;
	time(&t);
	m_sipproxy_id = sipproxy_id;
	m_sipporxy_fake_id = (sipproxy_id<<32)+(U32)t;
	m_error_call_cb = error_call_cb;
	m_error_rtpproxy_cb = error_rtpproxy_cb;

	m_remote_conncet_type = CT_TCP;
	m_remote_cnn_num = AgentConfig::Obj()->m_tcp_cnn_num;

	if(!m_swich_thread_noticer.start(1)){//启动1个线程处理agent检测到错误的回调
		LGE("m_swich_thread_noticer.start(1) fail");
		return false;
	}

	if(!m_channel_manager.start()){
		LGE("m_channel_manager.start() fail");
		m_swich_thread_noticer.stop();
		return false;
	}

	if(!CnnMixer::start_tcp(AgentConfig::Obj()->m_tcp_work_thr_num,
		AgentConfig::Obj()->m_tcp_current_thr_num,AgentConfig::Obj()->m_tcp_current_packet_num,
		AgentConfig::Obj()->m_tcp_post_cp_thr_num,AgentConfig::Obj()->m_tcp_notify_thr_num)){
			LGE("->Failed to CnnMixer::startTcp");
			m_swich_thread_noticer.stop();
			m_channel_manager.stop();
			return false;
	}

    LGM("->Init suc.");
    m_runing = true;
    return m_runing;
}

void RtpproxyAsynClient::uninit()
{
	if (!m_runing)
		return;

	LGW("->begin to uninit.");

	m_runing = false;
	U32 st = GMGetTickCount();
	U32 bt = st;
	U32 ct = st;

	m_channel_manager.stop();
	ct = GMGetTickCount();
	LGW("->take %dms to stop m_channel_manager",ct - bt);

	bt = GMGetTickCount();
	m_swich_thread_noticer.stop();
	ct = GMGetTickCount();
	LGW("->take %dms to stop m_swich_thread_noticer",ct - bt);

	bt = GMGetTickCount();
	CnnMixer::stopAll();
	ct = GMGetTickCount();
	LGW("->take %dms to stop CnnMixer",ct - bt);

	LGW("->finish uninit. taskes %d ms",GMGetTickCount()-st);
}

bool RtpproxyAsynClient::init_remote_channel(const GMAddrEx& key_remote_addr)
{
	if (CT_NO == m_remote_conncet_type){
		LGW("->there is no tcp or ib channel,please check config file");
		return false;
	}

	ConAddress key_addr(m_remote_conncet_type, m_local_key_addr, key_remote_addr, m_remote_cnn_num);
	return m_channel_manager.init_channel(key_addr);
}

void RtpproxyAsynClient::dotask(const GMAddrEx *raddr, CmdHead* cmd, U32 timeout,U64 ctx, void *cb)
{
	RA_ECODE ec=RA_ALLOC_MEMORY_FAILED;
	SendTask* st = NULL;
	ConAddress cra;
	U32 buflen=0;
	SACnnID conid;
    if(raddr && (0 == raddr->m_ip || 0 == raddr->m_port)){
        LGW("bad parameter ip:%d port:%d",raddr->m_ip,raddr->m_port);
 		ec = RA_BAD_PARAMETER;
 		goto E3;
 	}
	st = new SendTask(cmd,cb,ctx,timeout);
	if (raddr){
		cra.m_raddr = *raddr;
		cra.m_laddr = m_local_key_addr;
		cra.m_ct = m_remote_conncet_type;
		cra.m_con_num = m_remote_cnn_num;
    } 

	buflen = PURE_CMD_MAX_SIZE;
	st->m_send_buf = XMemPool::alloc(buflen,GMSZ("dotask,ct:%s",cmd_name(cmd->m_cmd_type)));
	if (NULL == st->m_send_buf){
		Log::writeError(LOG_SA,1,"RtpproxyAsynClient::dotask ->XMemPool::alloc send_buf fail");
		goto E2;
	}
	st->m_key_addr = cra;
	ec = m_channel_manager.get_con_id(st, conid);
	if (RA_SUC != ec)
		goto E1;

	st->m_cmd->m_cmd_id = CmdHead::genCmdID();
	{
		XSpace xs(st->m_send_buf,buflen);
		//为了使发送时间准确，所以在发送前序列化
		st->m_cmd->m_send_req_time = GMGetTickCount();
		try{
			st->m_cmd->ser(xs);
		}catch (exception& ){
			Log::writeError(LOG_SA,1," RtpproxyAsynClient::dotask->fail to serialize cmd %d ",cmd->m_cmd_type);
			ec = RA_SERIALIZE_FAILED;
			goto E1;
		}

		st->m_send_len = xs.used_size();
	}
	if (conid.m_id != 0){		
		m_channel_manager.add_doing_task(st);
        write_cnn(conid,st);        
	}else{
		ec = m_channel_manager.add_waitting_task(st);
		if (RA_SUC != ec){
			Log::writeError(LOG_SA,1,"dotask->add_waitting_task fail task %d ",st);
			goto E1;
		}
	}
	return;

E1:
	XMemPool::dealloc(st->m_send_buf);
E2:
	delete st;
E3:
	asyn_notice_error_task(ec, cmd, ctx, cb);
}

void RtpproxyAsynClient::on_write_connection(CnnMixerError cme,const SACnnID& cid, GMBUF* buf_array, 
    U32 array_num, U32 actuallen, U64 ctx1, U64 ctx2)
{
	if (CME_SUC == cme){
		XMemPool::dealloc(buf_array[0].buf);
		/*目前先不限制同时发送任务数*/
		SendTask* task = NULL;
		if (m_channel_manager.fetch_waiting_task(cid, task)){
			m_channel_manager.add_doing_task(task);
			task->m_cmd->m_send_req_time = GMGetTickCount();
			write_cnn(cid,task);
		}
		return;
	}
	
	char tb[1024]={0};
    RA_ECODE ec = RA_WRITE_CONNECTION;
    LGE("->Fail to write %s,CME:%d",cid.toString(tb,sizeof(tb)),cme);
	if (CME_FATAL_ERROR == cme || CME_INVALID_PARAMETER == cme){
		m_channel_manager.remove_cnn(cid);
		closeConnection(cid);
	}

	SendTask* task;
	if (m_channel_manager.fetch_doing_task(CmdKey(static_cast<U32>(ctx1),static_cast<U32>(ctx2)),task))
	{
		if (!task->m_tried_all){
			SACnnID conid;
			RA_ECODE ec = m_channel_manager.get_con_id(task, conid);
			if (conid.m_id != 0){
				m_channel_manager.add_doing_task(task);
				task->update_cmdhead_before_send();
				return write_cnn(conid,task);   //不用释放send_buf,因为本次发送还在使用
			}else{
				LGW("->发送失败后，获取备用连接失败，不再继续发送");
			}
		}else{
			//所有连接都发送失败，认为该rtpproxy异常，通知Sipproxy处理异常
			LGE("rtpproxy %s error report to sipproxy ",GMAddressEx(task->m_key_addr.m_raddr).toStr());
			m_error_rtpproxy_cb(&task->m_key_addr.m_raddr);
		}

		//g_fl.write(GMSZ("%llu fail to write too many times\r\n",task->m_ctx));

		notify_by_task(ec,task);
	}else{

		/*      g_fl.write(GMSZ("fail to write_connection but can't find doing task. cmd_id %X passwd %X\r\n",
		static_cast<U32>(ctx1),static_cast<U32>(ctx2)));*/
	}

	XMemPool::dealloc(buf_array[0].buf);
}

class ClearTimeoutTaskCB:public IRAStopCallCB
{
public:
	virtual void on_stop_call(RA_ECODE ec, int user_id, PortPair ports, CallInfo* call_info, 
							  GMAddrEx* rtp_addr, U64 ctx){
		delete this;
	}
};

void RtpproxyAsynClient::clear_timeout_task(SACnnID cid,RA_CMD_TYPE ct,char* buf,U32 buflen)
{
	//约定外部一定会调用stop_call
	return;
	try
	{
		XSpace xs(buf,buflen);
		switch(ct){
		case RA_START_CALL_RESP:
			{
				StartCallResp resp;
				resp.unser(xs);
				//TODO
			}
			break;
		case RA_BUILD_PATH_RESP:
			{
				BuildPathResp resp;
				resp.unser(xs);
				if (resp.m_ec != RA_SUC)
					return;

				ClearTimeoutTaskCB* cb = new(std::nothrow) ClearTimeoutTaskCB();
				if (NULL == cb)
					return;


					ConAddress addr;
					if(m_channel_manager.get_cnn_addr(addr,cid)){
						//TODO stop_call
					}else{
						Log::writeError(LOG_SA,1,"RtpproxyAsynClient::clear_timeout_task->fail to get cnn addr");
					}
			}
			break;
		}
	}
	catch (exception&)
	{
		//唉，那就没有办法了	
	}
}

CallInfo CallInfoInner2CallInfo(CallInfoInner* call_info){
	CallInfo info;
	info.caller = call_info->m_caller.c_str();
	info.callee = call_info->m_callee.c_str();
	info.caller_appkey = call_info->m_caller_appkey.c_str();
	info.callee_appkey = call_info->m_callee_appkey.c_str();
	info.call_id = call_info->m_call_id.c_str();
	info.session_h = call_info->m_session_h;
	info.session_l = call_info->m_session_l;
	info.call_type = call_info->m_call_type;
	info.is_caller = call_info->m_is_caller;
	info.report_addr = &call_info->m_report_addr;
	info.dest_rtp_addr = &call_info->m_dest_rtp_addr;
	return info;
}

PathInfo PathInfoInner2PathInfo(PathInfoInner* path_info)
{
	PathInfo info;
	info.path_num = path_info->m_path_num;
	for(int i=0;i<info.path_num;++i){
		info.path_info[i]=path_info->m_path_info[i].c_str();
		info.cid[i]= path_info->m_cid[i];
	}
	return info;
}

void RtpproxyAsynClient::on_read_connection(CnnMixerError cme,const SACnnID& cid,const GMAddrEx& remote_addr, char* buf, U32 reqlen, U32 resplen)
{
	//提交下次收
	if (CME_FATAL_ERROR != cme){
		char* next_rev_buf = (char*)XMemPool::alloc(PURE_CMD_MAX_SIZE,"agent,on_read_connection,post recv buf");
		if (NULL == next_rev_buf){
			LGE("->Fail to new recv buf");
			m_channel_manager.remove_cnn(cid);
			closeConnection(cid);
			XMemPool::dealloc(buf);
			return;
		}else{
			asynReadConnection(cid,next_rev_buf,PURE_CMD_MAX_SIZE);
		}
	}

	if (CME_SUC != cme)
	{
	    char tbuf[1024]={0};
        LGE("->Fail to read %s,%d",cid.toString(tbuf,sizeof(tbuf)),cme);
        XMemPool::dealloc(buf);	

        if (CME_FATAL_ERROR == cme){
            m_channel_manager.remove_cnn(cid);
            closeConnection(cid);
        }
        return;
	}

	CmdHead head;
	XSpace xs(buf,resplen);
	try
	{
		head.unser(xs);
	}
	catch(std::exception&)
	{
		 XMemPool::dealloc(buf);	
		char tbuf[128]={0};
		LGE("->Fail to unserialize CmdHead.Recv Len %d.%s",resplen,cid.toString(tbuf,sizeof(tbuf)));
		return;
	}

	//RA_ERROR_CALL没有主动发起的请求，所以单独处理
	if(RA_ERROR_CALL_RESP==head.m_cmd_type){
		ErrorCallResp resp;
		resp.unser(xs);
		m_error_call_cb(&resp.m_rtp_addr,resp.m_call_id.c_str());
		return XMemPool::dealloc(buf);
	}

	SendTask* st;
	if (!m_channel_manager.fetch_doing_task(CmdKey(head.m_cmd_id,head.m_passwd),st))
	{
		clear_timeout_task(cid,(RA_CMD_TYPE)head.m_cmd_type,buf,resplen);
        /*因为其他时间信息都在请求里面，所以无法获得，只有二阶段信息*/
        U32 phase2 = static_cast<U32>(head.m_send_resp_time - head.m_recv_req_time);//从收到请求到发送回应
        LGE("->cmd_id %X passwd %X cmd_type %s 收到回应但是任务已超时。二阶段时间%d",
			head.m_cmd_id,head.m_passwd,cmd_name(head.m_cmd_type),phase2);

		 XMemPool::dealloc(buf);	
		return;
	}

	try
	{
		XSpace xs(buf,resplen);
		switch (head.m_cmd_type)
		{
		case RA_GET_PORTS_RESP:
			{
				GetPortsResp resp;
				resp.unser(xs);
				GetPortsReq* req = (GetPortsReq*)st->m_cmd;
				((IRAGetPortCB*)st->m_cb)->on_get_ports(resp.m_ec,resp.m_portpair,resp.m_user_id,
														&req->m_rtp_addr,st->m_ctx);
			}
			break;
		case RA_START_CALL_RESP:
			{
				StartCallResp resp;
				resp.unser(xs);
				StartCallReq* req = (StartCallReq*)st->m_cmd;
				CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
		
				PathInfo path_info = PathInfoInner2PathInfo(&req->m_path_info);
				((IRAStartCallCB*)st->m_cb)->on_start_call(resp.m_ec,resp.m_path_suc,req->m_user_id,
					req->m_portpair,&call_info,&path_info,&req->m_short_link_info,&req->m_rtp_addr,st->m_ctx);
			}
			break;
		case RA_STOP_CALL_RESP:
			{
				StopCallResp resp;
				resp.unser(xs);
				StopCallReq* req = (StopCallReq*)st->m_cmd;
				CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
				((IRAStopCallCB*)st->m_cb)->on_stop_call(resp.m_ec,req->m_user_id,req->m_ports,
														 &call_info,&req->m_rtp_addr,st->m_ctx);
			}
			break;
		case RA_FREE_PORTS_RESP:
			{
				FreePortResp resp;
				resp.unser(xs);
				FreePortsReq* req = (FreePortsReq*)st->m_cmd;
				((IRAFreePortsCB*)st->m_cb)->on_free_ports(resp.m_ec,req->m_user_id,req->m_portpair,
															&req->m_rtp_addr,st->m_ctx);
				
			}
			break;

        case RA_BUILD_PATH_RESP:
            {
                BuildPathResp resp;
                resp.unser(xs);
				BuildPathReq* req = (BuildPathReq*)st->m_cmd;
				CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
				PathInfo path_info = PathInfoInner2PathInfo(&req->m_path_info);
				((IRABuildPathCB*)st->m_cb)->on_build_path(resp.m_ec,resp.m_path_suc,req->m_user_id,
															&call_info,&path_info,&req->m_rtp_addr,
															st->m_ctx);
            }
            break;
		case RA_SHAKE_HAND_RESP:
			{
				ShakeHandResp resp;
				resp.unser(xs);

				//没把 ShakeHandReq从超时检测队列中取出，超时后会被超时检测线程取出，取出后不做任何动作
				ConAddress key_addr(m_remote_conncet_type, m_local_key_addr, resp.m_key_addr, m_remote_cnn_num);
				m_channel_manager.set_shake_hand(key_addr);

				for (XList<GMAddrEx>::iterator it = resp.m_remoteAddrs.begin();
					it!=resp.m_remoteAddrs.end();++it){
						m_channel_manager.add_con_address(key_addr, ConAddress(m_remote_conncet_type,
							GMAddrEx(), *it, m_remote_cnn_num));
				}
			}
			break;
		case RA_SIPPROXY_OFFLINE_RESP:
			{
				SipproxyOfflineResp resp;
				resp.unser(xs);
				SipproxyOfflineReq* req = (SipproxyOfflineReq*)st->m_cmd;

				((IRASipproxyOffLineCB*)st->m_cb)->on_sipproxy_off_line(resp.m_ec,&req->m_rtp_addr,
																		st->m_ctx);
			}
			break;
		default:
			assert(false);
			break;
		}
	}
	catch(exception&)
	{
		notify_by_task(RA_UNSERIALIZE_FAILED,st);
		 XMemPool::dealloc(buf);
		return;
	}

	delete st->m_cmd;
	delete st;//释放SentTask
    XMemPool::dealloc(buf);	
}

//会删除req
void RtpproxyAsynClient::notify_by_req(RA_ECODE ec, CmdHead* cmd, U64 ctx, void* cb)
{
	switch(cmd->m_cmd_type)
	{
	case RA_GET_PORTS_REQ:
		{
			GetPortsReq* req = (GetPortsReq*)cmd;
			((IRAGetPortCB*)cb)->on_get_ports(ec,PortPair(),req->m_user_id,&req->m_rtp_addr,ctx);
		}
		break;
	case RA_START_CALL_REQ:
		{
			StartCallReq* req = (StartCallReq*)cmd;
			CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
			
			PathInfo path_info = PathInfoInner2PathInfo(&req->m_path_info);
			((IRAStartCallCB*)cb)->on_start_call(ec,false,req->m_user_id,req->m_portpair,&call_info,
												 &path_info,&req->m_short_link_info,&req->m_rtp_addr,ctx);
		}
		break;
	case RA_STOP_CALL_REQ:
		{
			StopCallReq* req = (StopCallReq*)cmd;
			CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
			((IRAStopCallCB*)cb)->on_stop_call(ec,req->m_user_id,req->m_ports,&call_info,
				&req->m_rtp_addr,ctx);
		}
		break;
	case RA_FREE_PORTS_REQ:
		{
			FreePortsReq* req = (FreePortsReq*)cmd;
			((IRAFreePortsCB*)cb)->on_free_ports(ec,req->m_user_id,req->m_portpair,&req->m_rtp_addr,ctx);

		}
		break;
	case RA_BUILD_PATH_REQ:
		{
			BuildPathReq* req = (BuildPathReq*)cmd;
			CallInfo call_info = CallInfoInner2CallInfo(&req->m_call_info);
			PathInfo path_info = PathInfoInner2PathInfo(&req->m_path_info);
			((IRABuildPathCB*)cb)->on_build_path(ec,false,req->m_user_id,&call_info,&path_info,
												 &req->m_rtp_addr,ctx);
		}
		break;
	case RA_SHAKE_HAND_REQ:
		{

		}
		break;
	case RA_SIPPROXY_OFFLINE_REQ:
		{
			SipproxyOfflineReq* req = (SipproxyOfflineReq*)cmd;
			((IRASipproxyOffLineCB*)cb)->on_sipproxy_off_line(ec,&req->m_rtp_addr,ctx);
		}
		break;
	default:
		assert(false);
		break;
	}

	delete cmd;//删除请求
	cmd = NULL;
}

void RtpproxyAsynClient::asyn_get_ports(int user_id, GMAddrEx* rtp_addr,IRAGetPortCB* cb,
										U32 timeout, U64 ctx)
{
	GetPortsReq *req = new(std::nothrow) GetPortsReq(user_id,rtp_addr);
	if (req == NULL){
		return cb->on_get_ports(RA_ALLOC_MEMORY_FAILED,PortPair(),user_id,rtp_addr, ctx);
	}

	req->m_sipproxy_id = m_sipproxy_id;
	req->m_sipproxy_fake_id = m_sipporxy_fake_id;
	if(0==user_id || NULL==rtp_addr ||0==rtp_addr->m_port|| 0==timeout) {
		return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
    }

	dotask(rtp_addr, req, timeout, ctx, cb);
}
void RtpproxyAsynClient::asyn_free_ports(int user_id, GMAddrEx* rtp_addr, PortPair ports,
										IRAFreePortsCB* cb, U32 timeout, U64 ctx)
{
	FreePortsReq* req = new(std::nothrow) FreePortsReq(user_id,ports,rtp_addr);
	if(NULL==req){
		return cb->on_free_ports(RA_ALLOC_MEMORY_FAILED,user_id,ports,rtp_addr,ctx);
	}

	req->m_sipproxy_id = m_sipproxy_id;
	req->m_sipproxy_fake_id = m_sipporxy_fake_id;
	if(0==user_id || NULL==rtp_addr ||0==rtp_addr->m_port|| 0==timeout || 0==ports.rtp_recv_port 
		|| 0==ports.rtp_sent_port) {
		return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
	}

	dotask(rtp_addr, req, timeout, ctx, cb);
}

void RtpproxyAsynClient::asyn_start_call(int user_id, PortPair ports,CallInfo* call_info,
										 PathInfo* path_info, ShortLinkInfo* short_link_info,
										 GMAddrEx* rtp_addr, IRAStartCallCB* cb,U32 timeout,U64 ctx)
{
	StartCallReq* req = new(std::nothrow)StartCallReq(m_sipproxy_id,m_sipporxy_fake_id,user_id,ports,
													  rtp_addr);
	if(NULL==req){
		LGE("new StartCallReq fail");
		return cb->on_start_call(RA_ALLOC_MEMORY_FAILED,false,user_id,ports,call_info,path_info,
								 short_link_info,rtp_addr,ctx);
	}

	if(0==user_id || NULL==rtp_addr ||0==rtp_addr->m_port|| 0==timeout || 0==ports.rtp_recv_port 
		|| 0==ports.rtp_sent_port || NULL==call_info || NULL==path_info || NULL==short_link_info){
			return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
	}
	req->m_call_info = *call_info;
	req->m_path_info = *path_info;
	req->m_short_link_info = *short_link_info;
	dotask(rtp_addr, req, timeout, ctx, cb);
}

void RtpproxyAsynClient::asyn_stop_call(int user_id, PortPair ports, CallInfo* call_info,
										GMAddrEx* rtp_addr, IRAStopCallCB* cb, U32 timeout, U64 ctx)
{
	StopCallReq* req = new(std::nothrow)StopCallReq(m_sipproxy_id,m_sipporxy_fake_id,user_id,ports,
													rtp_addr);
	if(NULL==req){
		LGE("new StopCallReq fail");
		return cb->on_stop_call(RA_ALLOC_MEMORY_FAILED,user_id,ports,call_info,rtp_addr,ctx);
	}
	if(0==user_id || NULL==rtp_addr || 0==rtp_addr->m_port || 0==timeout || 0==ports.rtp_recv_port 
		|| 0==ports.rtp_sent_port || NULL==call_info) {
			return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
	}
	req->m_call_info = *call_info;
	dotask(rtp_addr, req, timeout, ctx, cb);
}

void RtpproxyAsynClient::asyn_build_path(int user_id,PortPair ports,CallInfo* call_info,
										 PathInfo* path_info,GMAddrEx* rtp_addr, IRABuildPathCB* cb,
										 U32 timeout,U64 ctx)
{
	BuildPathReq* req = new(std::nothrow)BuildPathReq(m_sipproxy_id,m_sipporxy_fake_id,user_id,
													  ports,rtp_addr);
	if(NULL==req){
		LGE("new BuildPathReq fail");
		return cb->on_build_path(RA_ALLOC_MEMORY_FAILED,false,user_id,call_info,path_info,rtp_addr,ctx);
	}
	if(0==user_id || NULL==rtp_addr || 0==rtp_addr->m_port || 0==timeout || NULL==call_info
		|| NULL==path_info) {
			return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
	}

	req->m_call_info = *call_info;
	req->m_path_info = *path_info;
	dotask(rtp_addr, req, timeout, ctx, cb);
}

void RtpproxyAsynClient::InnerNotifyTask::excute()
{
	RtpproxyAsynClient::Obj()->notify_by_req(m_ec,m_req,m_ctx,m_cb);
	delete this;
}

void RtpproxyAsynClient::asyn_notice_error_task(RA_ECODE nErr, CmdHead* req, U64 ctx,void* cb)
{
	//req内部释放
	InnerNotifyTask* nt = new(std::nothrow) InnerNotifyTask(nErr,req,ctx,cb);
	//构建异步通知失败，只能同步通知了,req内部释放
	if(NULL == nt)
		return notify_by_req(nErr,req,ctx,cb);

	if(!m_swich_thread_noticer.add(nt)){
		delete nt;
		notify_by_req(nErr,req,ctx,cb);
	}	
}

void RtpproxyAsynClient::asyn_sipproxy_off_line(GMAddrEx* rtp_addr, IRASipproxyOffLineCB* cb, 
												U32 timeout, U64 ctx)
{
	SipproxyOfflineReq* req = new(std::nothrow)SipproxyOfflineReq();
	if (req == NULL){
		return cb->on_sipproxy_off_line(RA_ALLOC_MEMORY_FAILED,rtp_addr, ctx);
	}

	req->m_sipproxy_id = m_sipproxy_id;
	req->m_sipproxy_fake_id = m_sipporxy_fake_id;
	if(NULL==rtp_addr ||0==rtp_addr->m_port|| 0==timeout) {
		return asyn_notice_error_task(RA_BAD_PARAMETER,req,ctx,cb);
	}

	dotask(rtp_addr, req, timeout, ctx, cb);
}
