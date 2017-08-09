#ifndef CnnMixer_h__
#define CnnMixer_h__

#include "sa_cnn_id.h"
#include "con_address.h"
#include <aio_basic/CompletionPortPub.h>
#include "rtpproxy_cmd.h"
#ifdef WIN32
#include <iphlpapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#else
#include <ifaddrs.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#endif

#ifndef cp
#define cp aio
#endif

enum CnnMixerError
{
	CME_SUC = 0,
	CME_UNKONW_ERROR,
	CME_SYSTEM_BUSY,
	CME_FATAL_ERROR,
	CME_TIME_OUT,
	CME_INVALID_PARAMETER,
};


class CnnMixer:private cp::TcpAcceptNotifier,private cp::TcpConnectNotifier,
			   private cp::TcpWritevNotifier,private cp::TcpReadvNotifier,
			   private cp::UdpWritevNotifier,private cp::UdpReadvNotifier
{
public:
	CnnMixer();

	bool start_tcp(U32 pop_cp_thread_num = 8, U32 concurrent_thread_num = 0, U32 concurrent_packet_num = 1,
        U32 post_cp_thread_num = 8,U32 notify_thread_num=8){
            return m_tcp_complete_port.start(this,pop_cp_thread_num,concurrent_thread_num,concurrent_packet_num,
                post_cp_thread_num,notify_thread_num);
    }

    bool add_tcp_listen_socket(GMAddrEx &addr, U32 wait_con_num){
        return m_tcp_complete_port.add_listen_socket(addr,wait_con_num);
    }

    void get_tcp_listen_addrs(XList<GMAddrEx>& addrs){
        m_tcp_complete_port.get_listen_addrs(addrs);
    }

	bool start_udp(U32 work_thread_num=0, U32 concurrent_thread_num=0, U32 concurrent_packet_num=1){
		return m_udp_complete_port.start(work_thread_num,concurrent_thread_num,concurrent_packet_num);
	}

	bool add_udp_socket(aio::CPID& id,GMAddrEx &addr){
		return AIO_SUC==m_udp_complete_port.add_socket(id,addr);
	}

	void del_udp_socket(aio::CPID id){
		m_udp_complete_port.del_socket(id);
	}

    void stopAll(){
        m_tcp_complete_port.stop();
		m_udp_complete_port.stop();
    }

	//////////////////////////////////////////////////////////////////////////
	bool connnect(SACnnID& cid,const ConAddress& raddr,U32 waitTimout = INFINITE);

	void closeConnection(const SACnnID& cid);

	void asynReadConnection(const SACnnID& cid,char* buf,U32 willReadLen);

	void asynWriteConnection(const SACnnID& cid,GMBUF* buf_array,U32 array_num,U64 ctx1, U64 ctx2);

	void asynWriteUdpConnection(const SACnnID& cid,GMAddrEx& addr,GMBUF* buf_array,U32 array_num,U64 ctx1, U64 ctx2);

	bool get_normalips(std::list<GMIP> &normal_addrs);

	virtual void on_read_connection(CnnMixerError cme,const SACnnID& cid,const GMAddrEx& remote_addr,char* buf, U32 req_len, U32 resp_len)=0;

	virtual void on_write_connection(CnnMixerError cme,const SACnnID& cid, GMBUF* buf_array, U32 array_num, U32 resp_len, U64 ctx1, U64 ctx2)=0;

	virtual void on_accept_connection(const SACnnID& id)=0;
	
private:
	//////////////////////////////////////////////////////////////////////////
    CnnMixerError TcpWriteState_to_CnnMixerError(cp::TcpWriteState ws)
    {
        switch(ws)
        {
        case cp::TcpWrite_Suc:
            return CME_SUC;
        case cp::TcpWrite_Failed:
            return CME_FATAL_ERROR;
        case cp::TcpWrite_Illegal_Buf:
            return CME_INVALID_PARAMETER;
        case cp::TcpWrite_Timeout:
            return CME_TIME_OUT;
        default:
            return CME_UNKONW_ERROR;
        }
    }

    virtual void onTcpWriteFinished(cp::TcpWriteState ws,cp::CPID cpid,const GMTcpAddrEx& addr,char* data,U32 wantWriteLen,U32 actuallen,U64 userContext1,U64 userContext2){
        GMBUF buf_array(data,wantWriteLen);
        CnnMixerError ec = TcpWriteState_to_CnnMixerError(ws);
        on_write_connection(ec,SACnnID(SIT_TCP,cpid),&buf_array,1,actuallen,userContext1,userContext2);
    }

    virtual void on_tcp_writev(cp::TcpWriteState st, cp::CPID id, const GMTcpAddrEx &addr, GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2)
    {
        CnnMixerError ec = TcpWriteState_to_CnnMixerError(st);
        on_write_connection(ec,SACnnID(SIT_TCP,id),data_array,array_count,retlen,ctx1,ctx2);
    }

    virtual void on_tcp_readv(cp::TcpReadState rs,cp::CPID cpid, const GMTcpAddrEx &addr,GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2);

    virtual void on_tcp_connect(cp::TcpConnectState cs,cp::CPID cpid,const GMTcpAddrEx& addr,U64 userContext1,U64 userContext2){assert(false);}

    virtual void on_tcp_accept(cp::TcpAcceptState as,cp::CPID cpid,const GMTcpAddrEx& addr);

	virtual void on_udp_readv(AIO_ECODE ws, aio::CPID cid, const  GMAddrEx &remote_addr,
							  GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2);
	virtual void on_udp_writev(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr,
							   GMBUF *data_array,U32 array_count,U32 retlen, U64 ctx1, U64 ctx2);
private:
	U32 m_head_len;
	volatile long long m_write_times;
	volatile long long m_on_write_times;
    cp::TcpCompletionPort m_tcp_complete_port;
	cp::UdpCompletionPort m_udp_complete_port;
};

inline CnnMixer::CnnMixer():m_write_times(0),m_on_write_times(0)
{
	CmdHead head;
	char buf[1024]={0};
	XSpace xs(buf,sizeof(buf)); 
	head.ser(xs);
	m_head_len = xs.used_size(); 
}


inline void CnnMixer::asynWriteConnection(const SACnnID& cid,GMBUF* buf_array,U32 array_num,U64 ctx1, U64 ctx2)
{
	m_tcp_complete_port.asyn_writevex(this,cid.m_id,buf_array,array_num,ctx1,ctx2);
}

inline void CnnMixer::asynWriteUdpConnection(const SACnnID& cid,GMAddrEx& addr,GMBUF* buf_array,
											 U32 array_num,U64 ctx1, U64 ctx2)
{
	m_udp_complete_port.asyn_writev(this,cid.m_id,addr,buf_array,array_num,ctx1,ctx2);
}

inline bool CnnMixer::connnect(SACnnID& cid,const ConAddress& raddr,U32 timeout)
{
	cid.m_tp = SIT_TCP;
	cid.m_id = m_tcp_complete_port.connect(raddr.m_raddr,raddr.m_laddr);
	return cid.m_id > 0;
}

inline void CnnMixer::closeConnection(const SACnnID& cid){
   if (SIT_TCP == cid.m_tp && 0 != cid.m_id) {
#ifdef WIN32
	   m_tcp_complete_port.close_connection(cid.m_id,true);
#else
	   m_tcp_complete_port.close_connection(cid.m_id);
#endif
    }else {
		//TODO
    }
}

inline void CnnMixer::asynReadConnection(const SACnnID& cid,char* buf,U32 willReadLen){
	GMBUF data(buf,m_head_len);   
	if (SIT_UDP == cid.m_tp) {
        m_udp_complete_port.asyn_readv(this,cid.m_id,&data,1,0,willReadLen);
    }
    else if(SIT_TCP == cid.m_tp){
        m_tcp_complete_port.asyn_readv(this,cid.m_id,&data,1,1,willReadLen,true);//1表示读取head
    }
    else
        assert(false);
}

inline void CnnMixer::on_tcp_readv(cp::TcpReadState rs, cp::CPID cpid, const GMTcpAddrEx &addr,GMBUF *data_array, U32 array_count, U32 resplen, U64 ctx1,U64 ctx2)
{
    //暂时只处理array_count=1的情况
    char* buf = data_array[0].buf;
    U32 reqlen = data_array[0].len;
    if (cp::TcpRead_Suc != rs || reqlen != resplen){
        if (1 == ctx1)
            return on_read_connection(CME_FATAL_ERROR,SACnnID(SIT_TCP,cpid),addr.m_remoteAddr,buf,static_cast<U32>(ctx2),0);
        else
            return on_read_connection(CME_FATAL_ERROR,SACnnID(SIT_TCP,cpid),addr.m_remoteAddr,buf-m_head_len,static_cast<U32>(ctx2),0);
    }

    if (1 == ctx1)//读取头部完成
    {
        try
        {
            XSpace xs(buf,resplen);
            CmdHead head;
            head.unser(xs);
            if (head.m_body_len > 0){
                GMBUF data(buf+resplen,head.m_body_len);
                m_tcp_complete_port.asyn_readv(this,cpid,&data,1,0,ctx2,true);
            }else{
                on_read_connection(CME_SUC, SACnnID(SIT_TCP,cpid),addr.m_remoteAddr, buf, static_cast<U32>(ctx2), resplen);
			}
        }catch (exception&){
            on_read_connection(CME_FATAL_ERROR,SACnnID(SIT_TCP,cpid),addr.m_remoteAddr,buf,static_cast<U32>(ctx2),0);
        }
    }else{
        on_read_connection(CME_SUC,SACnnID(SIT_TCP,cpid),addr.m_remoteAddr,buf-m_head_len,static_cast<U32>(ctx2),resplen+m_head_len);
    }
}

inline void CnnMixer::on_tcp_accept(cp::TcpAcceptState as,cp::CPID cpid,const GMTcpAddrEx& addr)
{
    if(cp::TcpAccept_Suc == as)
        on_accept_connection(SACnnID(SIT_TCP, cpid));
}

inline bool CnnMixer::get_normalips(std::list<GMIP> &normal_addrs)
{
#ifdef WIN32
    PIP_ADAPTER_INFO adapterinfo;
    PIP_ADAPTER_INFO adapter = NULL;
    PIP_ADDR_STRING addr = NULL;
    ULONG outbuflen = sizeof(IP_ADAPTER_INFO);
    adapterinfo = (IP_ADAPTER_INFO *)malloc(sizeof(IP_ADAPTER_INFO));
    if (adapterinfo == NULL){
        Log::writeError(LOG_SA, 1, "CnnMixer::get_normalips Error allocating memory needed to call GetAdaptersinfo");
        return false;
    }

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable
    if (GetAdaptersInfo(adapterinfo, &outbuflen) == ERROR_BUFFER_OVERFLOW){
        free(adapterinfo);
        adapterinfo = (IP_ADAPTER_INFO *) malloc(outbuflen);
        if (adapterinfo == NULL){
            Log::writeError(LOG_SA,1,"CnnMixer::get_normalips Error allocating memory needed to call GetAdaptersinfo");
            return false;
        }
    }

    DWORD retval = GetAdaptersInfo(adapterinfo, &outbuflen);
    if (retval == NO_ERROR){
        adapter = adapterinfo;
        while (adapter) 
        {
            if (strstr(adapter->Description, "IPoIB") == NULL){	
                addr = &adapter->IpAddressList;
                while(addr){
                    if (strcmp(addr->IpAddress.String, "0.0.0.0") != 0 && 
                        strcmp(addr->IpAddress.String, "127.0.0.1") != 0){
                            normal_addrs.push_back(ntohl(inet_addr(addr->IpAddress.String)));
                    }
                    addr = addr->Next;
                }
            }

            adapter = adapter->Next;
        }
    }

    free(adapterinfo);
#else
    struct ifaddrs *ifAddrStruct;
    void *tmpAddrPtr;
    getifaddrs(&ifAddrStruct);
    getifaddrs(&ifAddrStruct);
    char tmp_ip[INET_ADDRSTRLEN];

    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr->sa_family==AF_INET) {
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, tmp_ip, INET_ADDRSTRLEN);
            if (strcmp(tmp_ip, "127.0.0.1") != 0) {
                T_IP tip = {0};
                strcpy(tip, tmp_ip);
                GMIP ip = GMConvertIP_a2n(tip); 
                normal_addrs.push_back(ip);
            }
        }
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    freeifaddrs(ifAddrStruct);
#endif
    return true;
}

inline void CnnMixer::on_udp_readv(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr,
								   GMBUF *data_array, U32 array_count, U32 retlen, U64 ctx1, U64 ctx2)
{
	char* buf = data_array[0].buf;
	U32 reqlen = data_array[0].len;
	if (AIO_SUC != ws ){
		on_read_connection(CME_FATAL_ERROR,SACnnID(SIT_UDP,cid),remote_addr,buf,reqlen,0);
	}else{
		on_read_connection(CME_SUC,SACnnID(SIT_UDP,cid),remote_addr,buf,reqlen,retlen);
	}
}

inline void CnnMixer::on_udp_writev(AIO_ECODE ws, aio::CPID cid, const GMAddrEx &remote_addr,
									GMBUF *data_array,U32 array_count,U32 retlen, U64 ctx1, U64 ctx2)
{
	CnnMixerError ec = ws==AIO_SUC?CME_SUC:CME_FATAL_ERROR;
	on_write_connection(ec,SACnnID(SIT_UDP,cid),data_array,array_count,retlen,ctx1,ctx2);
}
#endif // CnnMixer_h__
