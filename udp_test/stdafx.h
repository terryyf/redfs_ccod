// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>


// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�
#include "QNLog/Log_Remote/LogPub.h"
#include <gtest/gtest.h>
#pragma comment (lib, "gtest")
#include "rtpproxy_client/rtpproxy_api_pub.h"

#ifndef LOG_CLD 
#define LOG_CLD  ((I64)1 << 52)
#endif
#define XLOG_MODULE_NAME "RtpproxyClientTester"
#define XLOG_SUB_TYPE LOG_CLD