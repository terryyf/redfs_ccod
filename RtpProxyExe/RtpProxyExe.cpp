// StorageAccessExe.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "iostream"
#include <signal.h>
//#include <unistd.h>  
#include <rtp_proxy/rtpproxy_pub.h>
#include <BaseLibrary/CrashProcCtrlStaticDll/CrashProcAPI.h>
#include "BaseLibrary/GMHelper/gmhelper_pub.h"

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "CrashProcCtrlStaticDll.lib")
#define SVCNAME TEXT("RtpProxy")
#define SVC_ERROR ((DWORD)0xC0020001L)
#ifdef WIN32//todo add linux code
SERVICE_STATUS          gSvcStatus; 
SERVICE_STATUS_HANDLE   gSvcStatusHandle; 
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
DWORD WINAPI SvcCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
VOID WINAPI SvcMain(DWORD, LPTSTR *); 

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcReportEvent(LPTSTR);
VOID DoDeleteSvc();
#endif

//void work_proc(void*)
//{
//	if(start_rtp_proxy())
//	{
//		printf("start Storage Access successfully\n");
//	}
//	else
//	{
//		printf("fail to start Storage Access\n");
//		return ;
//	}
//	while(true)
//	{   
//		Sleep(1);
//	}
//}


#ifndef WIN32
void sig_routine(int sig_num)
{
	if (sig_num == SIGHUP)
	{
		stop_rtp_proxy();
		printf("receive a signal %d\n", sig_num);	
	}
}
#endif

int main(int argc, char* argv[])
{
    if(argc > 1 && strcmp(argv[1], "exe") == 0){
        if(start_rtp_proxy())
            printf("start Rtp Proxy successfully\n");
        else
            printf("fail to start Rtp Proxy\n");

        getchar();
        stop_rtp_proxy();
        return 0;
    }

#ifdef WIN32//todo add linux code
    installCrashProcCtrl(0);
	if(lstrcmpi(argv[1], TEXT("install")) == 0){
		SvcInstall();
		return 0;
	}

	SERVICE_TABLE_ENTRY DispatchTable[] = { 
		{ SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain }, 
		{ NULL, NULL } 
	}; 

	if (!StartServiceCtrlDispatcher(DispatchTable)) { 
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher")); 
	} 
#else
	signal(SIGHUP, sig_routine);
	int pid = 0;
        int fd[2];
        int* write_fd = &fd[1];
        int* read_fd = &fd[0];
        if (-1 == pipe(fd)) {
            printf("failed to create pipe\n");
            return 0;
        }

	pid = fork();
	if (pid == 0)
	{
            close(*read_fd);
	    int fd = open("/opt/redcdn/bin/redfs.pid", \
                        O_WRONLY|O_CREAT|O_TRUNC, S_IRWXO|S_IRWXG|S_IRWXU);
            if (-1 == fd) {
                printf("fail to create redfs.pid, errno:%s\n", strerror(errno));
            }
    	    char pid_str[20];
    	    memset(pid_str, 0, sizeof(pid_str));
    	    sprintf(pid_str, "%d", getpid());
    	    write(fd, pid_str, sizeof(pid_str));
    	    close(fd);

	    signal(SIGHUP, sig_routine);
	    if(start_rtp_proxy())
    	    {
        	printf("start Storage Access successfully\n");
                write(*write_fd, "Succ", 4);
                close(*write_fd);
    	    }
    	    else
    	    {
        	printf("fail to start Storage Access\n");
                write(*write_fd, "Fail", 4);
                close(*write_fd);
        	return 0;
    	    }
	    while(true)
	    {
		sleep(INT_MAX);
		printf("this is child process\n");
	    }
	}
        else {
            close(*write_fd);
            char buf[10] = {0};
            read(*read_fd, buf, sizeof(buf));
            printf("parent process:%s\n", buf);
            close(*read_fd);
        }
        //GMThreadEx<std::string,GMTFT_GLOBAL_FUN> thread;
	//thread.create();
	//thread.run(work_proc,NULL);
	//thread.waitThreadExit();
	//if(start_rtp_proxy())
	//	printf("start Storage Access successfully\n");
	//else
	//	printf("fail to start Storage Access\n");

	//getchar();
	//stop_rtp_proxy();
	return 0;
#endif
	//DoDeleteSvc();
	return 0;
}

#ifdef WIN32//todo add linux code
VOID SvcInstall()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
	TCHAR szPath[MAX_PATH];

	if(!GetModuleFileName(NULL, szPath, MAX_PATH))
	{
		printf("Cannot install service (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the SCM database. 
	schSCManager = OpenSCManager( 
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Create the service
	schService = CreateService( 
		schSCManager,              // SCM database 
		SVCNAME,                   // name of service 
		SVCNAME,                   // service name to display 
		SERVICE_ALL_ACCESS,        // desired access 
		SERVICE_WIN32_OWN_PROCESS, // service type 
		SERVICE_AUTO_START,        // start type 
		SERVICE_ERROR_NORMAL,      // error control type 
		szPath,                    // path to service's binary 
		NULL,                      // no load ordering group 
		NULL,                      // no tag identifier 
		NULL,                      // no dependencies 
		NULL,                      // LocalSystem account 
		NULL);                     // no password 

	if (schService == NULL) 
	{
		printf("CreateService failed (%d)\n", GetLastError()); 
		CloseServiceHandle(schSCManager);
		return;
	}
	else 
		printf("Service installed successfully\n"); 

	SERVICE_PRESHUTDOWN_INFO info;
	info.dwPreshutdownTimeout = 300000;
	ChangeServiceConfig2(schService, SERVICE_CONFIG_PRESHUTDOWN_INFO, &info);

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);
}

bool gStopped = false;
DWORD WINAPI ThreadFun(LPVOID lpParam)
{
	stop_rtp_proxy();
	gStopped = true;
	return 0;
}

VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR *lpszArgv)
{
	// Register the handler function for the service
	gSvcStatusHandle = RegisterServiceCtrlHandlerEx(SVCNAME, SvcCtrlHandlerEx, 0);
	if( !gSvcStatusHandle )
	{ 
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler")); 
		return; 
	} 

	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS; 
	gSvcStatus.dwServiceSpecificExitCode = 0;    

	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);// Report initial status to the SCM
	ghSvcStopEvent = CreateEvent(
		NULL,    // default security attributes
		TRUE,    // manual reset event
		FALSE,   // not signaled
		NULL);   // no name

	if ( ghSvcStopEvent == NULL)
	{
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	//start access
	if(start_rtp_proxy())
	{	
		printf("start Storage Access successfully\n");
	}
	else
	{
		printf("fail to start Storage Access\n");
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	// Report running status when initialization is complete.
	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	while(1)
	{
		// Check whether to stop the service.
		WaitForSingleObject(ghSvcStopEvent, INFINITE);
		DWORD dwId;
		CreateThread(NULL, 0, ThreadFun, NULL, 0, &dwId);

		while (!gStopped)
		{
			Sleep(3000);
			ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
		}

		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}
}

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.
	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else 
		gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_PRESHUTDOWN;

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else 
		gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

DWORD WINAPI SvcCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch(dwControl) 
	{  
	case SERVICE_CONTROL_STOP: 
	case SERVICE_CONTROL_PRESHUTDOWN:
		ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
		SetEvent(ghSvcStopEvent);
		return NO_ERROR;
	case SERVICE_CONTROL_INTERROGATE: 
		return NO_ERROR;
	default: 
		return ERROR_CALL_NOT_IMPLEMENTED;
	} 
}

VOID SvcReportEvent(LPTSTR szFunction) 
{ 
	HANDLE hEventSource;
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];

	hEventSource = RegisterEventSource(NULL, SVCNAME);
	if(NULL != hEventSource)
	{
		sprintf_s(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource,        // event log handle
			EVENTLOG_ERROR_TYPE, // event type
			0,                   // event category
			SVC_ERROR,           // event identifier
			NULL,                // no security identifier
			2,                   // size of lpszStrings array
			0,                   // no binary data
			lpszStrings,         // array of strings
			NULL);               // no binary data

		DeregisterEventSource(hEventSource);
	}
}

VOID __stdcall DoDeleteSvc()
{
	SC_HANDLE schSCManager;
	SC_HANDLE schService;
//	SERVICE_STATUS ssStatus; 

	// Get a handle to the SCM database.
	schSCManager = OpenSCManager( 
		NULL,                    // local computer
		NULL,                    // ServicesActive database 
		SC_MANAGER_ALL_ACCESS);  // full access rights 

	if (NULL == schSCManager) 
	{
		printf("OpenSCManager failed (%d)\n", GetLastError());
		return;
	}

	// Get a handle to the service.
	schService = OpenService( 
		schSCManager,       // SCM database 
		SVCNAME,          // name of service 
		DELETE);            // need delete access 

	if (schService == NULL)
	{ 
		printf("OpenService failed (%d)\n", GetLastError()); 
		CloseServiceHandle(schSCManager);
		return;
	}

	// Delete the service.
	if (!DeleteService(schService)) 
	{
		printf("DeleteService failed (%d)\n", GetLastError()); 
	}
	else 
		printf("Service deleted successfully\n"); 

	CloseServiceHandle(schService); 
	CloseServiceHandle(schSCManager);
}
#endif
