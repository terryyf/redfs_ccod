#ifndef CC_HELPER_H
#define CC_HELPER_H

#include <time.h>
#include "stdafx.h"
#include "CpuPerfC.h"
#ifdef WIN32
#include "io.h"
#include <pdh.h>
#include "Psapi.h"
#pragma comment(lib, "pdh.lib")
#endif

typedef struct RtpHeader
{
#ifdef ORTP_BIGENDIAN
	uint16_t version:2;
	uint16_t padbit:1;
	uint16_t extbit:1;
	uint16_t cc:4;
	uint16_t markbit:1;
	uint16_t paytype:7;
#else
	uint16_t cc:4;
	uint16_t extbit:1;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t paytype:7;
	uint16_t markbit:1;
#endif
	uint16_t seq_number;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t csrc[16];
} rtp_header_t;

typedef struct _QN_SUB_EXTEND_
{
	uint8_t	 qn_pt;				//自定义消息类型标识.	qn_media_type_t
	uint8_t  qn_subflow_id;		//多路径子流ID.
	uint16_t qn_seq_number;		//各子流中消息序号.		//一条路径上的端到端统计
	uint16_t qn_server_seq;		//留给服务器端填充下行段序列号的字段，用于统计下行段丢包率   //分段统计 
	uint16_t qn_sp_pt_seq;		//当前路径下，当前媒体类型的发送序列号，用于统计该段路径中各种类型数据的丢包率
	uint8_t  qn_static_sub_id;   //标识是那条子路的统计信息,在发统计信息时用
	uint8_t  qn_sub_ext_t_len;  //扩展结构qn_sub_ext_t的占用的字节大小
}qn_sub_ext_t;

typedef enum _media_type_
{
	QN_MEDIA_BEGING=-1,
	QN_MEDIA_AUDIO=0,
	QN_MEDIA_AUDIO_RTCP,	//1
	QN_MEDIA_VIDEO,
	QN_MEDIA_VIDEO_RTCP,	//3
	QN_MEDIA_EXT_P2P_PING,
	QN_MEDIA_EXT_SUB_RTCP,	//5	终端使用
	QN_MEDIA_EXT_AUDIO_FEC,
	QN_MEDIA_EXT_VIDEO_FEC,	//7
	QN_MEDIA_AUDIO_4FEC,
	QN_MEDIA_VIDEO_4FEC,	//9

	///--------------------------->  <---------------------------///
	QN_MEDIA_EXT_SRV_STATISTIC_RESET,	//10	 通话初期，客户端发送该包至服务端，服务端将统计数据清零

	QN_MEDIA_EXT_UP_SRV_PING,	//11	上行PING，客户端发向中转服务的PING包
	QN_MEDIA_EXT_DOWN_SRV_PING,	//		下行PING，中转服务发向客户端的PING包

	QN_MEDIA_EXT_UP_SRV_RTCP,	//13	上行分段统计反馈
	QN_MEDIA_EXT_DOWN_SRV_RTCP,	//		下行分段统计反馈数据

	QN_MEDIA_EXT_INTER_SRV_PING,//15	背靠背传输，媒体relay之间的PING包
	QN_MEDIA_EXT_INTER_SRV_RTCP,//		背靠背传输，媒体Relay之间的统计信息，反馈格式(A->B的统计，B统计后，继续向前发送至B的接收端)
	///--------------------------->  <---------------------------///

	QN_MEDIA_EXT_REMOTE_UP_RTCP,		//17	 远端SRV UPLOAD RTCP包， 该包由媒体中转产生，发送至RTP发送端，由终端更改包类型，再发向对端，做统计使用
	QN_MEDIA_EXT_REMOTE_DOWN_RTCP,		//		 远端DOWN LOAD RTCP包， 由客户端端产生，发送至对端， 中转服务器透传该包。
	QN_MEDIA_EXT_REMOTE_INTER_SRV_RTCP,	//19	 背靠背传输，远端relay之间的统计信息，客户端使用，将本端信息发送至对端

	QN_MEDIA_EXT_REMOTE_RTT_RTCP,	//20
	QN_MEDIA_EXT_TRANS_LOSS_RTCP,		//21	 用于传输的端到端丢包统计包 

	QN_MEDIA_EXT_VIDEO_FPS_RTP,		//22	 用于传输的端到端丢包统计包 

	QN_MEDIA_EXT_AUDIO_COPY,		 //23   音频重发包（数据尾部带重发包计数字段）
	QN_MEDIA_EXT_VIDEO_COPY,		 //24   视频重发包（数据尾部带重发包计数字段）

	QN_MEDIA_EXT_AUDIO_ARQ_RTP,		 //25   音频丢包重传包（数据报尾部带冲发包计数字段）
	QN_MEDIA_EXT_VIDEO_ARQ_RTP,		 //26   视频丢包重传包（数据报尾部带冲发包计数字段）

	QN_MEDIA_EXT_AUDIO_ARQ_ACK,		 //27	音频收包确认包，也是重传请求包
	QN_MEDIA_EXT_VIDEO_ARQ_ACK,		 //28	视频收包确认包，也是重传请求包

	QN_MEDIA_EXT_E2E_RTT_PING,		 //29   主路RTT时延统计

	QN_MEDIA_EXT_TRANS_RECV_INFO_RTCP,  //30   用于传输的端到端接收到的RTP信息
	QN_MEDIA_EXT_E2E_NOTIFICATION_NETCHANGE,	//31   用于端到端切换网络的通知

	QN_MEDIA_EXT_AUDIO_DEC_FROM_FEC,	//32 从FEC恢复的音频包
	QN_MEDIA_EXT_VIDEO_DEC_FROM_FEC,	//33 从FEC恢复的视频包

	QN_MEDIA_OTHER=127,
	QN_MEDIA_END
}qn_media_type_t;

class SpeedStatistic
{
  static const U32 zone_num = 1;
public:
  SpeedStatistic(U32 interval=300):m_interval(interval),m_sendRespTimes(0),m_recvReqTimes(0){
    if(m_interval<1)m_interval=1000;

    m_beginTime = GMGetTickCount();
    m_showThread.init(&SpeedStatistic::showProc,this,NULL);
    m_showThread.start();
  }

  ~SpeedStatistic(){
    m_showThread.stop();
    m_showThread.waitThreadExit();
  }

  void count_req(U32 num){
    InterlockedExchangeAdd(&m_recvReqTimes,num);
  }

  void count_resp(U32 num){
    InterlockedExchangeAdd(&m_sendRespTimes,num);
  }

  U32 get_req_num(){
    return m_recvReqSpeed;
  }

  U32 get_resp_num(){
    return m_sendRespSpeed;
  }
private:
  void showProc(void*)
  {
    Sleep(m_interval);

    U32 sendRespTimes = 0;
    U32 recvReqTimes = 0;

    sendRespTimes =	InterlockedExchange(&m_sendRespTimes,sendRespTimes);
    recvReqTimes =	InterlockedExchange(&m_recvReqTimes,recvReqTimes);

    U32 realInterval = GMGetTickCount() - m_beginTime;
    realInterval = realInterval > 0 ? realInterval : 1;
    m_beginTime = GMGetTickCount();

    m_recvReqSpeed = recvReqTimes*1000/realInterval;
    m_sendRespSpeed = sendRespTimes*1000/realInterval;
  }

private:
  GMDaemonThread<SpeedStatistic> m_showThread;

  U32 m_recvReqTimes;
  U32 m_sendRespTimes;
  U32 m_interval;
  U32 m_beginTime;
  U32 m_recvReqSpeed;
  U32 m_sendRespSpeed;
};

class SynTaskAsynDealer:public GMSingleTon<SynTaskAsynDealer>
{
public:
    bool start(U32 thread_num = 10){
        return m_dealer.start(thread_num);
    }
    void stop(){
        m_dealer.stop();
    }
    bool add(ITask* task){
        return m_dealer.add(task);
    }
private:
    TaskDealer m_dealer;
};

class DurationCounterEx:public DurationCounter{
public:
    U64 get_duration(bool reset=true){
        U64 d = DurationCounter::get_duration();
        if (reset){
            reset_begin_time();
        }
        return d;
    }
};

class QuerySysInfo{
public:
	QuerySysInfo(U32 interval,U32 num,DWORD pid){}
	bool start(){
	return m_cpu_usage.init();
}
	void stop(){
	m_cpu_usage.uninit();
}
	void get_cpu_usage(CpuInfo& cpu_info){
		m_cpu_usage.get_cpu_info(cpu_info);
	}
	U32 get_mem_usage(){
#ifdef WIN32 //todo add linux code
	HANDLE hToken;
	LUID uID;
	TOKEN_PRIVILEGES tp;
	if(OpenProcessToken(GetCurrentProcess(),TOKEN_ALL_ACCESS, &hToken))
	{
		LookupPrivilegeValue(NULL,SE_DEBUG_NAME ,&uID);
		tp.PrivilegeCount           = 1;
		tp.Privileges[0].Luid       = uID;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(hToken ,FALSE, &tp, sizeof(tp), NULL, NULL);
		CloseHandle(hToken);
	}

	MEMORYSTATUSEX memory_status;
	memory_status.dwLength = sizeof (memory_status);
	if(GlobalMemoryStatusEx(&memory_status)){
		return ((memory_status.ullTotalPhys-memory_status.ullAvailPhys)*100)/memory_status.ullTotalPhys;
	}
	LGE("GlobalMemoryStatusEx fail");
	return 100;
#else
		FILE *fp;
		if((fp = fopen("/proc/meminfo", "r")) == NULL){
			printf("fopen fail\n");
			return -1;
		}

		char buf[128];
		char total[128];
		char avaiable[128];
		long long i = 0;
		long long j = 0;
		while(strlen(fgets(buf,128,fp)) > 0){
			if(strncmp(buf,"MemTotal",8) == 0){
				for(i = 9; i<strlen(buf); ++i)
				{
					if(buf[i] > '0' && buf[i] < '9'){
						total[j] = buf[i];
						++j;
					}
				}
			}else if(strncmp(buf,"MemAvailable",12) == 0){
				j = 0;
				for(i = 13; i<strlen(buf); ++i)
				{
					if(buf[i] > '0' && buf[i] < '9'){
						avaiable[j] = buf[i];
						++j;
					}
				}
				break;
			}
		}
		double all = atof(total);
		double avai = atof(avaiable);
		double mem_usage = (all - avai) / all;
		fclose(fp);
		return mem_usage*100;
#endif
}
private:
	CpuCounter m_cpu_usage;
};

class LossRate{
public:
	LossRate():m_max_seq_num(0),m_min_seq_num(0),m_total_package_num(0),m_unordered_num(0){}
	void update_statistic(U64 seq_num){
		m_lock.lock();
		//收到总数加1
		m_total_package_num++;

		if(0==m_min_seq_num&&m_min_seq_num==m_max_seq_num){
			m_min_seq_num = seq_num;
			m_max_seq_num = seq_num;
			m_lock.unlock();
			return ;
		}
		
		//不等于当前最大包号的下一个，则认为乱序
		if(seq_num<m_max_seq_num+1){
			m_unordered_num++;
		}else{
			m_max_seq_num=seq_num;
		}
		m_lock.unlock();
	}

	//获取统计信息，包括收到包总数，丢包数，丢包率，乱序率
	void get_statistic(U32& total_num,U32& loss_num,U32& loss_rate,U32& unordered_rate)
	{
		m_lock.lock();
		loss_num = m_max_seq_num - m_min_seq_num-m_total_package_num;
		total_num = m_total_package_num;

		loss_rate = loss_num*100/(m_max_seq_num-m_min_seq_num);
		unordered_rate = m_unordered_num*100/m_total_package_num;
		m_total_package_num = 0;
		m_unordered_num =0;
		m_min_seq_num = m_max_seq_num;
		m_lock.unlock();
	}

	U32 m_total_package_num;//收到总的包数
	U32 m_unordered_num;//乱序包数

	U64 m_max_seq_num;//当前最大包序列号
	U64 m_min_seq_num;//当前最小包序列号
	GMLock m_lock;
};
#endif
