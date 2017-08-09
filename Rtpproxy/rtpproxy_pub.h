#ifndef RTP_PROXY_PUB
#define RTP_PROXY_PUB
#include <XTools/XToolsPub.h>

#ifdef WIN32
#define RTP_PROXY_API __declspec(dllimport)
#pragma comment(lib,"rtp_proxy.lib")
#else
#define RTP_PROXY_API
#endif

extern "C" RTP_PROXY_API bool start_rtp_proxy();

extern "C" RTP_PROXY_API void stop_rtp_proxy();

#endif

