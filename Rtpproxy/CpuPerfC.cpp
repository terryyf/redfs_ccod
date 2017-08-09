#include "stdafx.h"
#include <atlbase.h>	// for CRegKey use
#include "CpuPerfC.h"

bool CpuCounter::init()
{
#ifdef WIN32
	m_lStatus = PdhOpenQuery(NULL, NULL, &m_hQuery);

	if (m_lStatus != ERROR_SUCCESS)
	{
		printf("PdhOpenQuery failed,last_error %d \n",GetLastError());
		return false;  
	}

	PDH_COUNTER_PATH_ELEMENTS elements;
	char szBuf[1024] = "";
	DWORD dwBufSize = 0;

	elements.szMachineName = NULL;
	elements.szObjectName = "Processor";
	elements.szInstanceName = TEXT("_Total");
	elements.szCounterName = TEXT("% Processor Time");
	elements.szParentInstance = NULL;
	elements.dwInstanceIndex = -1;
	dwBufSize = sizeof(szBuf);
	PdhMakeCounterPath(&elements, szBuf, &dwBufSize, 0);
	m_lStatus = PdhAddCounter(m_hQuery, szBuf, NULL, &m_hcCPU);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhAddCounter  Processor Time failed,last_error %d \n",GetLastError());
		return false;
	}
	elements.szCounterName = TEXT("% User Time");
	dwBufSize = sizeof(szBuf);
	PdhMakeCounterPath(&elements, szBuf, &dwBufSize, 0);
	m_lStatus = PdhAddCounter(m_hQuery, szBuf, NULL, &m_hcUserTime);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhAddCounter  User Time failed,last_error %d \n",GetLastError());
		return false;
	}

	elements.szCounterName = TEXT("% Privileged Time");
	dwBufSize = sizeof(szBuf);
	PdhMakeCounterPath(&elements, szBuf, &dwBufSize, 0);
	m_lStatus = PdhAddCounter(m_hQuery, szBuf, NULL, &m_hcSysTime);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhAddCounter  Privileged Time failed,last_error %d \n",GetLastError());
		return false;
	}

	elements.szCounterName = TEXT("% Idle Time");
	dwBufSize = sizeof(szBuf);
	PdhMakeCounterPath(&elements, szBuf, &dwBufSize, 0);
	m_lStatus = PdhAddCounter(m_hQuery, szBuf, NULL, &m_hcIdleTime);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhAddCounter  Idle Time failed,last_error %d \n",GetLastError());
		return false;
	}
#endif

	return true;
}

void CpuCounter::get_cpu_info( CpuInfo& cpu_info )
{
#ifdef WIN32
	m_lStatus = PdhCollectQueryData(m_hQuery);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhCollectQueryData  Processor failed,last_error %d \n",GetLastError());
		return ;
	}
	Sleep(2000);

	m_lStatus = PdhCollectQueryData(m_hQuery);
	if (m_lStatus != ERROR_SUCCESS){
		printf("PdhCollectQueryData  Processor failed,last_error %d \n",GetLastError());
		return ;
	}

	// 空闲时间
	m_lStatus = PdhGetFormattedCounterValue(m_hcIdleTime, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &m_cv);
	if (m_lStatus == ERROR_SUCCESS)
	{
		cpu_info.m_idle_time = m_cv.doubleValue;
	}
	// 用户态时间
	m_lStatus = PdhGetFormattedCounterValue(m_hcUserTime, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &m_cv);
	if (m_lStatus == ERROR_SUCCESS)
	{
		cpu_info.m_user_time = m_cv.doubleValue;
	}
	// 内核态时间
	m_lStatus = PdhGetFormattedCounterValue(m_hcSysTime, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &m_cv);
	if (m_lStatus == ERROR_SUCCESS)
	{
		cpu_info.m_sys_time = m_cv.doubleValue;
	}

	// CPU时间，注意必须加上PDH_FMT_NOCAP100参数，否则多核CPU会有问题，详见MSDN
	m_lStatus = PdhGetFormattedCounterValue(m_hcCPU, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, NULL, &m_cv);
	if (m_lStatus == ERROR_SUCCESS)
	{
		cpu_info.m_process_time = m_cv.doubleValue;
	}
#else
	m_fp = fopen("/proc/stat", "r");
	if(m_fp == NULL) {
		perror("Open /proc/stat failed.\n");
		return;
	}

	char buf[128];
	char cpu[5];
	long int user,nice,sys,idle,iowait,irq,softirq;
	long int all1,all2,idle1,idle2;
	float usage;

	fgets(buf, sizeof(buf), m_fp);
	sscanf(buf,"%s%d%d%d%d%d%d%d",cpu,&user,&nice,&sys,&idle,&iowait,&irq,&softirq);

	/*第二次取数据*/
	Sleep(2000);
	memset(buf,0,sizeof(buf));
	cpu[0] = '\0';
	user=nice=sys=idle=iowait=irq=softirq=0;

	fgets(buf,sizeof(buf),m_fp);
	sscanf(buf,"%s%d%d%d%d%d%d%d",cpu,&user,&nice,&sys,&idle,&iowait,&irq,&softirq);

	all2 = user+nice+sys+idle+iowait+irq+softirq;
	idle2 = idle;

	float used = all2-all1-(idle2-idle1);
	float usage_per = used / (all2-all1)*100;

	cpu_info.m_user_time = user / 100.0;
	cpu_info.m_sys_time = sys / 100.0;
	cpu_info.m_idle_time = idle / 100.0;
	cpu_info.m_iowait_time = iowait / 100.0;
	cpu_info.m_process_time = used / 100.0;
	fclose(m_fp);
#endif
}

void CpuCounter::uninit()
{
#ifdef WIN32
	if(NULL!=m_hcUserTime)
		PdhRemoveCounter(m_hcUserTime);
	if(NULL!=m_hcSysTime)
		PdhRemoveCounter(m_hcSysTime);
	if(NULL!=m_hcIdleTime)
		PdhRemoveCounter(m_hcIdleTime);
	if(NULL!=m_hcCPU)
		PdhRemoveCounter(m_hcCPU);
	if(NULL!=m_hQuery)
		PdhCloseQuery(m_hQuery);
#endif
}
