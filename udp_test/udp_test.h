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
	UdpTester(string svr_ip,int user_num);

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
	UdpUser(UdpTester* tester,UdpCompletionPort* cp,const char* ip,int port)
		:m_tester(tester),m_cp(cp),m_svr_sent_addr(ip,port),m_svr_recv_addr(ip,port+1)
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
	GMAddrEx m_svr_recv_addr;
	CPID m_recv_id;
	GMAddrEx m_svr_sent_addr;
	CPID m_sent_id;
	GMAddrEx m_remote_addr;
	GMBUF m_data_array;
	char m_buf[1024];
	GMCustomTimerEx<UdpUser>* m_timer;
	UdpTester* m_tester;
	UdpCompletionPort* m_cp;
};
#endif