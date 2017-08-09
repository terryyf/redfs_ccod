#include "stdafx.h"
#include "rtpproxy_cmd.h"

U32 volatile g_cmdid = 0;

U32 CmdHead::genCmdID()
{
	return InterlockedIncrement(&g_cmdid);
}
