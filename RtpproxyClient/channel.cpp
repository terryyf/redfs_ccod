#include "stdafx.h"
#include "rtpproxy_cmd.h"
#include "channel.h"
#include "asyn_rtpproxy_client.h"

using namespace std;

SACnnID ConGroup::get_a_cnn()
{
	GMAutoLock<GMRWLock> al(&m_con_ids_lock,GMReadOperator);

	if (m_connections.empty()) {
        LGD("ConGroup::get_a_cnn->m_connections is empty");
		return SACnnID();
    }
	
	U32 index = InterlockedIncrement(&m_seed) % m_connections.size();
	return m_connections[index];
}

U32 ConGroup::cnn_size()
{
	m_con_ids_lock.readLock();
	U32 size = static_cast<U32>(m_connections.size());
	m_con_ids_lock.unReadLock();
	return size;
}

void ConGroup::add_cnn(const SACnnID &conid)
{
	m_con_ids_lock.writeLock();
	m_connections.push_back(conid);
	m_con_ids_lock.unWriteLock();
}

void ConGroup::del_cnn(const SACnnID &conid)
{
	m_con_ids_lock.writeLock();
    std::vector<SACnnID>::iterator it = m_connections.begin();
    for(;it!=m_connections.end();++it){
        if(*it==conid){
            m_connections.erase(it);
            break;
        }
    }	
	m_con_ids_lock.unWriteLock();
}

void ConGroup::close_all_cnn()
{
	m_con_ids_lock.writeLock();

	for (std::vector<SACnnID>::iterator it = m_connections.begin();it!=m_connections.end();++it)
		m_agent->closeConnection(*it);
	
	m_connections.clear();
	m_con_ids_lock.unWriteLock();
}

void ConGroup::keep_connect()
{
	//拷贝一份，发送包活包
	m_con_ids_lock.writeLock();
	std::vector<SACnnID> con_ids = m_connections;
	m_con_ids_lock.unWriteLock();

	for (std::vector<SACnnID>::iterator it = con_ids.begin(); it != con_ids.end(); ++it)
	{
		KeepConnetionAlive ka;
		char *p = (char*)XMemPool::alloc(PURE_CMD_MAX_SIZE,"agent,ConGroup::keep_connect->KeepConnetionAlive buf");
		if (p != NULL)
		{
			XSpace xs(p, PURE_CMD_MAX_SIZE);
			ka.m_sipproxy_fake_id = RtpproxyAsynClient::Obj()->m_sipporxy_fake_id;
			ka.m_sipproxy_id = RtpproxyAsynClient::Obj()->m_sipproxy_id;
			ka.m_usr_req_time = GMGetTickCount();
			ka.m_send_req_time = ka.m_usr_req_time;
            ka.ser(xs);
			GMBUF buf_array(p,xs.used_size());
			m_agent->asynWriteConnection(*it, &buf_array,1, RA_KEEP_ALIVE_REQ, RA_KEEP_ALIVE_REQ);
		}
	}
}

Channel::Channel(ChannelManager *channel_manager, RtpproxyAsynClient *agent,const ConAddress &key_addr) 
	: m_channel_manager(channel_manager)
	, m_agent(agent)
	, m_key_addr(key_addr)
	, m_failed(false)
	, m_shaked(false)
	, m_seed(0)
	, m_thread_interval(0)
{
	m_manager_thread.init(&Channel::manager_proc, this, NULL);
	m_manager_thread.start();

	m_timeout_thread.init(&Channel::check_timeout_proc, this, NULL);
	m_timeout_thread.start();
}

Channel::~Channel()
{
	U32 bt = GMGetTickCount();
	U32 dt = 0;
	m_manager_thread.stop();
	m_manager_thread_sleeper.wakeup();
	m_manager_thread.waitThreadExit();

	//dt = GMGetTickCount() - bt;
	//printf("~Channel---------1 %d\n",dt);
	//bt = GMGetTickCount();

	m_timeout_thread.stop();
	m_timeout_thread_sleeper.wakeup();
	m_timeout_thread.waitThreadExit();
	
	/*dt = GMGetTickCount() - bt;
	printf("~Channel---------2 %d\n",dt);
	bt = GMGetTickCount();*/
    /*
    m_wait_tasks_lock.writeLock();
    for (XList<SendTask*>::iterator it = m_wait_tasks.begin(); it != m_wait_tasks.end(); ++it){
        XMemPool::dealloc(it->value->m_send_buf);
        m_agent->notify_by_task(SA_AGENT_NOT_RUNNING,it->value);	
    }
    m_wait_tasks.clear();
    m_wait_tasks_lock.unWriteLock();
    */
	/*dt = GMGetTickCount() - bt;
	printf("~Channel---------2 %d\n",dt);
	bt = GMGetTickCount();*/

	m_groups_lock.writeLock();
	for (XMap<ConAddress, ConGroup*>::iterator it = m_con_groups.begin(); it != m_con_groups.end(); ++it){
		it->second->close_all_cnn();
		delete it->second;
	}

	//dt = GMGetTickCount() - bt;
	//printf("~Channel---------2 %d\n",dt);
	//bt = GMGetTickCount();

	m_con_groups.clear();
	m_groups_lock.unWriteLock();
}

void Channel::get_cnn(SACnnID &conid, SendTask*task)
{
	++task->m_cmd->m_try_get_cnn_times;
	
	GMAutoLock<GMRWLock> al(&m_groups_lock,GMReadOperator);

	if (m_con_groups.empty())
		return;

	//第一次轮流选择一个连接组
	if (task->m_used_addrs.empty())
	{
		XMap<ConAddress, ConGroup*>::iterator it = m_con_groups.begin();
		U32 index = (U32)InterlockedIncrement(&m_seed) % m_con_groups.size();
		for (U32 i=0; i<index ; ++i, ++it);//just increment it
				
		if (it->second->m_has_tried_to_connect)
		{
			conid = it->second->get_a_cnn();
			task->m_used_addrs.insert(it->first);
		}
	}
	
	if (conid.m_id == 0)
	{
		for (XMap<ConAddress, ConGroup*>::iterator it = m_con_groups.begin(); it != m_con_groups.end(); ++it)
		{
			XSet<ConAddress>::iterator it_find = task->m_used_addrs.find(it->first);
			if (it_find == task->m_used_addrs.end() && it->second->m_has_tried_to_connect)
			{
				conid = it->second->get_a_cnn();
				task->m_used_addrs.insert(it->first);
				break;
			}
		}

		if (conid.m_id == 0 && m_con_groups.size() <= task->m_used_addrs.size())
		{
			task->m_tried_all = true;
			if (m_con_groups.size() == 1)
			{
				conid = m_con_groups.begin()->second->get_a_cnn();
				task->m_used_addrs.insert(m_con_groups.begin()->first);
			}
		}
	}

	if (0 != conid.m_id)
		++task->m_cmd->m_got_cnn_times;
}

bool Channel::add_con_address(const ConAddress &addr)
{
	m_groups_lock.readLock();
	XMap<ConAddress, ConGroup*>::iterator it = m_con_groups.find(addr);
	if (it != m_con_groups.end()){
		m_groups_lock.unReadLock();
		return true;
	}
	m_groups_lock.unReadLock();

	m_groups_lock.writeLock();
	it = m_con_groups.find(addr);
	if (it != m_con_groups.end()){
		m_groups_lock.unWriteLock();
		return true;
	}

	ConGroup *group = new(std::nothrow) ConGroup(m_agent);
	if (group == NULL){
		m_groups_lock.unWriteLock();
		return false;
	}

	m_con_groups.insert(std::make_pair(addr, group));
	m_groups_lock.unWriteLock();

	m_manager_thread_sleeper.wakeup();//立马建立连接
	return true;
}

void Channel::manager_proc(void *param)
{
	m_manager_thread_sleeper.sleep(CHECK_CHANNEL_INTERVAL);

	check_connect();

	m_groups_lock.readLock();
	if (m_failed || m_con_groups.empty())
	{
		m_failed = true;
		m_groups_lock.unReadLock();
		return;
	}
	m_groups_lock.unReadLock();

	if (!m_shaked)
	{
		ShakeHandReq* req = new(std::nothrow) ShakeHandReq();
		if (req != NULL)
		{
			req->m_key_addr = m_key_addr.m_raddr;
			m_agent->dotask(&m_key_addr.m_raddr, req,DEFAULT_TIMEOUT, 0, NULL);
		}
	}

	//连接建立成功，驱动所有等待队列中的任务
	SendTask* task;
	while (fetch_waitting_task(task))
	{
		I32 dt = static_cast<I32>(GMGetTickCount() - task->m_cmd->m_usr_req_time); 
		if (dt > static_cast<I32>(task->m_time_out))
		{
			XMemPool::dealloc(task->m_send_buf);
			Log::writeWarning(LOG_SA,1,"Channel::manager_proc->连接建立成功后，准备做等待队列中的任务但任务已经超时。任务已提交%dms",dt);
			m_agent->notify_by_task(RA_TIMEOUT,task);
			continue;
		}

		SACnnID conid;
		get_cnn(conid, task);
		if (conid.m_id != 0)
		{
			m_channel_manager->add_doing_task(task);
			task->update_cmdhead_before_send();
			m_agent->write_cnn(conid,task);
		}
		else
		{
			add_waitting_task(task);
			Log::writeWarning(LOG_SA,1,"Channel::manager_proc->没能获取到连接,继续放回对头，任务已经提交%dms.",dt);
			break;
		}
	}
}

void Channel::check_timeout_proc(void *param)
{
	XList<SendTask*> notify_list;

	m_wait_tasks_lock.writeLock();
	U64 ct = GMGetTickCount();
	for (XList<SendTask*>::iterator it = m_wait_tasks.begin(); it != m_wait_tasks.end();)
	{
        //超时没有将SendTask发送出去
		if ((ct - it->value->m_cmd->m_usr_req_time) > it->value->m_time_out)
		{
			notify_list.push_back(it->value);
			m_wait_tasks.erase(it++);
		}
		else
		{
			++it;
		}
	}
	m_wait_tasks_lock.unWriteLock();

	for (XList<SendTask*>::iterator it = notify_list.begin(); it != notify_list.end(); ++it)
	{
		XMemPool::dealloc(it->value->m_send_buf);
		
		CmdHead& head = *(it->value->m_cmd);
		U64 ft = GMGetTickCount();
		U32 total_time = static_cast<U32>(ft - head.m_usr_req_time);

        LGE("->svr addr %s cmd_id %d passwd %X cmd_type %d.已经在队列中等待%dms (排队%d) 排队%d次,"
            "尝试获取连接%d次，发送请求%d次 ",
            GMAddress(it->value->m_key_addr.m_raddr.m_ip,it->value->m_key_addr.m_raddr.m_port).toStr(),
            head.m_cmd_id,head.m_passwd,head.m_cmd_type,total_time,head.m_agent_total_queue_time,
            head.m_agent_queue_times,head.m_try_get_cnn_times,head.m_got_cnn_times);

        if(0!=head.m_got_cnn_times){
            m_agent->notify_by_task(RA_TIMEOUT,it->value);
        }else{
            //一次连接都没有获取到的，返回SA_CHANNEL_NOT_EXIST
            m_agent->notify_by_task(RA_CHANNEL_NOT_EXIST,it->value);
        }
	}

	m_timeout_thread_sleeper.sleep(1000);
}

void Channel::del_cnn(const SACnnID &conid)
{
	m_groups_lock.writeLock();
	XMap<SACnnID, ConGroup*>::iterator it = m_id_groups.find(conid);
	if (it != m_id_groups.end())
	{
		it->second->del_cnn(conid);
		m_id_groups.erase(it);
	}

	m_groups_lock.unWriteLock();
	m_manager_thread_sleeper.wakeup();
}

void Channel::add_waitting_task(SendTask*task)
{
	task->m_enqueue_time = GMGetTickCount();
	++task->m_cmd->m_agent_queue_times;
	m_wait_tasks_lock.writeLock();

	assert(task->m_cmd->m_try_get_cnn_times>=1);//进入等待队列至少尝试获取过一次连接

	if (1 == task->m_cmd->m_try_get_cnn_times)
		m_wait_tasks.push_back(task);
	else
		m_wait_tasks.push_front(task);//非第一次提交的任务都是从等待队列对头取出的，所以如果不能立即处理应放回对头
	m_wait_tasks_lock.unWriteLock();
}

bool Channel::fetch_waitting_task(SendTask* &task)
{
	GMAutoLock<GMRWLock> al(&m_wait_tasks_lock,GMWriteOperator);
	if (m_wait_tasks.empty())
		return false;

	task = m_wait_tasks.front();
	task->m_cmd->m_agent_total_queue_time +=GMGetTickCount()-task->m_enqueue_time;

	m_wait_tasks.pop_front();	
	
	return true;
}

void Channel::check_connect()
{
	m_groups_lock.readLock();
	XMap<ConAddress, ConGroup*> con_groups = m_con_groups;
	m_groups_lock.unReadLock();

	for (XMap<ConAddress, ConGroup*>::iterator it = con_groups.begin(); it != con_groups.end(); ++it)
	{
		if (GMGetTickCount() - it->second->m_last_con_time > m_con_interval)
		{
			int need_con_num = it->first.m_con_num - it->second->cnn_size();
			for (int i=0; i<need_con_num; ++i)
			{
				SACnnID conid;
				if (m_agent->connnect(conid, it->first, CON_TIME_OUT))
				{
					char *buf = (char*)XMemPool::alloc(PURE_CMD_MAX_SIZE,"ConGroup::check_connect,post recv buf");
					if (NULL != buf)
					{
						//1 add into group
						it->second->add_cnn(conid);
						
						//2 add into channel
						m_groups_lock.writeLock();
						m_id_groups.insert(make_pair(conid, it->second));
						m_groups_lock.unWriteLock();

						//3 add into manager
						m_channel_manager->add_id_channels(conid, this);
					
						m_agent->asynReadConnection(conid, buf,PURE_CMD_MAX_SIZE);
					}
					else
					{
						m_agent->closeConnection(conid);
					}
				}
			}

			it->second->m_last_con_time = GMGetTickCount();
		}

		it->second->m_has_tried_to_connect = true;
		if (it->second->cnn_size() == 0)//连续MAX_CON_FAILED_TIME次坚持连接个数为0,删除连接组
		{
			++it->second->m_failed_time;
		}
		else
		{
			it->second->m_failed_time = 0;
			if (m_thread_interval % 3 == 0)//这里需要跟sa端的进程死亡时间同步
				it->second->keep_connect();
		}

		if (it->second->m_failed_time == MAX_CON_FAILED_TIME)
		{
			m_groups_lock.writeLock();

			//从m_id_groups表中移除所有ConGroup
			for (XMap<SACnnID, ConGroup*>::iterator it_group = m_id_groups.begin(); it_group != m_id_groups.end();)
			{
				if (it->second == it_group->second)
					m_id_groups.erase(it_group++);
				else
					++it_group;
			}

			//从m_id_groups表中移除所有ConGroup
			m_con_groups.erase(it->first);

			m_groups_lock.unWriteLock();

			//因为判断连接个数 和 移除 ConGroup没在一个锁里，万一中途增加了连接也关闭。
			it->second->close_all_cnn();
			delete it->second;
		}
	}

	++m_thread_interval;
}

//////////////////////////////////////////////////////////////////////////
bool ChannelManager::start()
{
	if (m_runing)
		return true;

	m_manager_thread.init(&ChannelManager::manager_proc, this, NULL);
	m_runing = m_manager_thread.start();

	return m_runing;
}

void ChannelManager::stop()
{
	if (!m_runing)
		return;

	m_runing = false;
	U32 bt = GMGetTickCount();
	U32 dt = 0;
    XList<Channel*> channel_list;
	m_channels_lock.writeLock();
	m_id_channels.clear();
	
	for (XMap<ConAddress, Channel*>::iterator it_chan = m_channels.begin(); it_chan != m_channels.end(); ++it_chan){
		channel_list.push_back(it_chan->second);
	}
	m_channels.clear();
	m_channels_lock.unWriteLock();

    for(XList<Channel*>::iterator it = channel_list.begin();it!=channel_list.end();it++)
        delete *it;
	/*dt = GMGetTickCount() - bt;
	printf("-------------1 %d\n",dt);*/

    bt = GMGetTickCount();
    m_manager_thread.stop();
    m_manager_thread_sleeper.wakeup();
    m_manager_thread.waitThreadExit();

	m_doing_tasks_lock.writeLock();
	for (XMap<CmdKey, SendTask*>::iterator it_do = m_doing_tasks.begin(); it_do != m_doing_tasks.end(); ++it_do){
		m_agent->notify_by_task(RA_AGENT_NOT_RUNNING,it_do->second);
	}
	m_doing_tasks.clear();
	m_doing_tasks_lock.unWriteLock();
	//dt = GMGetTickCount() - bt;
	//printf("-------------2 %d\n",dt);
}

bool ChannelManager::init_channel(const ConAddress &key_addr)
{
	if (!m_runing)
		return false;

	m_channels_lock.readLock();
	XMap<ConAddress, Channel*>::iterator it = m_channels.find(key_addr);
	if (it != m_channels.end()){
		m_channels_lock.unReadLock();
		return true;
	}
	m_channels_lock.unReadLock();

	m_channels_lock.writeLock();
	it = m_channels.find(key_addr);
	if (it != m_channels.end()){
		m_channels_lock.unWriteLock();
		return true;
	}

	Channel *chan = new(std::nothrow) Channel(this, m_agent, key_addr);
	if (chan == NULL){
		m_channels_lock.unWriteLock();
		return false;
	}

	if (!chan->add_con_address(key_addr)){
		delete chan;
		m_channels_lock.unWriteLock();
		return false;
	}

	m_channels.insert(std::make_pair(key_addr, chan));
	m_channels_lock.unWriteLock();

	return true;
}

bool ChannelManager::add_con_address(const ConAddress &key_addr,const ConAddress &address)
{
	if (!m_runing)
		return false;

	m_channels_lock.readLock();
	XMap<ConAddress, Channel*>::iterator it = m_channels.find(key_addr);
	if (it == m_channels.end()){
		m_channels_lock.unReadLock();
		return false;
	}

	bool ret = it->second->add_con_address(address);
	m_channels_lock.unReadLock();

	return ret;
}

RA_ECODE ChannelManager::get_con_id(SendTask*task, SACnnID &conid)
{
	if (!m_runing)
		return RA_AGENT_NOT_RUNNING;

	if (!init_channel(task->m_key_addr))
		return RA_ALLOC_MEMORY_FAILED;

	m_channels_lock.readLock();
	XMap<ConAddress, Channel*>::iterator it = m_channels.find(task->m_key_addr);
	if (it == m_channels.end()){
		m_channels_lock.unReadLock();
		return RA_CHANNEL_NOT_EXIST;
	}

	it->second->get_cnn(conid, task);
	m_channels_lock.unReadLock();

	return RA_SUC;
}

void ChannelManager::add_doing_task(SendTask*task)
{
	m_doing_tasks_lock.writeLock();	
	m_doing_tasks.insert(make_pair(CmdKey(task->m_cmd->m_cmd_id,task->m_cmd->m_passwd), task));
	m_doing_tasks_lock.unWriteLock();
}

void ChannelManager::remove_channel_from_id_channes_table(Channel* c )
{
	for (XMap<SACnnID, Channel*>::iterator it = m_id_channels.begin();it!=m_id_channels.end();){
		if (it->second == c)
			m_id_channels.erase(it++);
		else
			++it;
	}
}

void ChannelManager::remove_cnn(const SACnnID &conid)
{
	if (!m_runing)
		return;

	m_channels_lock.writeLock();
	XMap<SACnnID, Channel*>::iterator it = m_id_channels.find(conid);
	if (it != m_id_channels.end()){
		it->second->del_cnn(conid);
		m_id_channels.erase(it);
	}
	m_channels_lock.unWriteLock();
}

void ChannelManager::add_id_channels(SACnnID &conid, Channel *chan)
{
	m_channels_lock.writeLock();
	m_id_channels.insert(std::make_pair(conid, chan));
	m_channels_lock.unWriteLock();
}

bool ChannelManager::fetch_doing_task(const CmdKey& key, SendTask* &task)
{
	bool ret = false;
	m_doing_tasks_lock.writeLock();

	XMap<CmdKey, SendTask*>::iterator it = m_doing_tasks.find(key);
	if (it != m_doing_tasks.end())
	{
		ret = true;
		task = it->second;
		m_doing_tasks.erase(it);
	}

	m_doing_tasks_lock.unWriteLock();
	return ret;
}

void ChannelManager::set_shake_hand(ConAddress &key_addr)
{
	if (!m_runing)
		return;

	m_channels_lock.readLock();

	XMap<ConAddress, Channel*>::iterator it = m_channels.find(key_addr);
	if (it != m_channels.end())
		it->second->m_shaked = true;

	m_channels_lock.unReadLock();
}

RA_ECODE ChannelManager::add_waitting_task(SendTask*task)
{
	if (!m_runing)
		return RA_AGENT_NOT_RUNNING;

	m_channels_lock.readLock();
	XMap<ConAddress, Channel*>::iterator it = m_channels.find(task->m_key_addr);
	if (it == m_channels.end()){
		m_channels_lock.unReadLock();
		return RA_CHANNEL_NOT_EXIST;
	}

	it->second->add_waitting_task(task);
	m_channels_lock.unReadLock();
	return RA_SUC;
}

bool ChannelManager::fetch_waiting_task(SACnnID conid, SendTask* &task)
{
	GMAutoLock<GMRWLock> al(&m_channels_lock,GMReadOperator);

	XMap<SACnnID, Channel*>::iterator it = m_id_channels.find(conid);
	if (it != m_id_channels.end())
		return it->second->fetch_waitting_task(task);

	return false;
}


bool ChannelManager::get_cnn_addr(ConAddress& addr,const SACnnID& id )
{
	m_channels_lock.readLock();
	XMap<SACnnID, Channel*>::iterator it = m_id_channels.find(id);
	if (it != m_id_channels.end()){
		addr = it->second->m_key_addr;
		m_channels_lock.unReadLock();
		return true;
	}
	
	m_channels_lock.unReadLock();
	return false;
}

void ChannelManager::manager_proc(void *param)
{
	XList<SendTask*> notify_list;

	m_doing_tasks_lock.writeLock();
	U64 ct = GMGetTickCount();
	U32 doing_num = static_cast<U32>(m_doing_tasks.size());
	for (XMap<CmdKey, SendTask*>::iterator it = m_doing_tasks.begin(); it != m_doing_tasks.end();)
	{
		if ((ct - it->second->m_cmd->m_usr_req_time) > it->second->m_time_out){
			notify_list.push_back(it->second);
			m_doing_tasks.erase(it++);
		}else{
			++it;
		}
	}
	m_doing_tasks_lock.unWriteLock();
	U32 dt = static_cast<U32>(GMGetTickCount()-ct);
	if (dt > 20)
		Log::writeWarning(LOG_SA,1,"SAA:ChannelManager::manager_proc->遍历正在做的任务耗时%dms 正在做任务总数%d",dt,doing_num);

	for (XList<SendTask*>::iterator it = notify_list.begin(); it != notify_list.end(); ++it)
	{
		CmdHead& head = *(it->value->m_cmd);
		U64 ft = GMGetTickCount();
		U32 total_time = static_cast<U32>(ft - head.m_usr_req_time);
		U32 last_send_time = static_cast<U32>(head.m_send_req_time - head.m_usr_req_time); //超时没有收到服务器响应
		{
			Log::writeError(LOG_SA,1,"SAA:ChannelManager::manager_proc->svr addr %s cmd_id %d passwd %X cmd_type %s. \n"
				"处理中的任务超时，目前距用户请求%dms 最近一次发送距用户请求%dms,(排队%d) 排队%d次，尝试获取连接%d次，发送请求%d次 ",
				GMAddress(it->value->m_key_addr.m_raddr.m_ip,it->value->m_key_addr.m_raddr.m_port).toStr(),head.m_cmd_id,head.m_passwd,cmd_name(head.m_cmd_type),
				total_time,last_send_time,head.m_agent_total_queue_time,head.m_agent_queue_times,head.m_try_get_cnn_times,head.m_got_cnn_times);
		}
		m_agent->notify_by_task(RA_TIMEOUT,it->value);
	}
    notify_list.clear();
	//delete failed channel
    SendTask* task;
    bool fetch_result =true;
	m_channels_lock.writeLock();
	for (XMap<ConAddress, Channel*>::iterator it = m_channels.begin(); it != m_channels.end();){
		if (it->second->m_failed){
			//从id_channels_table移除后才可以删除通道，否则会出现访问被删除了的通道。
			remove_channel_from_id_channes_table(it->second);
            //取出等待的任务，在锁外面通知，因为上层调用可能在回调里继续调用，会引发m_channels_lock死锁
            while(fetch_result){                
               fetch_result = it->second->fetch_waitting_task(task);
               if(fetch_result)
                   notify_list.push_back(task);
            }
			delete it->second;
			m_channels.erase(it++);
		}else{
			++it;
		}
	}
	m_channels_lock.unWriteLock();
    for (XList<SendTask*>::iterator it = notify_list.begin(); it != notify_list.end(); ++it)
    {
        XMemPool::dealloc(it->value->m_send_buf);
        m_agent->notify_by_task(RA_AGENT_NOT_RUNNING,it->value);
    }
    notify_list.clear();
	m_manager_thread_sleeper.sleep(2000);
}
