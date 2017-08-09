#pragma once
#include "gtest/gtest.h"
#include "rtpproxy_client/rtpproxy_api_pub.h"
class RtpproxyClientTester :public testing::Test,public IRAGetPortCB,public IRAFreePortsCB,
	public IRAStartCallCB,public IRAStopCallCB,public IRABuildPathCB,public IRASipproxyOffLineCB
{
public:
	RtpproxyClientTester(void);
	~RtpproxyClientTester(void);
	virtual void SetUp(){
		U64 sipproxy_id = 20170808;
		m_agent = create_asyn_rtpproxy_client(sipproxy_id,on_error_call_cb,on_error_rtpproxy);
	}
	virtual void TearDown(){
		destroy_asyn_rtpproxy_client(m_agent);
	}

	static void on_error_call_cb(GMAddrEx* rtp_addr,const char* call_id){


	} 
	static void on_error_rtpproxy(GMAddrEx* rtp_addr){

	}

	virtual void on_get_ports(RA_ECODE ec, PortPair ports, int user_id, GMAddrEx* rtp_addr, U64 ctx)
	{
		m_ec = ec;
		if(RA_SUC==ec){
			m_ports = ports;
		}
		m_event.signal();
	}

	virtual void on_free_ports(RA_ECODE ec, int user_id, PortPair ports,GMAddrEx* rtp_addr,U64 ctx){
		m_ec = ec;
		m_event.signal();
	}

	void on_start_call(RA_ECODE ec, bool path_suc, int user_id, PortPair ports, 
		CallInfo* call_info, PathInfo* path_info, 
		ShortLinkInfo* short_link_info, GMAddrEx* rtp_addr, U64 ctx){
			m_ec = ec;
			m_event.signal();
	}

	virtual void on_build_path(RA_ECODE ec,bool path_suc,int user_id,CallInfo* call_info, 
		PathInfo* path_info, GMAddrEx* rtp_addr, U64 ctx){
			m_ec = ec;
			m_event.signal();
	}

	virtual void on_stop_call(RA_ECODE ec, int user_id, PortPair ports, CallInfo* call_info,
		GMAddrEx* rtp_addr, U64 ctx){
			m_ec = ec;
			m_event.signal();
	}

	virtual void on_sipproxy_off_line(RA_ECODE ec,GMAddrEx* rtp_addr, U64 ctx){
		m_ec = ec;
		m_event.signal();
	}

	IAsynRtpProxyClient* m_agent;
	GMSysEvent	m_event;
	PortPair m_ports;
	GMAddrEx m_rtp_addr;
	int m_user_id;
	RA_ECODE m_ec;
	CallInfo m_call_info;
	PathInfo m_path_info;
	ShortLinkInfo m_short_link;
	GMAddrEx m_report_addr;
	GMAddrEx m_dest_rtp_addr;
};

//不启动Rtpproxy，测超时
//启动Rtpproxy,测接口
TEST_F(RtpproxyClientTester,InterfaceTest)
{
	m_agent->asyn_get_ports(m_user_id,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	
	m_agent->asyn_start_call(m_user_id,m_ports,&m_call_info,&m_path_info,&m_short_link,&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	
	m_agent->asyn_build_path(m_user_id,m_ports,&m_call_info,&m_path_info,&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	m_agent->asyn_stop_call(m_user_id,m_ports,&m_call_info,&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	m_agent->asyn_free_ports(m_user_id,&m_rtp_addr,m_ports,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	m_agent->asyn_sipproxy_off_line(&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	
};

TEST_F(RtpproxyClientTester,StartStopCallTest){
	m_agent->asyn_get_ports(m_user_id,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	m_agent->asyn_start_call(m_user_id,m_ports,&m_call_info,&m_path_info,&m_short_link,&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	//未stop,调用free

	m_agent->asyn_free_ports(m_user_id,&m_rtp_addr,m_ports,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	//未stop，调用off_line
	m_agent->asyn_sipproxy_off_line(&m_rtp_addr,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
}

TEST_F(RtpproxyClientTester,GetPortsTest){
	m_agent->asyn_get_ports(m_user_id,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	PortPair ports = m_ports;	

	//相同用户获取多次ports
	m_agent->asyn_get_ports(m_user_id,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	ASSERT_EQ(ports,m_ports);

	m_agent->asyn_get_ports(m_user_id,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	ASSERT_EQ(ports,m_ports);
	
	int user_id1 = m_user_id+1;
	m_agent->asyn_get_ports(user_id1,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	PortPair ports1= m_ports;


	int user_id2 = user_id1+1;
	m_agent->asyn_get_ports(user_id2,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	PortPair ports2= m_ports;

	//超过空闲ports数量的获取，获取失败
	int user_id3 = user_id2+1;
	m_agent->asyn_get_ports(user_id3,&m_rtp_addr,this,-1);
	m_event.wait();
	EXPECT_EQ(RA_FAIL,m_ec);

	//释放端口
	m_agent->asyn_free_ports(m_user_id,&m_rtp_addr,ports,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);
	
	m_agent->asyn_free_ports(user_id1,&m_rtp_addr,ports1,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	m_agent->asyn_free_ports(user_id2,&m_rtp_addr,ports2,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

	//重复释放,返回成功

	m_agent->asyn_free_ports(user_id2,&m_rtp_addr,ports2,this);
	m_event.wait();
	EXPECT_EQ(RA_SUC,m_ec);

}

