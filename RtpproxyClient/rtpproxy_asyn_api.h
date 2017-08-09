#ifndef CCOD_RTPPROXYAGENT_ASYN_API_H
#define CCOD_RTPPROXYAGENT_ASYN_API_H
#include "user_definitions.h"
#include "rtpproxy_ecode.h"

struct IRAGetPortCB{
	virtual void on_get_ports(RA_ECODE ec, PortPair ports, int user_id, GMAddrEx* rtp_addr, U64 ctx){}
};

struct IRAFreePortsCB{
	virtual void on_free_ports(RA_ECODE ec, int user_id, PortPair ports,GMAddrEx* rtp_addr,U64 ctx){}
};

/* path_suc：true-建路成功；false-建路失败*/
struct IRAStartCallCB{
	virtual void on_start_call(RA_ECODE ec, bool path_suc, int user_id, PortPair ports, 
							   CallInfo* call_info, PathInfo* path_info, 
							   ShortLinkInfo* short_link_info, GMAddrEx* rtp_addr, U64 ctx){}
};

struct IRABuildPathCB{
	virtual void on_build_path(RA_ECODE ec,bool path_suc,int user_id,CallInfo* call_info, 
								PathInfo* path_info, GMAddrEx* rtp_addr, U64 ctx){}
};

struct IRAStopCallCB{
	virtual void on_stop_call(RA_ECODE ec, int user_id, PortPair ports, CallInfo* call_info,
							  GMAddrEx* rtp_addr, U64 ctx){}
};

struct IRASipproxyOffLineCB{
	virtual void on_sipproxy_off_line(RA_ECODE ec,GMAddrEx* rtp_addr, U64 ctx){}
};

//异常呼叫回调函数
typedef void (*ErrorCallCB) (GMAddrEx* rtp_addr,const char* call_id);
//异常Rtpproxy回调函数
typedef void (*ErrorRtpProxysCB) (GMAddrEx* rtp_addr);

class IAsynRtpProxyClient{
public:
	/**
     * 初始化
     *  
     * @param sipproxy_id            标识不同sipproxy的ID
	 * @param error_call_cb			 错误呼叫回调函数
     * @param error_rtpproxy_cb		 异常rtpproxy回调函数 
     * 备注: 
     *    
     */
	virtual bool init(U64 sipproxy_id,ErrorCallCB error_call_cb,ErrorRtpProxysCB error_rtpproxy_cb)=0;

	/**
     * 初始化
     *  
     * @param 
	 *
	 * @return 
	 *
     * 备注: 
     *    
     */
	virtual void uninit()=0;

	/**
     * 获取端口
     *  
     * @param user_id			标识用户的id
	 * @param rtp_addr          rtpproxy的地址信息(ip和port)
	 * @param cb				回调通知对象
	 * @param timeout			任务超时时间
	 * @param ctx				用户上下文
	 * @return 
	 *
     * 备注: 
     *    
     */
	virtual void asyn_get_ports(int user_id, GMAddrEx* rtp_addr,IRAGetPortCB* cb,  
								U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0)=0;

	/**
     * 释放端口
     *  
     * @param user_id			标识用户的id
	 * @param rtp_addr          rtpproxy的地址信息(ip和port)
	 * @param cb				回调通知对象
	 * @param timeout			任务超时时间
	 * @param ctx				用户上下文
	 * @return 
	 *
     * 备注: 
     *    
     */
	virtual void asyn_free_ports(int user_id, GMAddrEx* rtp_addr, PortPair ports,
								 IRAFreePortsCB* cb, U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0)=0;
	/**
     * 开始呼叫+建路
     *  
     * @param user_id			标识用户的id
	 * @param ports				媒体udp端口信息
	 * @param call_info			呼叫信息
	 * @param path_info			建路信息
	 * @param rtp_addr          rtpproxy的地址信息(ip和port)
	 * @param cb				回调通知对象
	 * @param timeout			任务超时时间
	 * @param ctx				用户上下文
	 *
	 * @return 
	 *
     * 备注: 
     *    
	 */
	 
	virtual void asyn_start_call(int user_id, PortPair ports,CallInfo* call_info, PathInfo* path_info,
								 ShortLinkInfo* short_link_info, GMAddrEx* rtp_addr, 
								 IRAStartCallCB* cb, U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0)=0;

	virtual void asyn_build_path(int user_id,PortPair ports,CallInfo* call_info,PathInfo* path_info,
								 GMAddrEx* rtp_addr, IRABuildPathCB* cb, U32 timeout=DEFAULT_TIMEOUT,
								 U64 ctx=0 )=0;						 

	/**
     * 结束呼叫
     *  
     * @param user_id			标识用户的id
	 * @param ports				媒体udp端口信息	
	 * @param call_info			呼叫信息
	 * @param rtp_addr          rtpproxy的地址信息(ip和port)
	 * @param cb				回调通知对象
	 * @param timeout			任务超时时间
	 * @param ctx				用户上下文
	 *
	 * @return 
	 *
     * 备注: 
     *    
	 */
	virtual void asyn_stop_call(int user_id,  PortPair ports, CallInfo* call_info,GMAddrEx* rtp_addr, 
								IRAStopCallCB* cb, U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0)=0;
		
	virtual void asyn_sipproxy_off_line(GMAddrEx* rtp_addr, IRASipproxyOffLineCB* cb,
										U32 timeout=DEFAULT_TIMEOUT, U64 ctx=0)=0;
};
#endif