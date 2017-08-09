#include "stdafx.h"
#include "udp_test.h"
const U16 svr_base_port = 6000;

void UdpTester::on_udp_writev(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr, GMBUF *data_array,U32 array_count,U32 retlen, U64 ctx1, U64 ctx2)
{
	/*CPID id = (CPID)ctx1;
	asyn_readv(this,id,data_array,array_count,0,0);*/
	m_send_speed.count_new_value(retlen);
	m_send_package_speed.count_new_value(1);
}

bool UdpTester::start()
{
	bool ret = m_cp.start();
	CPID cp_id=0,cp_id1=0;
	AIO_ECODE ec ,ec1;
	for (int i=0;i<m_user_num;++i){
		ec = m_cp.add_socket(cp_id,m_udp_users[i]->m_svr_recv_addr);		
		ec1 = m_cp.add_socket(cp_id1,m_udp_users[i]->m_svr_sent_addr);
		if(AIO_SUC!=ec || AIO_SUC!=ec1){
			printf("m_cp.add_socket ec %d m_svr_recv_addr port %d ec1 %d m_svr_sent_addr port %d",
				ec,m_udp_users[i]->m_svr_recv_addr.m_port,ec1,m_udp_users[i]->m_svr_sent_addr.m_port);
			m_cp.close_all_sockets();
			return false;
		}
		m_udp_users[i]->m_recv_id = cp_id;
		m_udp_users[i]->m_sent_id = cp_id1;
		m_udp_users[i]->m_timer = &m_user_timer[cp_id1%TIMER_NUM];
	}		

	for(int i=0;i<TIMER_NUM;i++){
		m_user_timer[i].Enable();
	}		
	m_send_speed.start();
	m_send_package_speed.start();
	m_recv_speed.start();
	m_recv_package_speed.start();
	return true;
}

void UdpTester::test()
{
	int dest = m_user_num/2;
	for (int i=0;i<m_user_num/2;++i){
		m_udp_users[i]->m_remote_addr= m_udp_users[dest+i]->m_svr_recv_addr;
		m_udp_users[dest+i]->m_remote_addr = m_udp_users[i]->m_svr_recv_addr;
	}

	for(int i=0;i<m_user_num;++i){
		m_udp_users[i]->recv_data();
	}
	for(int i=0;i<m_user_num;++i){
		m_udp_users[i]->send_data(GMCustomTimerDue,0);
	}
}

UdpTester::UdpTester(string svr_ip,int user_num) :m_transfer_len(700),m_stoped(false),m_user_num(user_num)
{
	m_udp_users = new UdpUser*[user_num];
	int port = svr_base_port;
	for (int i=0;i<user_num;++i)
	{
		m_udp_users[i] = new UdpUser(this,&m_cp,svr_ip.c_str(),port);
		port+=2;
	}
}

UdpTester::~UdpTester()
{
	m_send_speed.stop();
	m_recv_speed.stop();
	m_send_package_speed.stop();
	m_recv_package_speed.stop();

	if(m_udp_users){
		for (int i=0;i<m_user_num;++i)
		{
			if(m_udp_users[i])
				delete m_udp_users[i];
		}
		delete[] m_udp_users;
	}
}

void UdpTester::on_udp_readv(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr, GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2)
{
	m_cp.asyn_readv(this,cid,data_array,array_count,0,0);
	m_recv_speed.count_new_value(retlen);
	m_recv_package_speed.count_new_value(1);
	/*	remote_addr.m_port+=1;
	asyn_writev(this,m_sent_id,remote_addr,data_array,array_count,cid,0);*/
}
