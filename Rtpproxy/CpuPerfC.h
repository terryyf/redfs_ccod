#ifndef _CPUPERFC_H
#define _CPUPERFC_H

#ifdef WIN32
#include "io.h"
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#endif

class CpuInfo{
public:
	CpuInfo():m_user_time(0.0),m_sys_time(0.0),m_idle_time(0.0),m_iowait_time(0.0),m_process_time(0.0){}
	float m_user_time;//用户态时间
	float m_sys_time;//系统内核时间
	float m_idle_time;//空闲时间
	float m_iowait_time;//iowait时间
	float m_process_time;//已使用cpu
};

class CpuCounter{
public:
#ifdef WIN32
	CpuCounter():m_hQuery(NULL),m_hcUserTime(NULL),m_hcSysTime(NULL),m_hcIdleTime(NULL){};
#else
	CpuCounter(): m_fp(NULL) {};
#endif

	bool init();
	void get_cpu_info(CpuInfo& cpu_info);
	void uninit();

private:
#ifdef WIN32
	HQUERY  m_hQuery;
	HCOUNTER m_hcUserTime, m_hcSysTime;
	HCOUNTER m_hcIdleTime;
	HCOUNTER m_hcCPU;
	PDH_FMT_COUNTERVALUE m_cv;
	PDH_STATUS m_lStatus ;
#else
	FILE* m_fp;
#endif
};
#endif