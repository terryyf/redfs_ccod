#ifndef CC_CHANNELS_H
#define CC_CHANNELS_H

#include "con_address.h"
#include "sa_cnn_id.h"
#include "rtpproxy_ecode.h"
#include <map>
#include <set>

#define CON_TIME_OUT 10000
#define CHECK_CHANNEL_INTERVAL 1000
#define MAX_CON_FAILED_TIME 10

class ChannelManager;
class RtpproxyAsynClient;

class SendTask
{
public:
	SendTask():m_tried_all(false),m_cmd(NULL),m_send_buf(NULL),m_send_len(0),m_cb(NULL),
			   m_enqueue_time(0),m_time_out(0),m_ctx(0){}

	SendTask(CmdHead* cmd,void* cb,U64 ctx,U32 timeout):m_cmd(cmd),m_cb(cb),m_ctx(ctx),
		m_time_out(timeout),m_send_buf(NULL)
	{
		m_cmd->m_usr_req_time=GMGetTickCount();
	}

	//修改了发送时间等信息后需要调用此方法，然后在发送
	void update_cmdhead_before_send()
	{
		XSpace xs(m_send_buf,PURE_CMD_MAX_SIZE);
		m_cmd->m_send_req_time = GMGetTickCount();
		CmdHead head = *m_cmd;
		head.ser(xs);
	}

public:
	CmdHead *m_cmd;
	char *m_send_buf;
	void *m_cb;
	U64 m_ctx;
	U32 m_send_len;
	U32 m_enqueue_time;
	U32 m_time_out;
	ConAddress m_key_addr;
	XSet<ConAddress> m_used_addrs;
	bool m_tried_all;  
};

class ConGroup:public FromXMemPool<ConGroup>
{
public:
	ConGroup(RtpproxyAsynClient *agent):m_agent(agent),m_failed_time(0), 
	m_last_con_time(0),m_seed(0), m_has_tried_to_connect(false){}
	
	/*负载均衡的返回一个连接*/
	SACnnID get_a_cnn();

	U32 cnn_size();

	void add_cnn(const SACnnID &conid);
	
	void del_cnn(const SACnnID &conid);

	void keep_connect();

	void close_all_cnn();

public:
	int m_failed_time;
	U32 m_last_con_time;
	bool m_has_tried_to_connect;

private:
	GMRWLock m_con_ids_lock;
	std::vector<SACnnID> m_connections;
	RtpproxyAsynClient* m_agent;
	GMDaemonThread<ConGroup> m_keep_con_thread;

	U32 m_seed;
};

class Channel;

class ChannelManager
{
public:
	ChannelManager(RtpproxyAsynClient *agent):m_agent(agent),m_runing(false){}
	
	bool start();

	void stop();

	bool init_channel(const ConAddress &key_addr);

	bool add_con_address(const ConAddress &key_addr,const ConAddress &address);

	bool get_cnn_addr(ConAddress& addr,const SACnnID& id);

	/*
	 获取一个连接
	 return 
		SA_ALLOC_MEMORY_FAILED 
		SA_CHANNEL_NOT_EXIST
		RA_SUC 成功 成功还需要判断cid，如果cid无效，需要加入等待队列
	*/
	RA_ECODE get_con_id(SendTask*task, SACnnID &cid);

	/*
	*发送前将任务缓存下来，如果在设定时间内未收到服务器回应，内部超时检测线程会通知任务超时
	*/
	void add_doing_task(SendTask*task);

	/*
	*取出正在做的任务。
	*当发送命令失败和收到回应后会取出正在做的任务
	*/
	bool fetch_doing_task(const CmdKey& key, SendTask* &task);


	/*
	*连接未建立的时候，将任何先缓存下来，连接建立好会自动进行发送
	*return 	
	*	SA_CHANNEL_NOT_EXIST
	*	RA_SUC
	*/
	RA_ECODE add_waitting_task(SendTask*task);

	/*
	*取出一个等待的任务
	*/
	bool fetch_waiting_task(SACnnID conid, SendTask* &task);

	void remove_cnn(const SACnnID &conid);

	void set_shake_hand(ConAddress &key_addr);

	void add_id_channels(SACnnID &conid, Channel *chan);

    U32 get_doing_task_num(){return m_doing_tasks.size();}
private:

	void manager_proc(void *param);

	void remove_channel_from_id_channes_table(Channel* c);

private:
	GMRWLock m_channels_lock;

	//通过标识地址找到通道。受m_channels_lock保护
	XMap<ConAddress, Channel*> m_channels;

	//通过连接ID找到通道。受m_channels_lock保护
	XMap<SACnnID, Channel*> m_id_channels;

	//正在做的任务列表
	GMRWLock m_doing_tasks_lock;
	XMap<CmdKey, SendTask*> m_doing_tasks;//key : cmd id

	GMSleepWithWakeup m_manager_thread_sleeper;
	GMDaemonThread<ChannelManager> m_manager_thread;

	RtpproxyAsynClient *m_agent;
	bool m_runing;
};

class Channel:public FromXMemPool<Channel>
{
public:
	Channel(ChannelManager *channel_manager, RtpproxyAsynClient *agent,const ConAddress &key_addr);

	~Channel();

	/*
	*根据
	*/
	void get_cnn(SACnnID &conid,SendTask*task);

	bool add_con_address(const ConAddress &addr);

	void manager_proc(void *param);

	void add_waitting_task(SendTask*task);

	void check_timeout_proc(void *param);

	void del_cnn(const SACnnID &conid);

	bool fetch_waitting_task(SendTask* &task);

	void check_connect();

public:
	bool m_shaked;
	bool m_failed;
	ConAddress m_key_addr;

private:
		
	GMRWLock m_wait_tasks_lock;
	XList<SendTask*> m_wait_tasks;

	GMRWLock m_groups_lock;//m_id_groups,m_con_groups都受此锁保护
	XMap<SACnnID, ConGroup*> m_id_groups;
	XMap<ConAddress, ConGroup*> m_con_groups;

	GMSleepWithWakeup m_manager_thread_sleeper;
	GMDaemonThread<Channel> m_manager_thread;
	GMSleepWithWakeup m_timeout_thread_sleeper;
	GMDaemonThread<Channel> m_timeout_thread;

	static const int m_con_interval = 2000;
	ChannelManager *m_channel_manager;
	RtpproxyAsynClient *m_agent;
	volatile U32 m_seed;
	U32 m_thread_interval;
};

#endif
