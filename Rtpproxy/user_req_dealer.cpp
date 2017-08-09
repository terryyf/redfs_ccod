#include "stdafx.h"
#include "rtpproxy_ecode.h"
#include "rtpproxy_config.h"
#include "user_req_dealer.h"
#include "rtpproxy.h"
#include "RelayClient/relayclientexport.h"
using namespace cp;

//relay_client读通知
int on_rtp_package_recv(unsigned int hsid,unsigned int lsid,int cid,char* buf,int len,
					    struct sockaddr* addr,int addrlen);

UserReqDealer::UserReqDealer():m_udp_recv_speed(300,10),m_tcp_recv_speed(300,10),m_ss(NULL),
	m_udp_send_speed(300,10),m_tcp_send_speed(300,10),m_sys_info(1000,60,getpid())	
{
	m_udp_recv_speed.start();
	m_udp_send_speed.start();


	m_tcp_recv_speed.start();
	m_tcp_send_speed.start();
	m_tcp_recv_package.start();
	m_tcp_send_package.start();
	m_tcp_send_fail_package.start();

	for(int i=0;i<MAX_CMD_NUM;++i) {
		m_req_speed[i].start();
		m_average_duration_time[i].start();
		m_max_duration_time[i].start();
	}
}

UserReqDealer::~UserReqDealer()
{
	m_udp_recv_speed.stop();
	m_udp_send_speed.stop();
	m_udp_recv_package.stop();
	m_udp_send_package.stop();
	m_udp_drop_package.stop();
	m_udp_send_fail_package.stop();
	
	m_tcp_recv_speed.stop();
	m_tcp_send_speed.stop();
	m_tcp_recv_package.stop();
	m_tcp_send_package.stop();
	m_tcp_send_fail_package.stop();
	for(int i=0;i<MAX_CMD_NUM;++i) {
		m_req_speed[i].stop();
		m_average_duration_time[i].stop();
		m_max_duration_time[i].stop();
	}

}

bool UserReqDealer::start()
{
	m_ss = new(std::nothrow) SpeedStatistic(RtpProxyConfig::GetInst()->m_report_time*1000);
	if(!m_ss)
		return false;

	list<GMIP> local_ips;
	get_normalips(local_ips);
	if(local_ips.empty()){
		LGE("->there is no normal netwrok interface,so don't run tcp service");
		return false;
	}

	//如果没有配置ip地址信息，取第一个检测到的ip地址作为监听地址
	if(RtpProxyConfig::Obj()->m_svr_ips.empty()){
		GMAddrEx addr;
		addr.m_ip = local_ips.front();
		RtpProxyConfig::Obj()->m_svr_ips.push_back(addr);
	}

	if(!CnnMixer::start_tcp(RtpProxyConfig::Obj()->m_tcp_work_thread_num,
							RtpProxyConfig::Obj()->m_tcp_concurrent_thread_num,
							RtpProxyConfig::Obj()->m_tcp_concurrent_packet_num,
							RtpProxyConfig::Obj()->m_tcp_post_cp_thread_num,
							RtpProxyConfig::Obj()->m_tcp_notify_thread_num)){
		LGE("->fail to start tcp");
		return false;
	}

	//监听所有地址
	for(std::list<GMAddrEx>::iterator it = RtpProxyConfig::Obj()->m_svr_ips.begin();
		it!=RtpProxyConfig::Obj()->m_svr_ips.end();){
			if(!CnnMixer::add_tcp_listen_socket(*it,200)){
				LGE("->fail to addListenSock %s err: %d %s ",GMAddressEx(*it).toStr(),errno,
					strerror(errno));
				it = RtpProxyConfig::Obj()->m_svr_ips.erase(it);
			}else{
				++it;
			}
	}

	if (RtpProxyConfig::Obj()->m_svr_ips.empty()){
		LGE("fail to listen tcp.check the config ip");
		return true;
	}

	m_listen_addr = RtpProxyConfig::Obj()->m_svr_ips.front();//取第一个有效地址作为对外的地址

	//启动UDP服务
	if(!CnnMixer::start_udp(RtpProxyConfig::Obj()->m_udp_work_thread_num,
							RtpProxyConfig::Obj()->m_udp_concurrent_thread_num,
							RtpProxyConfig::Obj()->m_udp_concurrent_packet_num)){
		CnnMixer::stopAll();
		return false;
	}

	CPID id,id1;
	GMAddrEx rtp_addr= m_listen_addr;
	GMAddrEx rtcp_addr = m_listen_addr;
	U32 port = RtpProxyConfig::Obj()->m_udp_min_port;
	//端口必须是从偶数开始的，而且下一个奇数端口必须可用；只有连续的两个端口可用，才认为是有效端口对
	if(0!=port%2){
		++port;
	}

	PortPair portpair;
	memset(&portpair,0,sizeof(PortPair));
	for(;port<=RtpProxyConfig::Obj()->m_udp_max_port;){
		rtp_addr.m_port = port++;
		rtcp_addr.m_port = port++;

		//rtp端口绑定
		if(CnnMixer::add_udp_socket(id,rtp_addr)){
			//rtcp端口绑定
			if(CnnMixer::add_udp_socket(id1,rtcp_addr)){
				//将cpid信息保存起来
				m_cpid_2_addr_map.insert(make_pair(id,rtp_addr));
				m_cpid_2_addr_map.insert(make_pair(id1,rtcp_addr));

				//发起rtp和rtcp端口的收
				char* buf = XMemPool::alloc(MAX_MEDIA_DATA_SIZE,"MEDIA_DATA");
				asynReadConnection(SACnnID(SIT_UDP,id),buf,MAX_MEDIA_DATA_SIZE);
				buf = XMemPool::alloc(MAX_MEDIA_DATA_SIZE,"MEDIA_DATA");
				asynReadConnection(SACnnID(SIT_UDP,id1),buf,MAX_MEDIA_DATA_SIZE);

				//将2个rtp端口组合成一个端口对放置到FreePortPairContainer
				if(0==portpair.rtp_recv_port){
					portpair.rtp_recv_port = rtp_addr.m_port;					
				}else if(0==portpair.rtp_sent_port){
					portpair.rtp_sent_port = rtp_addr.m_port;
					//将端口对信息加入到FreePortPair中
					FreePortPairContainer::Obj()->add_portpair(portpair);
					memset(&portpair,0,sizeof(PortPair));
				}
			}else{
				//端口不连续，释放rtp端口
				CnnMixer::del_udp_socket(id);
			}
		}
	}

	//初始化RelayClient TODO
	
	//初始化RouterClient,向RC注册 TODO

	LGW("SUC! listen_addr %s PortPair %d ",GMAddressEx(m_listen_addr).toStr(),
		FreePortPairContainer::Obj()->free_portpair());
	return true;
}

void UserReqDealer::stop()
{
	if (NULL == m_ss)//说明没有成功启动过
		return;

	//向RouterCenter注销自己
	
	//销毁RelayClient

	CnnMixer::stopAll();
	if(m_ss){
		delete m_ss;
		m_ss = NULL;
	}
}
//******************************deal communication********************************************
//*/
void UserReqDealer::on_accept_connection(const SACnnID& id)
{	
	/*第一次提交收，分配内存，以后收都不继续使用这个内存，知道接收失败不继续收了才释放*/
	bool post_suc = false;
	char* recvbuf = (char*)XMemPool::alloc(PURE_CMD_MAX_SIZE,"CC::UserReqDealer::on_accept_connection->recv req buf");
	if (NULL != recvbuf){
		post_suc = true;
		asynReadConnection(id,recvbuf,PURE_CMD_MAX_SIZE);
	}

	if (!post_suc){
		Log::writeError(LOG_SA,1,"CC::UserReqDealer::onAcceptPipeFinished->Don't post any "
			"read pipe,because of failing to alloc memery.Close Pipe");
		closeConnection(id);
	}
}

/*
*服务器端连接内存使用方式
*IB内存是IB模块内部分配，所以由IB模块自己管理
*pipe,tcp内存是投递接收时分配，发送完回应释放。对于不发送回应的命令，处理完请求就要立即释放。
* 这里的buf是动态分配的内存
*/
void UserReqDealer::on_read_connection(CnnMixerError cme,const SACnnID& cid,const GMAddrEx& remote_addr,
										char* buf,U32 reqlen,U32 recvlen)
{
	/*
		根据SACnnID连接类型进行处理，TCP都是控制协议，UDP都是媒体数据
		UDP需要根据id找到对应的端口，然后在端口转发信息表中查找对应端口的转发信息，
		根据转发规则进行媒体数据转发处理
	*/
	if(CME_FATAL_ERROR != cme){
		U32 buf_size = PURE_CMD_MAX_SIZE;
		if(cid.m_tp==SIT_UDP){
			buf_size = MAX_MEDIA_DATA_SIZE;
		}
		char* next_recv_buf = (char*)XMemPool::alloc(buf_size,"UserReqDealer::on_read_connection->recv req buf");
		if (NULL == next_recv_buf){
			close_connection(cid);		    /*因为没有投递收，所以必须关闭管道。否则agent发送的请求，服务器无法收到。*/
			XMemPool::dealloc(buf);
			return;
		}
		asynReadConnection(cid,next_recv_buf, buf_size);
	}

	if(CME_SUC != cme){                    /*接收错误，释放上次接收分配的内存*/
		char tbuf[128] = {0};
		Log::writeError(LOG_SA,1,"CC::UserReqDealer::on_read_connection->Fail to read %s",
            cid.toString(tbuf,sizeof(tbuf)));
		XMemPool::dealloc(buf);
		if (CME_FATAL_ERROR == cme)
			close_connection(cid);
		return;
	}
	
	//处理UDP媒体数据转发
	if(cid.m_tp==SIT_UDP){
		//更新系统udp的收包量/数统计信息
		m_udp_recv_speed.count_new_value(recvlen);
		m_udp_recv_package.count_new_value(1);
		//失败释放内存，成功内部释放内存(减少数据拷贝)
		if(!deal_media_data_transform(cid,remote_addr,buf,recvlen)){
			XMemPool::dealloc(buf);
		}
		return;
	}

	//更新系统tcp的收包量/数统计信息
	m_tcp_recv_speed.count_new_value(recvlen);
	m_tcp_recv_package.count_new_value(1);

	CmdHead head;
	XSpace xs(buf,recvlen);
	try
	{
		head.unser(xs);
	}catch (exception&){
		XMemPool::dealloc(buf);
		char idbuf[128]={0};
		Log::writeError(LOG_SA,1,"CC::UserReqDealer::on_read_connection->Fail to unser"
            "CmdHead.Close pipe %s. recv data len %u",cid.toString(idbuf,sizeof(idbuf)),recvlen);
		return;	
	}

 	if (RA_KEEP_ALIVE_REQ != head.m_cmd_type)
	    m_ss->count_req(1);

	/*
	*CAUTION::不发回应的命令要释放内存
	*/

	switch (head.m_cmd_type){
	/*file command*/
	case RA_BUILD_PATH_REQ:
		return deal_build_path(cid,buf,recvlen);
	case RA_START_CALL_REQ:
		return deal_start_call(cid,buf,recvlen);
	case RA_STOP_CALL_REQ:
		return deal_stop_call(cid,buf,recvlen);
	case RA_GET_PORTS_REQ:
		return deal_get_portpair(cid,buf,recvlen);
	case RA_FREE_PORTS_REQ:
		return deal_free_portpair(cid,buf,recvlen);
	case RA_KEEP_ALIVE_REQ:
		return deal_keep_alive(cid,buf,recvlen);	
	case RA_SHAKE_HAND_REQ:
		return deal_shake_hand(cid,buf,recvlen);
	case RA_SIPPROXY_OFFLINE_REQ:
		return deal_sipproxy_offline(cid,buf,recvlen);
	default:
		LGE("->receive unnkonw command name:%s,close connection id:%d",cmd_name(head.m_cmd_type),
			cid.m_id);
		close_connection(cid);/*这个连接发来的未知的命令类型，关闭这个连接不在处理这个连接的请求*/
		XMemPool::dealloc(buf);
	}
}


void UserReqDealer::deal_get_portpair(const SACnnID& cid,char* buf,U32 recvlen)
{
	GetPortsReq req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}

	UserPortInfo info;
	info.user_id = req.m_user_id;
	req.m_sipproxy_fake_id;
	if(RtpProxy::Obj()->get_ports(req.m_sipproxy_id,req.m_sipproxy_fake_id,info)){
		ec = RA_SUC;
	}else{
		ec = RA_FAIL;
	}

E:
	GetPortsResp resp(ec,req.m_user_id,info.ports);
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp,buf,cid);
}


class BuildPathTaskCB:public IRPBuildPathCB{
public:
	BuildPathTaskCB(const SACnnID& cid,StartCallReq* req,BuildPathReq* build_req)
		:m_cid(cid),m_is_build_path(true){
			if(req){
				m_resp.constuct_resp_from_req(*req);
				m_is_build_path = false;
				return;
			}

			if(build_req){
				m_build_path_resp.constuct_resp_from_req(*build_req);
			}
	}
	void on_build_path(RA_ECODE ec,bool suc){
		char* buf = XMemPool::alloc(PURE_CMD_MAX_SIZE);
		if(buf){
			if(m_is_build_path){
				m_build_path_resp.m_ec = ec;
				m_build_path_resp.m_path_suc = suc;
				UserReqDealer::Obj()->do_back_chores(&m_build_path_resp,buf,m_cid);
			}else{
				m_resp.m_ec = ec;
				m_resp.m_path_suc = suc;
				UserReqDealer::Obj()->do_back_chores(&m_resp,buf,m_cid);
			}			
		}else{
			LGE("XMemPool::alloc(PURE_CMD_MAX_SIZE) fail");
		}

		delete this;
	}
	SACnnID m_cid;
	StartCallResp m_resp;
	BuildPathResp m_build_path_resp;
	bool m_is_build_path;
};

void UserReqDealer::deal_start_call(const SACnnID& cid,char* buf,U32 recvlen)
{
	StartCallReq req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}
	BuildPathTaskCB* cb = new BuildPathTaskCB(cid,&req,NULL);
	if(!cb){
		ec = RA_ALLOC_MEMORY_FAILED;
		goto E;
	}
    RtpProxy::Obj()->asyn_start_call(req.m_sipproxy_id,req.m_user_id,req.m_portpair,
									 &req.m_call_info,&req.m_path_info,&req.m_short_link_info,cb);
	ProcessKeepAliveManager::Obj()->update(req.m_sipproxy_id,cid);
	return;//等建路回调
E:
	StartCallResp resp(ec,false);
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp,buf,cid);
}

void UserReqDealer::deal_free_portpair(const SACnnID& cid,char* buf,U32 recvlen)
{
	FreePortsReq req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}
	ec = RA_SUC;
	UserPortInfo info;
	info.user_id = req.m_user_id;
	info.ports = req.m_portpair;
	RtpProxy::Obj()->free_ports(req.m_sipproxy_id,info);

E:
	FreePortResp resp;
	resp.m_ec = ec;
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp, buf, cid);
}

void UserReqDealer::deal_stop_call(const SACnnID& cid,char* buf,U32 recvlen)
{
	StopCallReq req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}
	ec = RA_SUC;
	RtpProxy::Obj()->stop_call(req.m_sipproxy_id,req.m_user_id,req.m_ports,&req.m_call_info);
	ProcessKeepAliveManager::Obj()->update(req.m_sipproxy_id,cid);
E:
	StopCallResp resp;
	resp.m_ec = ec;
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp,buf,cid);
}

void UserReqDealer::deal_build_path(const SACnnID& cid,char* buf,U32 recvlen)
{
	BuildPathReq req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}

	BuildPathTaskCB* cb = new BuildPathTaskCB(cid,NULL,&req);
	if(!cb){
		ec = RA_ALLOC_MEMORY_FAILED;
		goto E;
	}
	return RtpProxy::Obj()->asyn_build_path(req.m_user_id,req.m_ports,&req.m_call_info,
											&req.m_path_info,cb);
E:
	BuildPathResp resp;
	resp.m_ec = ec;
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp,buf,cid);
}

void UserReqDealer::deal_keep_alive( const SACnnID& cid,char* buf,U32 recvlen )
{
	KeepConnetionAlive req;
	bool version_conflict = false;
	if (!do_front_chores(&req, buf, recvlen, cid,version_conflict))
		return;

	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		XMemPool::dealloc(buf);
		return;
	}
	//更新sipproxy对应的时间和连接,连接供建路通道保活失败时用
	ProcessKeepAliveManager::Obj()->update(req.m_sipproxy_id,cid);
	XMemPool::dealloc(buf);
}
void UserReqDealer::deal_shake_hand(const SACnnID& cid,char* buf,U32 recvlen)
{
	ShakeHandReq req;
	bool version_conflict = false;
	if(!do_front_chores(&req,buf,recvlen,cid,version_conflict))
		return;
	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}
	
E:
	ShakeHandResp resp;
	resp.constuct_resp_from_req(req);
	resp.m_key_addr = req.m_key_addr;
	CnnMixer::get_tcp_listen_addrs(resp.m_remoteAddrs);

	do_back_chores(&resp,buf,cid);
}

void UserReqDealer::deal_sipproxy_offline(const SACnnID& cid,char* buf,U32 recvlen)
{
	SipproxyOfflineReq req;
	bool version_conflict = false;
	if(!do_front_chores(&req,buf,recvlen,cid,version_conflict))
		return;
	RA_ECODE ec = RA_VERSION_INCONFORMITY;
	if(version_conflict){
		goto E;
	}
	RtpProxy::Obj()->deal_dead_sipproxy(req.m_sipproxy_id,req.m_sipproxy_fake_id);
	ec = RA_SUC;
E:
	SipproxyOfflineResp resp;
	resp.m_ec = ec;
	resp.constuct_resp_from_req(req);
	do_back_chores(&resp,buf,cid);
}


/*unserialize req, realloc buf for ib channel*/
bool UserReqDealer::do_front_chores(CmdHead* req,char*& buf,U32 rcvlen,const SACnnID& src_cid, bool& version_conflict)
{
	try
	{
		XSpace xs(buf,rcvlen);
		req->unser(xs);
	}catch (exception& e){
		char tb[1024]={0};
		LGE(" unser failed,SACnnID %s, excep: %s",src_cid.toString(tb,sizeof(tb)),e.what());
		XMemPool::dealloc(buf);
		return false;
	}

	if(req->m_version!=RTPPROXY_VERSION){
		LGE("客户端和服务器版本不一致,请更换正确版本。客户端版本%u 服务器版本%u",
			req->m_version,RTPPROXY_VERSION);
		version_conflict = true;
	}
	if(req->m_cmd_type!=RA_KEEP_ALIVE_REQ){
		record_stat(this, GMGetTickCount(), static_cast<RA_CMD_TYPE>(req->m_cmd_type+1));
	}
	req->m_recv_req_time = tickcount();
	m_req_speed[req->m_cmd_type].count_new_value(1);
	return true;
}

void UserReqDealer::do_back_chores(CmdHead* resp,char* buf,const SACnnID& srcid,U32 skip_len)
{
	resp->m_send_resp_time = GMGetTickCount();
	m_req_speed[resp->m_cmd_type].count_new_value(1);
	XSpace xs(buf,PURE_CMD_MAX_SIZE);
	try
	{
		resp->ser(xs);
		if (skip_len>0){	
			xs.skip(skip_len);
			resp->set_body_len(xs);
		}
	}
	catch(exception&)
	{
		assert(false);
	}
	
	m_tcp_send_package.count_new_value(1);

	GMBUF buf_array(buf,xs.used_size());
	asynWriteConnection(srcid, &buf_array,1,resp->m_cmd_type,0);
}


/*
	UDP需要根据id找到对应的端口，然后在端口转发信息表中查找对应端口的转发信息，
	根据转发规则进行媒体数据转发处理
	false:外部释放内存
	true:外部不释放内存
*/
//为减少内存拷贝，使用同一内存发多次数据，使用该回调通知，等引用计数为0才释放内存
class WriteUdpConnectionCB:public IWriteConnectionNotice
{
public:
	WriteUdpConnectionCB(int num,char* buf):m_num(num),m_buf(buf){}
	virtual void on_write(){
		if(0==InterlockedDecrement((volatile int*)&m_num)){
			XMemPool::dealloc(m_buf);
			delete this;
		}
	}

	char* m_buf;
	int m_num;
};
void add_qn_sub_ext_to_buf(qn_sub_ext_t* sub_ext,int port,SptrTFInfo& info,int cid){
	sub_ext->qn_pt = 0==port%2 ? QN_MEDIA_AUDIO:QN_MEDIA_AUDIO_RTCP;
	sub_ext->qn_seq_number = info->get_seq_num();
	sub_ext->qn_server_seq = sub_ext->qn_seq_number;
	sub_ext->qn_sp_pt_seq = sub_ext->qn_seq_number;
	sub_ext->qn_static_sub_id = cid;
	sub_ext->qn_subflow_id = cid;
	sub_ext->qn_sub_ext_t_len = sizeof(qn_sub_ext_t);
}
bool UserReqDealer::deal_media_data_transform(const SACnnID& cid,const GMAddrEx& remote_addr,char* buf,
											  U32 recvlen)
{
	m_udp_recv_speed.count_new_value(recvlen);
	int port = cid_2_port(cid.m_id);
	if(0==port){
		
		return false;
	}

	//处理rtcp报文
	if(1==port%2){

		return true;
	}

	//处理rtp媒体数据转发
	RtpHeader* rtp_header = (RtpHeader*)buf;
	U64 seq_num = rtp_header->seq_number;
	PortStatManager::Obj()->update_recv_statistic(port,seq_num,recvlen);
	//在端口转发信息表中查找对应端口的转发信息
	SptrTFInfo info = TransformInfoManager::Obj()->get_sptr_tf_info(port);
	if(!info.Get()){
		LGE("not find port %d transform info",port);
		return false;
	}

	info->m_rw_lock.writeLock();
	//更新目标地址,判断ssrc地址
	//第一次媒体通信，记录通信地址和ssrc_id
	if(info->m_dest_addr==GMAddrEx()&&0==info->m_ssrc_id){
		info->m_ssrc_id = rtp_header->ssrc;
		info->m_dest_addr = remote_addr;
	}else if(info->m_dest_addr!=remote_addr){//比较地址
		if(info->m_ssrc_id==rtp_header->ssrc){
			info->m_dest_addr = remote_addr;
		}else{
			info->m_rw_lock.unWriteLock();
			LGE("addr change and ssrc change,session error don't send data");
			return false;
		}
	}
	info->m_rw_lock.unWriteLock();
	//中转方式
	if(CT_TRANSFORM==info->m_call_info.m_call_type){
		info->m_rw_lock.readLock();
		//建路不成功
		if( PS_SUC!=info->m_path_status){
			for(int i=0;i<3;++i){
				//更新有效通道的丢包记录
				if(0!=info->m_path_info.m_cid[i]){
					SessionID session(info->m_call_info.m_session_h,info->m_call_info.m_session_l,
						info->m_path_info.m_cid[i]);
					SessionStatManager::Obj()->update_send_loss(&session);
				}					
			}	
			info->m_rw_lock.unReadLock();
		}else{
			info->m_rw_lock.unReadLock();
			//建路成功，按照relayclient格式组装包，调用接口发送数据
			//发两次数据，只有一条路也要发两条
			int cid=0;
			int send_num=0;
			for(int i=0;i<3;++i){
				if(0!=info->m_path_info.m_cid[i]){
					cid = info->m_path_info.m_cid[i];
					//cid有效，最多发送2次
					if(send_num++<2){
						add_qn_sub_ext_to_buf((qn_sub_ext_t*)(buf+recvlen),port,info,cid);
						//
						sockaddr_in localaddr;
						//TODO 从path中解析出sock_addr
						sockaddr dest_sockaddr;
						/*	localaddr.sin_family = AF_INET;
						localaddr.sin_port = htons(addr.m_port);
						localaddr.sin_addr.s_addr = htonl(addr.m_ip);*/
						int ret = SendRtpPackage(info->m_call_info.m_session_h,info->m_call_info.m_session_l,
							info->m_path_info.m_cid[i],buf,recvlen+sizeof(qn_sub_ext_t),
							&dest_sockaddr,sizeof(sockaddr));
						SessionID session(info->m_call_info.m_session_h,info->m_call_info.m_session_l,cid);
						int sent_len = 0 ;
						//更新统计
						if(0==ret){			
							sent_len =recvlen+sizeof(qn_sub_ext_t);	
						}
						SessionStatManager::Obj()->update_send_statistic(&session,sent_len);
					}					
				}				
			}	

			if(cid==0 || send_num==0){
				LGE("all channel invalid");//more info
				return false;
			}

			if(cid!=0&&send_num<2){
				add_qn_sub_ext_to_buf((qn_sub_ext_t*)(buf+recvlen),port,info,cid);
					//
					sockaddr_in localaddr;
					//TODO 从path中解析出sock_addr
					sockaddr dest_sockaddr;
				/*	localaddr.sin_family = AF_INET;
					localaddr.sin_port = htons(addr.m_port);
					localaddr.sin_addr.s_addr = htonl(addr.m_ip);*/
					int ret = SendRtpPackage(info->m_call_info.m_session_h,
											 info->m_call_info.m_session_l,cid,buf,
											 recvlen+sizeof(qn_sub_ext_t),&dest_sockaddr,
											 sizeof(sockaddr));	
					SessionID session(info->m_call_info.m_session_h,info->m_call_info.m_session_l,cid);
					int sent_len = 0 ;
					//更新统计
					if(0==ret){			
						sent_len =recvlen+sizeof(qn_sub_ext_t);	
					}
					SessionStatManager::Obj()->update_send_statistic(&session,sent_len);
			}
		}

		XMemPool::dealloc(buf);
		return true;
	}

	//直连方式
	//
	GMAddrEx addr = m_listen_addr;
	addr.m_port = info->m_direct_port;
	//根据发送端口找到SACnnID
	aio::CPID sent_cid = addr_2_cpid(addr);
	//找到发送端口的目的地址
	SptrTFInfo dirct_info = TransformInfoManager::Obj()->get_sptr_tf_info(info->m_direct_port);
	if(!dirct_info.Get()){
		PortStatManager::Obj()->update_send_loss(info->m_direct_port);
		LGE("not find port %u transform info",info->m_direct_port);
		return false;
	}
	dirct_info->m_rw_lock.readLock();
	GMAddrEx dest_addr = dirct_info->m_dest_addr;
	dirct_info->m_rw_lock.unReadLock();
	
	//地址有效，发送数据
	if(dest_addr!=GMAddrEx()){
		GMBUF buf_array(buf,recvlen);
		//发送给用户的只发一次，用户转发的需要发两次
		WriteUdpConnectionCB *cb = NULL;
		if(!dirct_info->m_is_recv_port){
			cb = new WriteUdpConnectionCB(2,buf);
			if(!cb){
				LGE("WriteUdpConnectionCB fail src %s dest_addr %s ",
					GMAddressEx(addr).toStr(),GMAddressEx(dest_addr).toStr());
				return false;
			}
			//更新系统发包量统计
			m_udp_send_package.count_new_value(1);
			asynWriteUdpConnection(SACnnID(SIT_UDP,sent_cid),dest_addr,&buf_array,1,0,(U64)cb);
			Sleep(10);

			//更新系统发包量统计
			m_udp_send_package.count_new_value(1);
			//间隔10ms发送第二次
			asynWriteUdpConnection(SACnnID(SIT_UDP,sent_cid),dest_addr,&buf_array,1,0,(U64)cb);
		}else{
			//包已经存在，不发送
			if(PortStatManager::Obj()->package_exist(port,seq_num)){
				return false;
			}

			//更新系统发包量统计
			m_udp_send_package.count_new_value(1);
			//包不存在，发送包
			asynWriteUdpConnection(SACnnID(SIT_UDP,sent_cid),dest_addr,&buf_array,1,0,(U64)cb);
		}
		
		
		return true;
	}

	//对端地址无效，丢数据
	m_udp_drop_package.count_new_value(1);
	LGW("port %u dest_addr invalidate loss package",info->m_direct_port);				
	PortStatManager::Obj()->update_send_loss(info->m_direct_port);
	return false;
}

void UserReqDealer::on_write_connection(CnnMixerError cme,const SACnnID& id, GMBUF* buf_array,
										U32 array_num,U32 actualLen,U64 ctx1,U64 ctx2)
{
	//更新UDP发送端口统计
	if(SIT_UDP==id.m_tp){
		//发送多次回调
		if(0!=ctx2){
			((IWriteConnectionNotice*)ctx2)->on_write();			
		}else{
			XMemPool::dealloc(buf_array[0].buf);
		}
		int port = cid_2_port(id.m_id);
		PortStatManager::Obj()->update_send_statistic(port,actualLen);
	}else{
		XMemPool::dealloc(buf_array[0].buf);
	}
	
	if (CME_SUC == cme){
        if (NULL != m_ss)
		    m_ss->count_resp(1);
		//更细系统发送数据统计
		if(SIT_TCP==id.m_tp){
			m_tcp_send_speed.count_new_value(actualLen);
		}else{
			m_udp_send_speed.count_new_value(actualLen);
		}
	}else{
		//更新系统发失败包统计
		if(SIT_TCP==id.m_tp){
			m_tcp_send_fail_package.count_new_value(1);
		}else{
			m_udp_send_fail_package.count_new_value(1);
		}
		char buf[1024]={0};
		Log::writeError(LOG_SA,1,"UserReqDealer::on_write_connection->fail to write %s. CnnMixerError:%d ",
                        id.toString(buf,sizeof(buf)),cme);
		if (CME_FATAL_ERROR == cme)
			close_connection(id);
	}
}


void UserReqDealer::get_req_detail( ReqDetail& req_detail )
{
    req_detail.m_get_ports = (U32)m_req_speed[RA_GET_PORTS_REQ].get_speed();
    req_detail.m_free_ports = (U32)m_req_speed[RA_FREE_PORTS_REQ].get_speed();
    req_detail.m_start_call = (U32)m_req_speed[RA_START_CALL_REQ].get_speed();
    req_detail.m_stop_call = (U32)m_req_speed[RA_STOP_CALL_REQ].get_speed();
    req_detail.m_build_path = (U32)m_req_speed[RA_BUILD_PATH_REQ].get_speed();
    req_detail.m_keep_alive = (U32)m_req_speed[RA_KEEP_ALIVE_REQ].get_speed();
}

void UserReqDealer::get_resp_detail( ReqDetail& resp_detail )
{
    resp_detail.m_get_ports = (U32)m_req_speed[RA_GET_PORTS_REQ].get_speed();
    resp_detail.m_free_ports = (U32)m_req_speed[RA_FREE_PORTS_REQ].get_speed();
    resp_detail.m_start_call = (U32)m_req_speed[RA_START_CALL_REQ].get_speed();
    resp_detail.m_stop_call = (U32)m_req_speed[RA_STOP_CALL_REQ].get_speed();
    resp_detail.m_build_path = (U32)m_req_speed[RA_BUILD_PATH_REQ].get_speed();
    resp_detail.m_keep_alive = (U32)m_req_speed[RA_KEEP_ALIVE_REQ].get_speed();
    resp_detail.m_error_call = (U32)m_req_speed[RA_ERROR_CALL_RESP].get_speed();
}

int UserReqDealer::cid_2_port(UID cid)
{
	std::map<aio::CPID,GMAddrEx>::iterator it = m_cpid_2_addr_map.find(cid);
	if(it==m_cpid_2_addr_map.end()){
		//更新系统信息
		LGE("cpid %llu not find in cpid_info,ASSERT!!",cid);
		assert(false);
		return 0;
	}

	return it->second.m_port;
}

void UserReqDealer::deal_error_session(U64 sipproxy_id, const char* call_id)
{
	SACnnID id;
	//从进程保活列表中查找到当前存活的连接，发送
	if(ProcessKeepAliveManager::Obj()->get_cid(sipproxy_id,id)){
		ErrorCallResp resp(&m_listen_addr,call_id);
		char* buf = XMemPool::alloc(PURE_CMD_MAX_SIZE);
		if(!buf){
			LGE("XMemPool::alloc fail");
			return;
		}
		do_back_chores(&resp,buf,id);
	}
}


void record_stat( UserReqDealer* dealer, U32 bt, enum RA_CMD_TYPE type )
{
    U64 duration = GMGetTickCount() - bt;

    dealer->m_average_duration_time[type].count_new_value(duration);
    dealer->m_max_duration_time[type].count_new_value(duration);
}

/*relay_client收到rtp的包
 1.更新接受数据统计
 2.数据去重
 3.根据session找出对应的端口
 4.根据端口信息找到对应的cid和转发地址
 5.去掉包尾
 6.拷贝数据，发送数据
 7.统计发送数据
*/ 

int on_rtp_package_recv(unsigned int hsid,unsigned int lsid,int cid,char* buf,int len,
						 struct sockaddr* addr,int addrlen)
 {
	 RtpHeader* rtp_header = (RtpHeader*)buf;
	 U64 seq_num = rtp_header->seq_number;
	 SessionID session_id(hsid,lsid,cid);
	 //更新session_id的统计
	 SessionStatManager::Obj()->update_recv_statistic(&session_id,seq_num,len);
	 //数据包去重，包存在，则直接返回
	 if(SessionStatManager::Obj()->package_exist(&session_id,seq_num)){
		return 0;
	 }
	 int port = SessionID2PortMap::Obj()->get_port(&session_id);
	 SptrTFInfo info = TransformInfoManager::Obj()->get_sptr_tf_info(port);
	 if(info.Get()){
		 char* sent_buf = XMemPool::alloc(MAX_MEDIA_DATA_SIZE);
		 if(!sent_buf){
			 LGE("XMemPool::alloc fail size %d",MAX_MEDIA_DATA_SIZE);
			 return -1;
		 }

		 len -= sizeof(qn_sub_ext_t);//去掉包尾
		 memcpy(sent_buf,buf,len);
		 GMAddrEx addr = UserReqDealer::Obj()->m_listen_addr;
		 addr.m_port = port;
		 aio::CPID cpid = UserReqDealer::Obj()->addr_2_cpid(addr);
		 GMBUF buf_array(buf,len);
		
		//更新系统发包量统计
		UserReqDealer::Obj()->m_udp_send_package.count_new_value(1);
		UserReqDealer::Obj()->asynWriteUdpConnection(SACnnID(SIT_UDP,cpid),info->m_dest_addr,
						&buf_array,1,0,0);
	 }

	 return 0;
 }