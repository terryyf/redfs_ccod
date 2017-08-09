#ifndef RTPPROXY_CLIENT_STDAFX_H
#define RTPPROXY_CLIENT_STDAFX_H

#include <BaseLibrary/GMHelper/gmhelper_pub.h>
#include <XTools/XToolsPub.h>
#include <aio_basic/CompletionPortPub.h>

#ifdef WIN32
#define CLIENT_EXPORT  __declspec(dllexport) 
#else
#define CLIENT_EXPORT __attribute__((visibility("default")))
#endif

#define LOG_SA (U64)1<<52

#define XLOG_MODULE_NAME "RTPPROXY_API"
#define XLOG_SUB_TYPE LOG_SA

#ifndef WIN32
typedef unsigned long DWORD;
#endif

#endif
