#ifndef SACnnID_h__
#define SACnnID_h__

/*#include <aio_basic/CompletionPortPub.h>*/

#ifdef X64
typedef U64 UID;
#else
typedef U32 UID;
#endif

enum SA_ID_TYPE
{
	SIT_TCP,
    SIT_UDP,
};

class SACnnID
{
public:
	SACnnID():m_tp(SIT_TCP),m_id(0){}

	SACnnID(SA_ID_TYPE tp,UID id)
	{
		m_tp = tp;
		m_id = id;
	}

	bool operator<(const SACnnID& id)const
	{
		if (m_tp!=id.m_tp)
			return m_tp < id.m_tp;

		return m_id < id.m_id;
	}

	bool operator==(const SACnnID& id)const
	{
		return m_tp == id.m_tp && m_id == id.m_id;
	}


	char* toString(char buf[],U32 buflen)const
	{
		if (SIT_UDP == m_tp) {
#ifdef X64
            strcat(buf,GMSZ("UDP:%lld",m_id));
#else
            strcat(buf,GMSZ("UDP:%d",m_id));
#endif
        }
		else
		{
#ifdef X64
			strcat(buf,GMSZ("TCP:%lld",m_id));
#else
			strcat(buf,GMSZ("TCP:%d",m_id));
#endif
			//GMStrCat(buf,buflen,GMSZ("RUDP:%s",GMAddress(m_remoteRudpAddr.m_ip,m_remoteRudpAddr.m_port).toStr()));
		}

		return buf;
	}

	SA_ID_TYPE m_tp;
	
	UID m_id;
	
	//GMAddr m_remoteRudpAddr;
};

#endif // SACnnID_h__
