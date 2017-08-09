#ifndef RTPPROXY_H__
#define RTPPROXY_H__
#include "rtpproxy_manager.h"
#include "sa_cnn_id.h"
using namespace std;

struct IRPBuildPathCB{
	virtual void on_build_path(RA_ECODE ec,bool suc){};
};
class RtpProxy:public GMSingleTon<RtpProxy>
{
public:
	bool get_ports(U64 sipproxy_id,U64 sipproxy_fake_id,UserPortInfo &info);
	
	void free_ports(U64 sipproxy_id,UserPortInfo& info);

	void asyn_start_call(U64 sipproxy_id,int user_id, PortPair ports,CallInfoInner* call_info,
						 PathInfoInner* path_info, ShortLinkInfo* short_link_info,IRPBuildPathCB* cb);

	

	void asyn_build_path(int user_id,PortPair ports,CallInfoInner* call_info,
						 PathInfoInner* path_info,IRPBuildPathCB* cb);
	void stop_call(U64 sipproxy_id, U64 user_id, PortPair ports, CallInfoInner* call_info);

	void deal_dead_sipproxy(U64 sipproxy_id,U64 sipproxy_fake_id=0);
private:
	void asyn_build_path_inner(CallInfoInner* call_info,PathInfoInner* path_info, PortPair ports,
		IRPBuildPathCB* cb);
};
#endif