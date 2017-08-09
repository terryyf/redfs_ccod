// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


// TODO: 在此处引用程序需要的其他头文件
#include "QNLog/Log_Remote/LogPub.h"
#include <gtest/gtest.h>
#pragma comment (lib, "gtest")
#include "rtpproxy_client/rtpproxy_api_pub.h"

#ifndef LOG_CLD 
#define LOG_CLD  ((I64)1 << 52)
#endif
#define XLOG_MODULE_NAME "RtpproxyClientTester"
#define XLOG_SUB_TYPE LOG_CLD