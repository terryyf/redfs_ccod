#ifndef ConAddress_h__
#define ConAddress_h__

enum ConnType
{
	CT_TCP,
    CT_UDP,
	CT_NO,
};

class ConAddress
{
public:
	ConAddress():m_ct(CT_NO),m_con_num(0){}

	ConAddress(ConnType ct,const GMAddrEx &laddr,const GMAddrEx &raddr,U32 con_num)
	{
		m_ct = ct;
		m_laddr = laddr;
		m_raddr = raddr;
		m_con_num = con_num;
	}

	bool operator<(const ConAddress& v)const
	{
		if (m_ct != v.m_ct)
			return m_ct < v.m_ct;

		if (m_laddr != v.m_laddr)
			return m_laddr < v.m_laddr;

		if (m_raddr != v.m_raddr)
			return m_raddr < v.m_raddr;

		if (m_con_num != v.m_con_num)
			return m_con_num < v.m_con_num;
        return false;
	}

	ConnType m_ct;
	GMAddrEx m_laddr;
	GMAddrEx m_raddr;
	U32 m_con_num;
};

#endif // ConAddress_h__
