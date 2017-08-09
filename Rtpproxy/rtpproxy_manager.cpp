#include "stdafx.h"
#include "rtpproxy_config.h"
#include "rtpproxy_manager.h"
#include "rtpproxy.h"
#include "user_req_dealer.h"
#include <RelayClient/relayclientexport.h>
#include <RelayClient/RouterCommonDefine.h>
UserInfoContainer::UserInfoContainer(U64 sipproxy_fake_id) :m_sipproxy_fake_id(sipproxy_fake_id){}

bool UserInfoContainer::get_user_info_in_wlock(UserPortInfo& info)
{
	m_rw_lock.writeLock();
	set<UserPortInfo>::iterator it_set = m_user_infos.find(info);
	if(it_set != m_user_infos.end()){
		info= *it_set;
		m_rw_lock.unWriteLock();
		return true;
	}

	bool ret = FreePortPairContainer::Obj()->get_portpair(info.ports);
	if(ret){
		m_user_infos.insert(info);		
	}	
	m_rw_lock.unWriteLock();
	return ret;
}

//�ҵ���Ӧ��UserPortInfo����true�����򣬷���false
//���set�б������Ϣ���û��������Ϣ��һ�£��Ա�����ϢΪ׼��
bool UserInfoContainer::remove_user_info(UserPortInfo& info)
{
	bool ret = false;
	m_rw_lock.writeLock();
	set<UserPortInfo>::iterator it = m_user_infos.find(info);
	if(it!=m_user_infos.end()){
		if((*it).ports!=info.ports){
			info.ports = (*it).ports;
			LGW("UserPortInfo in set different from parm,parm recv_port %d send_port %d, "
				"set recv_port %d send_port %d ",info.ports.rtp_recv_port,info.ports.rtp_sent_port,
				(*it).ports.rtp_recv_port,(*it).ports.rtp_sent_port);
		}
		m_user_infos.erase(it);
		ret = true;
	}	
	m_rw_lock.unWriteLock();
	return ret;
}

bool UserInfoContainer::get_user_info_in_rlock(UserPortInfo& info)
{
	bool ret = false;
	m_rw_lock.readLock();
	set<UserPortInfo>::iterator it_set = m_user_infos.find(info);
	if(it_set != m_user_infos.end()){//�ҵ���Ӧ��user_id
		info = *it_set;
		ret = true;
	}
	m_rw_lock.unReadLock();
	return ret;
}

void TransformInfoManager::modify_transform_info(int port,int cid[3],int cid_num)
{
	m_rw_lock.readLock();
	TFInfoMapIter it = m_transform_info_map.find(port);
	if(it!=m_transform_info_map.end()){
		SptrTFInfo info = it->second;
		m_rw_lock.unReadLock();
		info->modify(cid,cid_num);		
	}else{
		m_rw_lock.unReadLock();
	}
}

void TransformInfoManager::remove_cid(int port,int cid,int& path_num,string& call_id,U64& sipproxy_id)
{
	SptrTFInfo info;
	m_rw_lock.readLock();
	TFInfoMapIter it = m_transform_info_map.find(port);
	if(it!=m_transform_info_map.end()){
		info = it->second;
		m_rw_lock.unReadLock();
		info->remove_cid(cid,path_num,call_id,sipproxy_id);
	}else{
		m_rw_lock.unReadLock();
	}
}

void TransformInfoManager::add_transform_info(int port,SptrTFInfo info)
{
	//����ͨ������ͳ��
	if(info->m_is_recv_port){
		if(info->m_call_info.m_call_type==CT_TRANSFORM){
			InterlockedIncrement((volatile U32*)&m_transform_num);
		}else if(info->m_call_info.m_call_type==CT_DIRECT){
			InterlockedIncrement((volatile U32*)&m_direct_num);
		}else if(info->m_call_info.m_call_type==CT_RTPDIRECT){
			InterlockedIncrement((volatile U32*)&m_rtp_direct_num);
		}
	}
	m_rw_lock.writeLock();
	m_transform_info_map.insert(pair<int,SptrTFInfo>(port,info));
	m_rw_lock.unWriteLock();
}

//���ٽ�·(���)��ȥ����������(һ��);ֻ��recv_port��Ӧ��transform_info���н�·��Ϣ,sent_portֱ���Ƴ�
void TransformInfoManager::del_transform_info(PortPair ports)
{
	m_rw_lock.writeLock();
	TFInfoMapIter it = m_transform_info_map.find(ports.rtp_recv_port);
	if(it!=m_transform_info_map.end()){
		CallInfoInner& call_info = it->second->m_call_info;
		//��ת��ʽ�����ٽ�·(���)��ȥ����������(һ��);
		if(call_info.m_call_type==CT_TRANSFORM){
			for(int i=0;i<3;++i){			
				PathInfoInner& path_info = it->second->m_path_info;			
				//cid������0��ʾ��Ч
				if(0!=path_info.m_cid[i]){
					//�Ƴ���Ӧ��sessionͳ����Ϣ
					SessionID session(call_info.m_session_h,call_info.m_session_l,path_info.m_cid[i]);
					SessionStatManager::Obj()->del_portpair_statistic(&session);

					//���ٽ�·
					DestoryChannel(call_info.m_session_h,call_info.m_session_l,path_info.m_cid[i]);
				}	
			}

			DelShortLinkKeepAlive(call_info.m_session_h,call_info.m_session_l);
			InterlockedDecrement((volatile U32*)&m_transform_num);
		}else if(call_info.m_call_type==CT_DIRECT){
			InterlockedDecrement((volatile U32*)&m_direct_num);
		}else if(call_info.m_call_type==CT_RTPDIRECT){
			InterlockedDecrement((volatile U32*)&m_rtp_direct_num);
		}

		m_transform_info_map.erase(it);
	}
	
	//���Ͷ˿���Ϣֱ���Ƴ�
	m_transform_info_map.erase(ports.rtp_sent_port);
	m_rw_lock.unWriteLock();
}

SptrTFInfo TransformInfoManager::get_sptr_tf_info(int port)
{
	SptrTFInfo info;
	m_rw_lock.readLock();
	TFInfoMapIter it = m_transform_info_map.find(port);
	if(it!=m_transform_info_map.end()){
		info = it->second;
	}
	m_rw_lock.unReadLock();
	return info;
}

bool SipProxyCallManager::add_call_id(U64 sipproxy_id, string& call_id)
{
	m_rw_lock.readLock();
	map<U64,SptrCallIDSet>::iterator it = m_sipproxy_map.find(sipproxy_id);	
	//����ҵ��˶�Ӧ��sipproxy_id����ô��д����Ȼ�����һ����¼
	if(it != m_sipproxy_map.end()){
		SptrCallIDSet call_id_set = it->second;
		m_rw_lock.unReadLock();
		call_id_set->insert(call_id);
		return true;
	}

	//û�ҵ���Ӧ��sipproxy_id����ô�²���һ����¼��sipproxy���̱�
	m_rw_lock.unReadLock();
	m_rw_lock.writeLock();
	it = m_sipproxy_map.find(sipproxy_id);
	if(it != m_sipproxy_map.end()){
		SptrCallIDSet call_id_set = it->second;
		m_rw_lock.unWriteLock();
		call_id_set->insert(call_id);
		return true;
	}
	SptrCallIDSet call_id_set;
	CallIdContainer* container = new CallIdContainer;
	if(container){
		container->m_call_ids.insert(call_id);
		call_id_set.Reset(container);
		m_sipproxy_map.insert(pair<U64,SptrCallIDSet>(sipproxy_id,call_id_set));
	}	
	m_rw_lock.unWriteLock();

	return container!=NULL;
}

void SipProxyCallManager::del_call_id(U64 sipproxy_id, string& call_id)
{
	m_rw_lock.readLock();
	map<U64,SptrCallIDSet>::iterator it_map = m_sipproxy_map.find(sipproxy_id);
	//����ҵ��˶�Ӧ��sipproxy_id���Ƴ���Ӧ��call_id
	if(it_map != m_sipproxy_map.end()){
		SptrCallIDSet call_id_set= it_map->second;
		m_rw_lock.unReadLock();
		call_id_set->erase(call_id);
		return ;
	}
	m_rw_lock.unReadLock();
}


SptrCallIDSet SipProxyCallManager::del_call_id_set(U64 sipproxy_id)
{
	SptrCallIDSet call_id_set;
	m_rw_lock.writeLock();
	map<U64,SptrCallIDSet>::iterator it = m_sipproxy_map.find(sipproxy_id);
	if(it!=m_sipproxy_map.end()){
		call_id_set = it->second;
		m_sipproxy_map.erase(it);
		m_rw_lock.unWriteLock();
		call_id_set->clear();
	}else{
		m_rw_lock.unWriteLock();
	}
	return call_id_set;
}

SptrUIContainer UserPortInfoManager::find_container_in_rlock(U64 sipproxy_id)
{
	SptrUIContainer cnter;
	m_rw_lock.readLock();
	UIContainerMapIter it = m_user_portpair_info.find(sipproxy_id);
	if(it != m_user_portpair_info.end()){
		cnter = it->second;
	}
	m_rw_lock.unReadLock();
	return cnter;
}

bool UserPortInfoManager::get_ports_in_wlock(U64 sipproxy_id,U64 sipproxy_fake_id,UserPortInfo &info)
{	
	SptrUIContainer container;
	m_rw_lock.writeLock();
	UIContainerMapIter it = m_user_portpair_info.find(sipproxy_id);
	if(it != m_user_portpair_info.end()){
		container = it->second;
		m_rw_lock.unWriteLock();
		if(container.Get()){
			if(sipproxy_fake_id==container->m_sipproxy_fake_id){
				//��container�в���user_info
				if(container->get_user_info_in_rlock(info)){
					return true;
				}

				return container->get_user_info_in_wlock(info);
			}		
			//д���ж�����sipproxy���������⣬������ֱ�ӷ���
			LGE("Sipproxy have been reset too often,fake_id %llu new_fake_id %llu ",
				container->m_sipproxy_fake_id, sipproxy_fake_id);
			return false;
		}
	}

	//δ�ҵ�container
	bool ret1=false;
	UserInfoContainer* ctner= new UserInfoContainer(sipproxy_fake_id);
	if(ctner){
		ret1 = FreePortPairContainer::Obj()->get_portpair(info.ports);
		if(ret1){
			ctner->m_user_infos.insert(info);
			container.Reset(ctner);
			m_user_portpair_info.insert(pair<U64,SptrUIContainer>(sipproxy_id,container));
		}else{
			delete ctner;
		}			
	}			
	m_rw_lock.unWriteLock();
	return ret1;
}

bool UserPortInfoManager::del_user_port_info(U64 sipproxy_id, UserPortInfo& info)
{
	m_rw_lock.readLock();
	UIContainerMapIter it = m_user_portpair_info.find(sipproxy_id);	
	if(it != m_user_portpair_info.end()){//�ҵ�sipproxy_id
		m_rw_lock.unReadLock();
		return it->second->remove_user_info(info);
	}

	LGW("not find record :sipproxy_id %llu ",sipproxy_id);
	//δ�ҵ���Ӧ��sipproxy��¼
	m_rw_lock.unReadLock();	
	return false;
}

SptrUIContainer UserPortInfoManager::del_container(U64 sipproxy_id,U64 sipproxy_fake_id)
{
	SptrUIContainer container;
	m_rw_lock.writeLock();
	UIContainerMapIter it= m_user_portpair_info.find(sipproxy_id);
	if(it!=m_user_portpair_info.end()){
		if(it->second->m_sipproxy_fake_id==sipproxy_fake_id||sipproxy_fake_id==0){
			container = it->second;	
			m_user_portpair_info.erase(it);
		}else{
			LGM("container have been del,sipproxy_id %llu fake_id %llu ",sipproxy_id,
				sipproxy_fake_id);
		}
	}		
	m_rw_lock.unWriteLock();
	return container;
}

void TransformInfo::modify(int cid[3],int cid_num)
{
	m_rw_lock.writeLock();
	for(int i=0;i<3;++i){
		m_path_info.m_cid[i]=cid[i];			
	}
	m_path_info.m_path_num = cid_num;
	m_path_status = cid_num>0?PS_SUC:PS_FAIL;
	m_rw_lock.unWriteLock();
}

void TransformInfo::remove_cid(int cid,int& path_num,string& call_id,U64& sipproxy_id)
{
	m_rw_lock.writeLock();
	path_num = m_path_info.m_path_num;
	call_id = m_call_info.m_call_id;
	sipproxy_id = m_sipproxy_id;
	for(int i=0;i<m_path_info.m_path_num;++i){
		if(cid==m_path_info.m_cid[i]){
			path_num--;
			m_path_info.m_cid[i]=0;
			break;
		}
	}
	m_rw_lock.unWriteLock();
}

int ProcessKeepAliveManager::check_dead_sipproxy(GMCustomTimerCallBackType type,void* prama)
{
	SptrKAInfo info;
	std::list<U64> dead_sipproxy;
	m_rw_lock.readLock();
	for(map<U64,SptrKAInfo>::iterator it = m_keep_alive_map.begin();
		it != m_keep_alive_map.end();++it){
			info = it->second;
			if(info->sipproxy_dead()){
				LGE("sipproxy_id %llu dead ",it->first);
				//��¼����sipproxy��������ߴ���
				dead_sipproxy.push_back(it->first);				
			}
	}
	m_rw_lock.unReadLock();

	//��������sipproxy
	for(std::list<U64>::iterator it = dead_sipproxy.begin();it!=dead_sipproxy.end();++it){
		RtpProxy::Obj()->deal_dead_sipproxy(*it,0);
	}
	dead_sipproxy.clear();

	m_dead_process_checker.SetTimer(RtpProxyConfig::Obj()->m_dead_check_interval,this,
		&ProcessKeepAliveManager::check_dead_sipproxy,NULL);
	return 0;
}

bool KeepAliveInfo::sipproxy_dead()
{
	time_t ct;
	time(&ct);
	m_rw_lock.readLock();
	U32 diff = static_cast<U32>(ct-m_last_alive_time);
	if(diff>RtpProxyConfig::Obj()->m_precoess_deadtime){
		m_rw_lock.unReadLock();
		LGE("dead_time %llu s since last keep alive %u s",
			RtpProxyConfig::Obj()->m_precoess_deadtime,diff);
		return true;
	}

	m_rw_lock.unReadLock();
	return false;
}

struct CallSessionReport{
	string call_id;
	PortStatistic recv_port_stat;
	PortStatistic sent_port_stat;
	GMAddrEx report_addr;

	CALL_TYPE call_type;	//call_typeΪ��ת��ʽ�������������������SessionID�����Ҷ�Ӧ��ͳ������
	int session_hi;
	int session_lo;
	int cid[3];
	int num;				//cid��Ч��
};

//10��û��������Ϊsession
int PortStatManager::error_session_check(GMCustomTimerCallBackType type,void* prama)
{
	SptrPortStat port_stat;
	std::list<int> error_session;
	std::list<CallSessionReport> report_list;
	m_rw_lock.readLock();
	for(map<int,SptrPortStat>::iterator it = m_portpair_stat_map.begin();
		it != m_portpair_stat_map.end();++it){
			port_stat = it->second;
			if(port_stat->session_error()){
				LGE("find error session port %u  ",it->first);
				//��¼����sipproxy��������ߴ���
				error_session.push_back(it->first);				
			}else{
				time_t ct ;
				time(&ct);

				//��ȡ�˿�ͳ����Ϣ
				if(ct-port_stat->m_begin_stat_ticktime>=RtpProxyConfig::Obj()->m_report_time){
					CallSessionReport report_info;
					SptrTFInfo port_info = TransformInfoManager::Obj()->get_sptr_tf_info(it->first);
					if(port_info.Get()){		
						//���Ϊ��ת��ʽ��Ҫ��¼��·��Ϣ
						if(report_info.call_type==CT_TRANSFORM){
							//��ת��ʽֻ��Ҫ��¼���ն˿���Ϣ���������Ϣ��Ҫ����SessionID����
							if(port_info->m_is_recv_port){
								report_info.report_addr = port_info->m_call_info.m_report_addr;
								report_info.call_id = port_info->m_call_info.m_call_id;
								report_info.call_type = port_info->m_call_info.m_call_type;

								report_info.recv_port_stat = port_stat->get_statistic();
								report_info.session_hi = port_info->m_call_info.m_session_h;
								report_info.session_lo = port_info->m_call_info.m_session_l;
								for(int i=0;i<3;i++){
									if(port_info->m_path_info.m_cid[i]!=0){
										report_info.cid[i]=port_info->m_path_info.m_cid[i];
										report_info.num++;
									}								
								}
								//��������У��������洦��
								report_list.push_back(report_info);
							}else{
								//do nothing 
							}
						}else{
							report_info.report_addr = port_info->m_call_info.m_report_addr;
							report_info.call_id = port_info->m_call_info.m_call_id;
							report_info.call_type = port_info->m_call_info.m_call_type;
							//�ҵ���Ӧ�˿ڵ����ݣ���䵽CallSessionReport��
							if(port_info->m_is_recv_port){
								report_info.recv_port_stat = port_stat->get_statistic();
							}else{
								report_info.sent_port_stat = port_stat->get_statistic();
							}
							//��������У��������洦��
							report_list.push_back(report_info);
						}
					}

				}
			}
	}
	m_rw_lock.unReadLock();

	//����error session
	for(std::list<int>::iterator it = error_session.begin();it!=error_session.end();++it){
		SptrTFInfo info = TransformInfoManager::Obj()->get_sptr_tf_info(*it);
		if(info.Get()){
			UserReqDealer::Obj()->deal_error_session(info->m_sipproxy_id,
													 info->m_call_info.m_call_id.c_str());
		}		
	}
	error_session.clear();

	//�㱨CallSessionReport
	for(std::list<CallSessionReport>::iterator it = report_list.begin();it!=report_list.end();++it){
		//TODO �������ת��ʽ��Ҫ����SessionID�ҵ���Ӧ��ͳ����Ϣ
	}
	report_list.clear();

	m_error_session_checker.SetTimer(RtpProxyConfig::Obj()->m_error_call_check_interval,this,
									 &PortStatManager::error_session_check,NULL);
	return 0;
}

bool RtpProxyManager::start_rtp_proxy()
{
	GMAutoLock<GMLock> al(&m_start_lock);
	if(m_is_running){
		return true;
	}
	Log::open(true,(char*)"-dGMfyds",false);

	if(!RtpProxyConfig::Obj()->parse()){
		LGE("RtpProxyConfig::Obj()->parse() fail");
		return false;
	}

	if(!ProcessKeepAliveManager::Obj()->start()){
		LGE("ProcessKeepAliveManager::Obj()->start()");
		return false;
	}
	
	if(!PortStatManager::Obj()->start()){
		LGE("PortStatManager::Obj()->start()");
		goto E0;
	}

	if(!UserReqDealer::Obj()->start()){
		LGE("UserReqDealer::Obj()->start fail");
		goto E1;
	}

	m_is_running = true;
	return true;

E1:
	PortStatManager::Obj()->stop();
E0:
	ProcessKeepAliveManager::Obj()->stop();
	return false;
}

void RtpProxyManager::stop_rtp_proxy()
{
	GMAutoLock<GMLock> al(&m_start_lock);
	if(!m_is_running){
		return ;
	}
	m_is_running = false;
	UserReqDealer::Obj()->stop();
	PortStatManager::Obj()->stop();
	ProcessKeepAliveManager::Obj()->stop();
	Log::close();
}

extern "C" RTP_PROXY_API bool start_rtp_proxy()
{
	return RtpProxyManager::Obj()->start_rtp_proxy();
}

extern "C" RTP_PROXY_API void stop_rtp_proxy()
{
	return RtpProxyManager::Obj()->stop_rtp_proxy();
}