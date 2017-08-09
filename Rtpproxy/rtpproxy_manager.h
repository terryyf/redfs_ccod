#ifndef RTPPROXY_DEFINITIONS_H
#define RTPPROXY_DEFINITIONS_H

#include <BaseLibrary/GMHelper/gmhelperLibPub.h>
#include "user_definitions.h"
#include "rtpproxy_cmd.h"
#include "XTools/XToolsPub.h"
#include <list>
#include <map>
#include <set>
#include <bitset>
#include "sa_cnn_id.h"
/*数据包去重
	只在一定范围内序号去重
*/
const int MAX_UNIQ_RANGE = 100;
class PackageUniq{
public:
	PackageUniq():m_min_seq_num(0),m_max_seq_num(MAX_UNIQ_RANGE-1){}
	
	//过滤统计周期内相同包过滤，统计周期为2*MAX_UNIQ_RANGE

	//false：表示seq_num不存在，true：表示seq_num已存在
	bool package_exist(U64 seq_num){
		bool ret = true;
		int pos = seq_num%MAX_UNIQ_RANGE;
		m_lock.lock();
		if(seq_num>m_min_seq_num&&seq_num<m_max_seq_num){//处在当前统计周期内
			if(!m_bitset.test(pos)){
				m_bitset.set(pos);
				ret = false;
			}
		}else if(seq_num>m_max_seq_num){
		//seq_num大于最大值，超过统计范围，更新统计的最小，最大包号，记录老的统计值，生产新的统计值
			m_min_seq_num = m_max_seq_num+1;
			m_max_seq_num +=MAX_UNIQ_RANGE;
			m_last_bitset = m_bitset;//记录老的统计值
			m_bitset.reset();
			if(!m_bitset.test(pos)){
				m_bitset.set(pos);
				ret = false;
			}
		}else if(seq_num <m_min_seq_num){
			//seq_num处于上一个统计范围内，在老的统计值中查找更新
			if(seq_num>m_min_seq_num-MAX_UNIQ_RANGE){
				if(!m_last_bitset.test(pos)){
					m_last_bitset.set(pos);
					ret = false;
				}
			}else{
				//超过一个老的统计周期范围，不处理
			}		
		}
		m_lock.unlock();
		return ret;
	}
	U64 m_max_seq_num;
	U64 m_min_seq_num;
	GMLock m_lock;
	std::bitset<MAX_UNIQ_RANGE> m_bitset;
	/*保存上一个统计周期的bit信息，为了避免包乱序导致的去重失败；eg:min=0,range=100,max = 99;
	seq_num=80设置成功后，下一个seq_num=101，此时会更新min=100,max=200,m_bitset重新初始化为0；
	为了防止seq_num81---100去重失败，保留上一次的bitset的值;*/
	std::bitset<MAX_UNIQ_RANGE> m_last_bitset;
};
enum PathStatus{
	PS_BUILDING,
	PS_SUC,
	PS_FAIL
};

struct PortStatistic{
	PortStatistic(){
		memset(this,0,sizeof(PortStatistic));
	}
	PortStatistic(U32 total_recv_num,U32 total_recv_size,U32 loss_recv_num,U32 loss_recv_rate,
				  U32 unordered_rate,U32 total_send_num,U64 total_send_size,U32 loss_send_num,
				  U32 fail_send_num)
	:m_total_recv_num(total_recv_num),m_total_recv_size(total_recv_size),
	 m_loss_recv_num(loss_recv_num),m_loss_recv_rate(loss_recv_rate),
	 m_unordered_rate(unordered_rate),m_total_send_package(total_send_num),
	 m_total_send_size(total_send_size),m_loss_package(loss_send_num),
	 m_send_fail_num(fail_send_num){}

U32 m_total_recv_num;		//收包数
U32 m_loss_recv_num;		//接收丢包数
U32 m_loss_recv_rate;		//接受丢包率
U32 m_unordered_rate;		//接收乱序率
U64 m_total_recv_size;		//收包量

U32 m_total_send_package;	//发包数
U64 m_total_send_size;		//发包量
U32 m_loss_package;			//发送主动丢包数
U32 m_send_fail_num;		//发送失败数
};

const int ERROR_SESSION_TIME = 10;
class PortPairStatistic:public GMRefCounterBase<PortPairStatistic>{
public:
	PortPairStatistic()
	:m_max_seq_num(0),m_min_seq_num(0),m_total_recv_package(0),m_unordered_num(0)
	{
		time(&m_last_recv_ticktime);
		time(&m_begin_stat_ticktime);
	}

	bool package_exist(U64 seq_num){
		return m_package_uniq.package_exist(seq_num);
	}

	bool session_error(){
		time_t ct;
		time(&ct);
		m_rw_lock.readLock();
		int diff = ct - m_last_recv_ticktime;
		m_rw_lock.unReadLock();
		if(diff>ERROR_SESSION_TIME){
			LGE("session fail %d s not recv data");
			return true;
		}
		return false;
	}

	//更新统计信息；参数seq_num:收到包的序号
	void update_recv_statistic(U64 seq_num,U32 recv_len){
		m_rw_lock.writeLock();

		time(&m_last_recv_ticktime);//更新上一次接收时间
		InterlockedIncrement((U32 volatile*)&m_total_recv_package);
		InterlockedAdd64((LONGLONG volatile*)&m_total_recv_size,recv_len);
		if(0==m_min_seq_num&&m_min_seq_num==m_max_seq_num){
			m_min_seq_num = seq_num;
			m_max_seq_num = seq_num;
			m_rw_lock.unWriteLock();
			return ;
		}

		//不等于当前最大包号的下一个，则认为乱序
		if(seq_num<m_max_seq_num+1){
			m_unordered_num++;
		}else{
			m_max_seq_num=seq_num;
		}
		m_rw_lock.unWriteLock();
	}

	//获取统计信息，包括收到包总数，丢包数，丢包率，乱序率
	PortStatistic get_statistic()
	{
		m_rw_lock.writeLock();
		U32 loss_recv_num = m_max_seq_num - m_min_seq_num-m_total_recv_package;

		U32 loss_rate = loss_recv_num*100/(m_max_seq_num-m_min_seq_num);
		U32 unordered_rate = m_unordered_num*100/m_total_recv_package;
		
		PortStatistic info(m_total_recv_package,m_total_recv_size,loss_recv_num,loss_rate,unordered_rate,
						   m_total_send_package,m_total_send_size,m_loss_package,m_send_fail_num);
		//更新时间
		time(&m_begin_stat_ticktime);

		m_total_recv_package = 0;
		m_unordered_num =0;
		m_min_seq_num = m_max_seq_num;
		m_total_send_size=0;
		m_total_send_package =0;
		m_loss_package=0;
		m_send_fail_num=0;
		m_rw_lock.unWriteLock();
		return info;
	}

	//更新发送统计，send_len==0表示发送失败
	void update_send_statistic(U32 send_len){
			m_rw_lock.writeLock();
			InterlockedIncrement((U32 volatile*)&m_total_send_package);
			if(0==send_len){
				InterlockedIncrement((U32 volatile*)&m_send_fail_num);
			}else{
				InterlockedAdd64((LONGLONG volatile*)&m_total_send_size,send_len);
			}
			m_rw_lock.unWriteLock();
		}

	void update_send_loss(){
		 InterlockedIncrement((U32 volatile*)&m_loss_package);
	}

	U32 m_total_recv_package;	//收到总的包数
	U64 m_total_recv_size;		//收包总量
	U32 m_unordered_num;		//乱序包数

	U64 m_max_seq_num;			//当前最大包序列号
	U64 m_min_seq_num;			//当前最小包序列号

	U32 m_total_send_package;	//发包数
	U64 m_total_send_size;		//发包量
	U32 m_loss_package;			//丢包数
	U32 m_send_fail_num;		//发送失败数
	time_t m_begin_stat_ticktime;	//开始统计的时间，统计后更新该时间

	PackageUniq m_package_uniq;	//相同包过滤
	time_t m_last_recv_ticktime;	//上一次收到包的时间；根据该时间和m_max_seq_num
	GMRWLock m_rw_lock;
};
typedef GMEmbedSmartPtr<PortPairStatistic> SptrPortStat;
struct SessionID{
	SessionID(U32 sid_hi,U32 sid_lo,int cid):m_sid_hi(sid_hi),m_sid_lo(sid_lo),m_cid(cid){}
	bool operator<(const SessionID& r)const
	{
		if(m_sid_hi<r.m_sid_hi)
			return true;
		if(m_sid_hi>r.m_sid_hi)
			return false;

		//m_sid_hi相同，比较m_sid_lo
		if(m_sid_lo<r.m_sid_lo)
			return true;
		if(m_sid_lo>r.m_sid_lo)
			return false;

		return m_cid<r.m_cid;
	}
	U32 m_sid_hi;
	U32 m_sid_lo;
	int m_cid ;
};

class SessionStatManager: public GMSingleTon<SessionStatManager>{
public:
	bool package_exist(SessionID* session,U64 seq_num){
		SptrPortStat port_stat = get_sptr_port_stat(session);
		if(port_stat.Get()){
			return port_stat->package_exist(seq_num);
		}
		return false;
	}

	void update_recv_statistic(SessionID* session,U64 seq_num,U32 recv_len){
		SptrPortStat port_stat = get_sptr_port_stat(session);
		if(port_stat.Get()){
			port_stat->update_recv_statistic(seq_num,recv_len);
		}
	}
	void update_send_statistic(SessionID* session,U32 send_len)
	{
		SptrPortStat port_stat = get_sptr_port_stat(session);
		if(port_stat.Get()){
			port_stat->update_send_statistic(send_len);
		}
	}

	void update_send_loss(SessionID* session){
		SptrPortStat port_stat = get_sptr_port_stat(session);
		if(port_stat.Get()){
			port_stat->update_send_loss();
		}
	}

	PortStatistic get_session_stat(SessionID* session){
		SptrPortStat port_stat = get_sptr_port_stat(session);
		if(port_stat.Get()){
			return port_stat->get_statistic();
		}
		return PortStatistic();
	}

	void del_portpair_statistic(SessionID* session)
	{
		m_rw_lock.writeLock();
		m_session_stat_map.erase(*session);
		m_rw_lock.unWriteLock();
	}

	SptrPortStat get_sptr_port_stat(SessionID* session){
		SptrPortStat session_stat;
		m_rw_lock.readLock();
		map<SessionID,SptrPortStat>::iterator it = m_session_stat_map.find(*session);
		if(it!=m_session_stat_map.end()){
			session_stat = it->second;
			m_rw_lock.unReadLock();
		}else{
			m_rw_lock.unReadLock();
			m_rw_lock.writeLock();
			it = m_session_stat_map.find(*session);
			if(it!=m_session_stat_map.end()){
				session_stat = it->second;
			}else{
				PortPairStatistic* stat = new PortPairStatistic;
				if(stat){
					session_stat.Reset(stat);
					m_session_stat_map.insert(make_pair(*session,session_stat));
				}
			}
			m_rw_lock.unWriteLock();
		}
		return session_stat;
	}

	GMRWLock m_rw_lock;
	map<SessionID,SptrPortStat> m_session_stat_map;
};

class SessionID2PortMap:public GMSingleTon<SessionID2PortMap>
{
public:
	void add(SessionID* session,int port){
		 m_rw_lock.writeLock();
		 m_session_to_port_map.insert(make_pair(*session,port));
		 m_rw_lock.unWriteLock();
		}

	int get_port(SessionID* sesion){
			int port=0;
			m_rw_lock.readLock();
			map<SessionID,int>::iterator it = m_session_to_port_map.find(*sesion);
			if(it!=m_session_to_port_map.end()){
				port = it->second;
			}
			m_rw_lock.unReadLock();
			return port;
		}

	void del(SessionID* session){
		 m_rw_lock.writeLock();
		 m_session_to_port_map.erase(*session);
		 m_rw_lock.unWriteLock();
		}

GMRWLock m_rw_lock;
map<SessionID,int> m_session_to_port_map;
};

class PortStatManager : public GMSingleTon<PortStatManager>
{
public:
	PortStatManager(){}
	bool start(){		
		m_error_session_checker.Enable();
		return true;
	}
	void stop(){
		m_error_session_checker.Disable();
	}

	bool package_exist(int port,U64 seq_num){
		if(0==port){
			return true;
		}

		SptrPortStat port_stat = get_sptr_port_stat(port);
		if(port_stat.Get()){
			return port_stat->package_exist(seq_num);
		}
		return false;
	}

	void update_recv_statistic(int port,U64 seq_num,U32 recv_len){
		if(0==port){
			return ;
		}

		SptrPortStat port_stat = get_sptr_port_stat(port);
		if(port_stat.Get()){
			port_stat->update_recv_statistic(seq_num,recv_len);
		}
	}

	void update_send_statistic(int port,U32 send_len)
	{
		if(0==port){
			return ;
		}

		SptrPortStat port_stat = get_sptr_port_stat(port);
		if(port_stat.Get()){
			port_stat->update_send_statistic(send_len);
		}
	}

	void update_send_loss(int port){
		if(0==port){
			return ;
		}

		SptrPortStat port_stat = get_sptr_port_stat(port);
		if(port_stat.Get()){
			port_stat->update_send_loss();
		}
	}

	PortStatistic get_port_stat(int port){
		SptrPortStat port_stat = get_sptr_port_stat(port);
		if(port_stat.Get()){
			return port_stat->get_statistic();
		}
		return PortStatistic();
	}

	void del_portpair_statistic(PortPair ports)
	{
		m_rw_lock.writeLock();
		m_portpair_stat_map.erase(ports.rtp_recv_port);
		m_portpair_stat_map.erase(ports.rtp_sent_port);
		m_rw_lock.unWriteLock();
	}

	SptrPortStat get_sptr_port_stat(int port){
		SptrPortStat port_stat;
		if(0==port){
			return port_stat;
		}
		m_rw_lock.readLock();
		map<int,SptrPortStat>::iterator it = m_portpair_stat_map.find(port);
		if(it!=m_portpair_stat_map.end()){
			port_stat = it->second;
			m_rw_lock.unReadLock();
		}else{
			m_rw_lock.unReadLock();
			m_rw_lock.writeLock();
			it = m_portpair_stat_map.find(port);
			if(it!=m_portpair_stat_map.end()){
				port_stat = it->second;
			}else{
				PortPairStatistic* stat = new PortPairStatistic;
				if(stat){
					port_stat.Reset(stat);
					m_portpair_stat_map.insert(make_pair(port,port_stat));
				}
			}
			m_rw_lock.unWriteLock();
		}
		return port_stat;
	}

private:
	int error_session_check(GMCustomTimerCallBackType type,void* prama);
private:	
	GMCustomTimer<PortStatManager> m_error_session_checker;
	map<int,SptrPortStat> m_portpair_stat_map;
	GMRWLock m_rw_lock;
};


class CallIdContainer:public GMRefCounterBase<CallIdContainer>
{
public:
	void insert(string& call_id){
		m_rw_lock.writeLock();
		m_call_ids.insert(call_id);
		m_rw_lock.unWriteLock();
	}

	void erase(string& call_id){
		m_rw_lock.writeLock();
		m_call_ids.erase(call_id);
		m_rw_lock.unWriteLock();
	}
	void clear(){
		m_rw_lock.writeLock();
		m_call_ids.clear();
		m_rw_lock.unWriteLock();
	}
	set<string> m_call_ids;
	GMRWLock m_rw_lock;
};

class KeepAliveInfo : public GMRefCounterBase<KeepAliveInfo>
{
public:
	void update_keep_alive_info_wlock(SACnnID cid)
	{
		m_rw_lock.writeLock();
		time(&m_last_alive_time);
		m_cid = cid;
		m_rw_lock.unWriteLock();
	}

	KeepAliveInfo(SACnnID cid):m_cid(cid){
		 time(&m_last_alive_time);
	}

	SACnnID get_cid()
	{
		SACnnID tmp;
		m_rw_lock.readLock();
		tmp = m_cid;
		m_rw_lock.unReadLock();
		return tmp;
	}

	bool sipproxy_dead();
private:
	time_t m_last_alive_time;
	SACnnID m_cid;
	GMRWLock m_rw_lock;
};
typedef GMEmbedSmartPtr<KeepAliveInfo> SptrKAInfo;

class ProcessKeepAliveManager : public GMSingleTon<ProcessKeepAliveManager>
{
public:
	bool start(){
		m_dead_process_checker.Enable();
		return true;
	}
	void stop()
	{
		m_dead_process_checker.Disable();
	}
	int check_dead_sipproxy(GMCustomTimerCallBackType type,void* prama);

	SptrKAInfo find_keep_alive_info(U64 sipproxy_id)
	{
		SptrKAInfo info;
		map<U64,SptrKAInfo>::iterator it = m_keep_alive_map.find(sipproxy_id);
		if(it != m_keep_alive_map.end()){
			info = it->second;
		}
		return info;
	}

	bool update(U64 sipproxy_id,SACnnID cid)
	{
		bool ret = false;
		SptrKAInfo sptr_info;
		m_rw_lock.readLock();
		sptr_info = find_keep_alive_info(sipproxy_id);
		if(sptr_info.Get()){
			m_rw_lock.unReadLock();
			sptr_info->update_keep_alive_info_wlock(cid);
			return true;
		}
		m_rw_lock.unReadLock();
		
		m_rw_lock.writeLock();
		sptr_info = find_keep_alive_info(sipproxy_id);
		if(sptr_info.Get()){
			m_rw_lock.unWriteLock();
			sptr_info->update_keep_alive_info_wlock(cid);
			return true;
		}

		KeepAliveInfo *info = new KeepAliveInfo(cid);
		if(info){
			sptr_info.Reset(info);
			m_keep_alive_map.insert(pair<U64,SptrKAInfo>(sipproxy_id,sptr_info));
			m_rw_lock.unWriteLock();
			return true;
		}
		m_rw_lock.unWriteLock();
		return false;
	}

	bool get_cid(U64 sipproxy_id, SACnnID &cid)
	{
		m_rw_lock.readLock();
		map<U64,SptrKAInfo>::iterator it = m_keep_alive_map.find(sipproxy_id);
		if(it != m_keep_alive_map.end()){
			SptrKAInfo sptr = it->second;
			m_rw_lock.unReadLock();
			cid = sptr->get_cid();
			return true;
		}
		m_rw_lock.unReadLock();
		return false;
	}
private:
	map<U64,SptrKAInfo> m_keep_alive_map;
	GMRWLock m_rw_lock;
	GMCustomTimer<ProcessKeepAliveManager> m_dead_process_checker; 
};

class TransformInfo:public GMRefCounterBase<TransformInfo>{
public:
	TransformInfo(U32 direct_port,U64 sipproxy_id,CallInfoInner* call_info,PathInfoInner* path_info,
				  ShortLinkInfo* sl_info,bool is_recv_port )
		:m_direct_port(direct_port),m_sipproxy_id(sipproxy_id),m_path_status(PS_BUILDING),
		 m_sl_info(*sl_info),m_is_recv_port(is_recv_port),m_seq_num(0){
			m_path_info = *path_info;
			m_call_info = *call_info;
			 //rtp直连方式将m_dest_rtp_addr赋值给发送端口的m_dest_addr
			 if(!m_is_recv_port&&m_call_info.m_call_type==CT_RTPDIRECT){
				 m_dest_addr = m_call_info.m_dest_rtp_addr;
			 }
	}

	U32 get_seq_num(){
		return InterlockedIncrement(&m_seq_num);
	}

	void modify(int cid[3],int cid_num);

	void remove_cid(int cid,int& path_num,string& call_id,U64& sipproxy_id);

	U32 m_direct_port;              //直通端口对 rtp_send_port
	GMAddrEx m_dest_addr;           //转发地址,收到媒体数据才记录和更新该地址
	U32 m_ssrc_id;					//源id，转发地址可能会更新，ssrc_id不会改变；
	PathStatus m_path_status;       //建路状态,只要有一条建路成功，都认为成功；
	U64 m_sipproxy_id;
	CallInfoInner m_call_info;		//呼叫信息，call_type决定是否建路和保活
	PathInfoInner m_path_info;		//path_info不需要TOFIX
	ShortLinkInfo m_sl_info;
	bool m_is_recv_port;			//是否是接受消息端口，只有接收消息端口记录和更新建路信息
	U32 volatile m_seq_num;			//发送端的发包序号，从0开始，每发送1个依次递增
	GMRWLock m_rw_lock;
};

typedef GMEmbedSmartPtr<TransformInfo> SptrTFInfo;
typedef map<int,SptrTFInfo> TFInfoMap;
typedef map<int,SptrTFInfo>::iterator TFInfoMapIter;
class TransformInfoManager : public GMSingleTon<TransformInfoManager>
{
public:
	TransformInfoManager():m_transform_num(0),m_direct_num(0),m_rtp_direct_num(0){}

	SptrTFInfo get_sptr_tf_info(int port);
	void add_transform_info(int port, SptrTFInfo info);
	void del_transform_info(PortPair port);
	void modify_transform_info(int port,int cid[3],int cid_num);
	void remove_cid(int port,int cid,int& path_num,string& call_id,U64& sipproxy_id);
private:
	TFInfoMap m_transform_info_map; //这里int代表的是rtp_receive_port和rtp_send_port
	U32 m_transform_num;	//中转数
	U32 m_direct_num;		//直连数
	U32 m_rtp_direct_num;	//中转直连方数
	GMRWLock m_rw_lock;
};

typedef GMEmbedSmartPtr<CallIdContainer> SptrCallIDSet;
class SipProxyCallManager : public GMSingleTon<SipProxyCallManager>
{
public:
	bool add_call_id(U64 sipproxy_id, string& call_id);
	void del_call_id(U64 sipproxy_id, string& call_id);
	SptrCallIDSet del_call_id_set(U64 sipproxy_id);
private:
	map<U64,SptrCallIDSet> m_sipproxy_map;
	GMRWLock m_rw_lock;
};

struct UserPortInfo{
	U64 user_id;
	PortPair ports; 

	bool operator<(const UserPortInfo & right)const{
		if(this->user_id == right.user_id) //根据user_id去重
			return false;
		return this->user_id < right.user_id; //升序
	}
	bool operator>(const UserPortInfo & right)const{
		if(this->user_id == right.user_id) //根据user_id去重
			return false;
		return this->user_id > right.user_id;
	}
};

class UserInfoContainer:public GMRefCounterBase<UserInfoContainer> 
{
public:
	UserInfoContainer(U64 sipproxy_fake_id);
	bool get_user_info_in_wlock(UserPortInfo& info);
	bool get_user_info_in_rlock(UserPortInfo& info);
	bool remove_user_info(UserPortInfo& info);

	set<UserPortInfo> m_user_infos;
	GMRWLock m_rw_lock;
	U64 m_sipproxy_fake_id;	     //用于标识SipProxy每次登陆的id，每次登陆都不一样
};

typedef GMEmbedSmartPtr<UserInfoContainer> SptrUIContainer;
typedef map<U64,SptrUIContainer> UIContainerMap;
typedef map<U64,SptrUIContainer>::iterator UIContainerMapIter;
class UserPortInfoManager : public GMSingleTon<UserPortInfoManager>
{
public:
	//找到对应sipproxy的container
	SptrUIContainer find_container_in_rlock(U64 sipproxy_id);
	bool get_ports_in_wlock(U64 sipproxy_id,U64 sipproxy_fake_id, UserPortInfo &info);
	bool del_user_port_info(U64 sipproxy_id, UserPortInfo &info);
	SptrUIContainer del_container(U64 sipproxy_id,U64 sipproxy_fake_id);

private:
	UIContainerMap m_user_portpair_info;
	GMRWLock m_rw_lock;
};

class FreePortPairContainer:public GMSingleTon<FreePortPairContainer>{
public:
	FreePortPairContainer():m_used_num(0){}

	U32 free_portpair(){
		return m_free_portpairs.size();
	}

	void add_portpair(PortPair& ports){
		m_lock.lock();
		if(m_used_num>0){
			InterlockedDecrement((volatile U32*)&m_used_num);
		}
		m_free_portpairs.push_back(ports);
		m_lock.unlock();
	}

	bool get_portpair(PortPair& ports){
		bool ret = false;
		m_lock.lock();
		if(!m_free_portpairs.empty()){
			ports = m_free_portpairs.front();
			m_free_portpairs.pop_front();
			InterlockedIncrement((volatile U32*)&m_used_num);
			ret = true;
		}
		m_lock.unlock();
		return ret;
	}

	std::list<PortPair> m_free_portpairs;
	U32 m_used_num;
	GMLock m_lock;
};


class SSID
{
public:
    SSID():m_id(0){}
    SSID(U64 id):m_id(id){}


    //增加1，并返回
    SSID increment()
    {
        U64 nid = InterlockedIncrement64((volatile long long*)&m_id);
        return SSID(nid);
    }

    bool operator<(const SSID &ssid) const
    {
        return m_id < ssid.m_id;
    }
public:
    volatile U64 m_id;	
};

#include <ctime>
inline U64 get_fake_pid(){
	U64 sys_pid = GetCurrentProcessId();
	time_t t;
	time(&t);
	U64 sa_pid = (sys_pid<<32)+(U32)t;
	return sa_pid;
}

class ReqDetail
{
public:
	ReqDetail()
		:m_get_ports(0),m_free_ports(0),m_start_call(0),m_stop_call(0),m_build_path(0),m_error_call(0){}

	U32 m_get_ports;
	U32 m_free_ports;
	U32 m_start_call;
	U32 m_stop_call;
	U32 m_build_path;
	U32 m_keep_alive;		//心跳数量
	U32 m_error_call;
};

class RtpProxyManager:public GMSingleTon<RtpProxyManager>{
public:
	RtpProxyManager():m_is_running(false){}
	bool start_rtp_proxy();
	void stop_rtp_proxy();

	bool m_is_running;
	GMLock m_start_lock;
};
#endif
