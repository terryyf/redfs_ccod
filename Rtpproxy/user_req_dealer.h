#ifndef UserReqDealer_h__
#define UserReqDealer_h__

#include <list>
#include "../common/rtpproxy_cmd.h"
#include "../common/cnn_mixer.h"
#include "helper.h"
#include "rtpproxy_manager.h"
#define MAX_CMD_NUM MAX_CMD_ID+1

class ExtremeValue: public ExtremeValueStat {
public:
    ExtremeValue(): ExtremeValueStat(true) {}
};

struct IWriteConnectionNotice : public FromXMemPool<IWriteConnectionNotice>
{
	virtual void on_write() = 0;
}; 

class UserReqDealer : public CnnMixer,public GMSingleTon<UserReqDealer>
{
public:	
	UserReqDealer();

	~UserReqDealer();

	bool start();

	void stop();

    void do_back_chores(CmdHead* resp,char* buf,const SACnnID& srcid,U32 skip_len=0);

	void deal_error_session(U64 sipproxy_id, const char* call_id);
 private:

	/*deal commnuication*/
	virtual void on_accept_connection(const SACnnID& id);

	virtual void on_read_connection(CnnMixerError cme,const SACnnID& cid,const GMAddrEx& remote_addr,char* buf, U32 req_len,
        U32 resp_len);

	virtual void on_write_connection(CnnMixerError cme,const SACnnID& cid, GMBUF* buf_array, 
        U32 array_num, U32 resp_len,U64 ctx1, U64 ctx2);

	void close_connection(const SACnnID& id);
	
	/*do chores*/

	bool do_front_chores(CmdHead* req,char*& buf,U32 rcvLen,const SACnnID& srcID, bool& version_conflict);

	/*deal business*/
	void deal_get_portpair(const SACnnID& cid,char* buf,U32 recvlen);
	void deal_free_portpair(const SACnnID& cid,char* buf,U32 recvlen);
	void deal_start_call(const SACnnID& cid,char* buf,U32 recvlen);
	void deal_stop_call(const SACnnID& cid,char* buf,U32 recvlen);
	void deal_build_path(const SACnnID& cid,char* buf,U32 recvlen);
    void deal_keep_alive( const SACnnID& cid,char* buf,U32 recvlen );
	void deal_shake_hand(const SACnnID& cid,char* buf,U32 recvlen);
	void deal_sipproxy_offline(const SACnnID& cid,char* buf,U32 recvlen);

	void get_req_detail(ReqDetail& req_detail);
    void get_resp_detail(ReqDetail& resp_detail);

	bool deal_media_data_transform(const SACnnID& cid,const GMAddrEx& remote_addr,char* buf,U32 recvlen);

	
	//根据cid找对应端口
	int cid_2_port(UID cid);
public:
	//根据地址找CPID
	aio::CPID addr_2_cpid(GMAddrEx& addr){
		aio::CPID id=0;
		std::map<GMAddrEx,aio::CPID>::iterator it = m_addr_2_cpid_map.find(addr);
		if(it!=m_addr_2_cpid_map.end()){
			id = it->second;
		}
		return id;
	}

public:
	SpeedStat m_udp_recv_speed;
	SpeedStat m_udp_send_speed;

	SpeedStat m_tcp_recv_speed;	
	SpeedStat m_tcp_send_speed;
	
    SpeedStat m_udp_recv_package;		//udp收包数
	SpeedStat m_udp_send_package;		//udp发包数
    SpeedStat m_udp_send_fail_package;	//udp发包失败数
	SpeedStat m_udp_drop_package;		//udp主动丢包数

	SpeedStat m_tcp_recv_package;		//tcp收包数
	SpeedStat m_tcp_send_package;		//tcp发包数
	SpeedStat m_tcp_send_fail_package;	//tcp发包失败数
	

    RateStat        m_average_duration_time[MAX_CMD_NUM]; //平均时间
    ExtremeValue    m_max_duration_time[MAX_CMD_NUM];     //最大时间

    SpeedStat m_req_speed[MAX_CMD_NUM];
    SpeedStatistic* m_ss;//统计每秒收发的任务
	
	//记录cpid跟地址的关系
	std::map<aio::CPID,GMAddrEx> m_cpid_2_addr_map;
	std::map<GMAddrEx,aio::CPID> m_addr_2_cpid_map;
	QuerySysInfo m_sys_info;//获取指定进程在周期内的cpu和内存的消耗的平均值

	GMAddrEx m_listen_addr;
};

inline void UserReqDealer::close_connection( const SACnnID& id )
{
	CnnMixer::closeConnection(id);
}


void record_stat(UserReqDealer* dealer, U32 bt, enum RA_CMD_TYPE type);

#endif // UserReqDealer_h__
