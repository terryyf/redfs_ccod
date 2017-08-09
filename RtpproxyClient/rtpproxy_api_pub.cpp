#include "StdAfx.h"
#include "asyn_rtpproxy_client.h"
static GMLock g_redfs_init_lock;
static bool g_redfs_is_init = false;
static volatile long g_asyn_rtpproxy_client_ref = 0;


static bool init_asyn_rtpproxy_client(U64 sipproxy_id,ErrorCallCB error_call_cb,
									  ErrorRtpProxysCB error_rtpproxy_cb)
{
	if (g_redfs_is_init)
		return true;

	if (!g_redfs_is_init)
	{
		g_redfs_is_init = RtpproxyAsynClient::Obj()->init(sipproxy_id,error_call_cb,error_rtpproxy_cb);
	}

	return g_redfs_is_init;
}

extern "C" CLIENT_EXPORT IAsynRtpProxyClient* create_asyn_rtpproxy_client(U64 sipproxy_id,
									  ErrorCallCB error_call_cb,ErrorRtpProxysCB error_rtpproxy_cb)
{
	GMAutoLock<GMLock> al(&g_redfs_init_lock);

	if (!init_asyn_rtpproxy_client(sipproxy_id,error_call_cb,error_rtpproxy_cb))
		return NULL;

	++g_asyn_rtpproxy_client_ref;
	return RtpproxyAsynClient::Obj();
}

extern "C" CLIENT_EXPORT void destroy_asyn_rtpproxy_client(IAsynRtpProxyClient *p)
{
	GMAutoLock<GMLock> al(&g_redfs_init_lock);

	if (g_redfs_is_init)
	{
		if (--g_asyn_rtpproxy_client_ref == 0)
		{
			RtpproxyAsynClient::Obj()->uninit();
			g_redfs_is_init = false;
			RtpproxyAsynClient::DestroyInst();
		}
	}
}
