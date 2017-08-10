#ifndef  _UDP_TEST_H_
#define  _UDP_TEST_H_
#include <aio_basic/CompletionPortPub.h>
#pragma comment(lib,"aio_basic")
using namespace aio;

//12000个用户，两两对发数据，每个用户占用2个端口，1个收，1个发，每秒50个包
//目标 总共120万个包，400MB流量
class UdpUser;
#define  TIMER_NUM 7
class UdpTester:public UdpWritevNotifier,public UdpReadvNotifier{
public:
	UdpTester(string local_ip,string remote_ip,int user_num);

	~UdpTester();
	
	void on_udp_readv(AIO_ECODE ws, aio::CPID cid, const  GMAddrEx &remote_addr,
		GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2);

	void on_udp_writev(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr,
		GMBUF *data_array,U32 array_count,U32 retlen, U64 ctx1, U64 ctx2);
	bool start();
	
	void test();

private:
	UdpCompletionPort m_cp;
	UdpUser** m_udp_users;
	bool m_stoped;
	const U32 m_transfer_len;
	int m_user_num ;
	GMCustomTimerEx<UdpUser> m_user_timer[TIMER_NUM];
public:
	SpeedStat m_send_speed;
	SpeedStat m_send_package_speed;
	SpeedStat m_recv_speed;
	SpeedStat m_recv_package_speed;	
};

class UdpUser{
public:
	//local_port:发送端口，local_port+1:接收端口,remote_port=local_port+1:对端接收端口
	/*	A：6000  send_port				B：6000  send_port
		 ：6001	 recv_port				 ：6001	 recv_port
	*/
	UdpUser(UdpTester* tester,UdpCompletionPort* cp,const char* local_ip,int local_port,const char* remote_ip,int remote_port)
		:m_tester(tester),m_cp(cp),m_local_sent_addr(local_ip,local_port),m_local_recv_addr(local_ip,local_port+1),m_remote_addr(remote_ip,remote_port)
	{
		m_data_array.buf = m_buf;
		m_data_array.len = 1024;
	}

	void recv_data(){
		m_cp->asyn_readv(m_tester,m_recv_id,&m_data_array,1,0,0);
	}

	int send_data(GMCustomTimerCallBackType type,void* prama){
		m_cp->asyn_writev(m_tester,m_sent_id,m_remote_addr,&m_data_array,1,0,0);
		m_timer->SetTimer(18, this, &UdpUser::send_data, NULL);
		return 0;
	}
	GMAddrEx m_local_recv_addr; //接收地址
	CPID m_recv_id;
	GMAddrEx m_local_sent_addr;	//发送地址
	CPID m_sent_id;
	GMAddrEx m_remote_addr;		//目标地址
	GMBUF m_data_array;
	char m_buf[1024];
	GMCustomTimerEx<UdpUser>* m_timer;
	UdpTester* m_tester;
	UdpCompletionPort* m_cp;
};
#endif