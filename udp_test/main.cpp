// udp_test.cpp : 定义控制台应用程序的入口点。
//
#include "rtpproxy_client/rtpproxy_api_pub.h"
#include "stdafx.h"
#include "udp_test.h"
int _tmain(int argc, _TCHAR* argv[])
{
	Log::open(true,(char*)"-dGMfyds",false);
    testing::InitGoogleTest(&argc,argv);
    for(int i=0;i<1;++i){
        RUN_ALL_TESTS();
        Sleep(1000);
    }
    getchar();
   
    return 0;

	UdpTester tester("127.0.0.1",600);
	bool ret = tester.start();
	tester.test();
	while(1){
		Sleep(1000);
		printf("send %llu KB recv %llu KB send package %llu recv package %llu \n",
				tester.m_send_speed.get_speed()/1024,tester.m_recv_speed.get_speed()/1024,
				tester.m_send_package_speed.get_speed(),tester.m_recv_package_speed.get_speed());
	}
	return 0;
}

