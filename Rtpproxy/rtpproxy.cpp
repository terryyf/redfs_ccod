#include "stdafx.h"
#include "rtpproxy.h"
#include "user_req_dealer.h"
#include "rtpproxy_config.h"
#include <RelayClient/relayclientexport.h>
#include <RelayClient/RouterCommonDefine.h>

bool RtpProxy::get_ports(U64 sipproxy_id,U64 sipproxy_fake_id,UserPortInfo &info)
{
	SptrUIContainer container= UserPortInfoManager::Obj()->find_container_in_rlock(sipproxy_id);
	//�ҵ���Ӧ��container	
	if(container.Get()){
		if(sipproxy_fake_id==container->m_sipproxy_fake_id){
			//��container�в���user_info
			if(container->get_user_info_in_rlock(info)){
				return true;
			}
			return container->get_user_info_in_wlock(info);
		}		

		//����sipproxy����
		deal_dead_sipproxy(sipproxy_id,container->m_sipproxy_fake_id);
		//���¿�ʼ����
	}

	return  UserPortInfoManager::Obj()->get_ports_in_wlock(sipproxy_id,sipproxy_fake_id,info);
}

void RtpProxy::free_ports(U64 sipproxy_id,UserPortInfo& info)
{
	if(UserPortInfoManager::Obj()->del_user_port_info(sipproxy_id,info)){
		FreePortPairContainer::Obj()->add_portpair(info.ports);
	}else{
		LGW("not find record: sipproxy_id %llu user_id %d recv_port %d send_port %d",sipproxy_id,
			info.user_id,info.ports.rtp_recv_port,info.ports.rtp_sent_port);
	}	
}

/*ֻ�б���ʧ���˲Ż���ã�ctx�Ƕ˿���Ϣ��
  ���ݶ˿���Ϣ��TransformInfoMap���ҵ�cid��Ϣ�����ýӿ��Ƴ�ʧ�ܵ�cid������m_path_numֵ
  �������cid��ʧ���ˣ���֪ͨsipproxy*/

void on_channel_keep_alive(unsigned int sid_hi,unsigned int sid_lo,int cid,U64 ctx,int reason)
{
	int port = (int)ctx;
	int path_num=0;
	string call_id;
	U64 sipproxy_id;
	TransformInfoManager::Obj()->remove_cid(port,cid,path_num,call_id,sipproxy_id);
	//�Ƴ���Ӧ��sessionͳ����Ϣ
	SessionID session(sid_hi,sid_lo,cid);
	SessionStatManager::Obj()->del_portpair_statistic(&session);

	//�Ƴ�session���˿ڵ�ӳ��
	SessionID2PortMap::Obj()->del(&session);

	//ȥ��SessionKeepAlive
	DelSessionKeepAlive(sid_hi,sid_lo,cid);

	//DestoryChannel
	DestoryChannel(sid_hi,sid_lo,cid);
	LGW("sid_hi %d sid_lo %d cid %d reason %d",sid_hi,sid_lo,cid,reason);
	//����·����ʧ���ˣ�֪ͨsipproxy�Ựʧ��
	if(0==path_num){
		UserReqDealer::Obj()->deal_error_session(sipproxy_id, call_id.c_str());
		return;
	}
}


class BuildPathCB{
public:
	BuildPathCB(int path_num,PortPair ports,IRPBuildPathCB* cb)
		:m_suc_num(0),m_path_num(path_num),m_ports(ports),m_cb(cb){
			memset(m_cid,0,sizeof(int)*3);
	}
	void on_build_path(unsigned int sid_hi,unsigned int sid_lo,int cid,int reason){
		SessionID session(sid_hi,sid_lo,cid);
		if(0==reason){
			m_lock.lock();
			m_cid[m_suc_num++]=cid;
			m_lock.unlock();
			//����session���˿ڵ���Ϣ��¼
			SessionID2PortMap::Obj()->add(&session,m_ports.rtp_recv_port);
			//���ӱ���
			AddSessionKeepAlive(sid_hi,sid_lo,cid,on_channel_keep_alive,m_ports.rtp_recv_port);
		}else{
			//��·ʧ�ܣ�ɾ����Ӧsession��ͳ��
			SessionStatManager::Obj()->del_portpair_statistic(&session);
		}

		//���лص�����ɣ�����·���
		if(0==InterlockedDecrement((volatile int*)&m_path_num)){
			
			if(0!=m_suc_num){//��·�ɹ�
				//ֻ���½��ն˿ڵ���Ϣ����
				TransformInfoManager::Obj()->modify_transform_info(m_ports.rtp_recv_port,m_cid,m_suc_num);
			}

			//֪ͨsipproxy��·���
			m_cb->on_build_path(RA_SUC,m_suc_num>0);
			delete this;			
		}
	}
	GMLock m_lock;
	int m_path_num;
	int m_cid[3];
	int m_suc_num;
	PortPair m_ports;
	IRPBuildPathCB* m_cb;
};

void on_build_path(unsigned int sid_hi,unsigned int sid_lo,int cid,U64 ctx,int reason){
	BuildPathCB* cb = (BuildPathCB*)ctx;

	cb->on_build_path(sid_hi,sid_lo,cid,reason);
}

void RtpProxy::asyn_start_call(U64 sipproxy_id,int user_id, PortPair ports,CallInfoInner* call_info,
							  PathInfoInner* path_info, ShortLinkInfo* sl_info,IRPBuildPathCB* cb)
{
	string call_id(call_info->m_call_id);
	if(!SipProxyCallManager::Obj()->add_call_id(sipproxy_id,call_id)){
		return cb->on_build_path(RA_ALLOC_MEMORY_FAILED,false);
	}

	SptrTFInfo sptr_info;
	TransformInfo* info= new TransformInfo(ports.rtp_sent_port,sipproxy_id,call_info,
										   path_info,sl_info,true);
	if(info){
		sptr_info.Reset(info);
		TransformInfoManager::Obj()->add_transform_info(ports.rtp_recv_port,sptr_info);
		TransformInfo* info1= new TransformInfo(ports.rtp_recv_port,sipproxy_id,call_info,
												path_info,sl_info,false);
		if(info1){
			sptr_info.Reset(info1);
			TransformInfoManager::Obj()->add_transform_info(ports.rtp_sent_port,sptr_info);
		}else{
			TransformInfoManager::Obj()->del_transform_info(ports);
			LGE(" new TransformInfo fail call_id %s call_type %d recv_port %d send_port %d",
				call_info->m_call_id,call_info->m_call_type,ports.rtp_recv_port,ports.rtp_sent_port);
			return cb->on_build_path(RA_ALLOC_MEMORY_FAILED,false);
		}		
	}else{
		LGE(" new TransformInfo fail call_id %s call_type %d recv_port %d send_port %d",
			call_info->m_call_id,call_info->m_call_type,ports.rtp_recv_port,ports.rtp_sent_port);
		return cb->on_build_path(RA_ALLOC_MEMORY_FAILED,false);
	}

	//��ת��ʽ��Ҫ��������ͽ�·
	if(call_info->m_call_type==CT_TRANSFORM){
		//��������
		if(sl_info->relay_num>0){
			for(int i=0;i<sl_info->relay_num;++i){
				GMAddressEx addr(sl_info->relay_addr[i]);
				AddShortLinkKeepAlive(call_info->m_session_h,call_info->m_session_l,2, addr.ip(),
					addr.m_addr.m_port);
			}
		}

		//��ʼ����·
		if(path_info->m_path_num>0){
			asyn_build_path_inner(call_info,path_info, ports, cb);
		}
	}else{
		//����ת��ʽֱ��֪ͨ�ɹ�
		cb->on_build_path(RA_SUC,true);
	} 
}

//build_path��start_call֮��������һ������TransformInfo�ļ�¼
void RtpProxy::asyn_build_path(int user_id,PortPair ports,CallInfoInner* call_info,
								PathInfoInner* path_info,IRPBuildPathCB* cb)
{
	if(path_info->m_path_num>0){
		asyn_build_path_inner(call_info,path_info, ports, cb);
	}else{
		cb->on_build_path(RA_BAD_PARAMETER,false);
	}
}

void RtpProxy::asyn_build_path_inner(CallInfoInner* call_info,PathInfoInner* path_info,
									 PortPair ports, IRPBuildPathCB* cb)
{
	BuildPathCB* ctx = new BuildPathCB(path_info->m_path_num,ports,cb);
	if(ctx){
		for(int i=0;i<path_info->m_path_num;++i){
			string path = path_info->m_path_info[i].c_str();
			ShortLink short_link(path);
			BuildChannel(call_info->m_session_h,call_info->m_session_l, path_info->m_cid[i],
				&short_link,call_info->m_caller_appkey,call_info->m_callee_appkey,
				call_info->m_call_id,call_info->m_is_caller, on_build_path,(U64)ctx);

			//TO DEL ģ��ص�
			on_build_path(call_info->m_session_h,call_info->m_session_l, path_info->m_cid[i],(U64)ctx,0);
		}
	}else{
		cb->on_build_path(RA_ALLOC_MEMORY_FAILED,false);
	}
}

void RtpProxy::deal_dead_sipproxy(U64 sipproxy_id,U64 sipproxy_fake_id)
{
	//�����sipproxy�ϵ�����call_id��Ϣ
	SipProxyCallManager::Obj()->del_call_id_set(sipproxy_id);
	SptrUIContainer container=UserPortInfoManager::Obj()->del_container(sipproxy_id,sipproxy_fake_id);
	if(container.Get()){
		for(set<UserPortInfo>::iterator it = container->m_user_infos.begin();
			it!=container->m_user_infos.end();++it){
				//���ٽ�·,ȥ����������Ƴ�sessionͳ�ƣ��Ƴ��˿ڶ���Ϣ
				TransformInfoManager::Obj()->del_transform_info((*it).ports);
				//�Ƴ���Ӧ�Ķ˿�ͳ����Ϣ
				PortStatManager::Obj()->del_portpair_statistic((*it).ports);
				
				//�ͷŶ˿�,��PortPair���뵽�����б���
				free_ports(sipproxy_id,(UserPortInfo)(*it));
		}
	
		//���UserInfo
		container->m_user_infos.clear();
	}
}

void RtpProxy::stop_call(U64 sipproxy_id, U64 user_id, PortPair ports, CallInfoInner* call_info)
{
	string call_id(call_info->m_call_id);
	SipProxyCallManager::Obj()->del_call_id(sipproxy_id,call_id);
	//���ٽ�·,ȥ����������Ƴ�sessionͳ�ƣ��Ƴ��˿ڶ���Ϣ
	TransformInfoManager::Obj()->del_transform_info(ports);
	//�Ƴ���Ӧ�Ķ˿�ͳ����Ϣ
	PortStatManager::Obj()->del_portpair_statistic(ports);
}
