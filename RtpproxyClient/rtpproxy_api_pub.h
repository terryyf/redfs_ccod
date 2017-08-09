#include <BaseLibrary/GMHelper/gmhelper_pub.h>
#include <XTools/XToolsPub.h>
#include <aio_basic/CompletionPortPub.h>

#include "rtpproxy_asyn_api.h"
#pragma comment (lib, "rtpproxy_client.lib")

#ifdef WIN32
#define DLLIMPORT __declspec(dllimport)
#else
#define DLLIMPORT
#endif


/**
 * �����첽client
 *  
 * 
 * @return IAsynRtpProxyAgent*  
 */
extern "C" DLLIMPORT IAsynRtpProxyClient* create_asyn_rtpproxy_client(U64 sipproxy_id,ErrorCallCB error_call_cb,ErrorRtpProxysCB error_rtpproxy_cb);

/**
 * �����첽client 
 *  
 * @param agent �����ٵ�agentʵ�� 
 */
extern "C" DLLIMPORT void destroy_asyn_rtpproxy_client(IAsynRtpProxyClient *agent);

 