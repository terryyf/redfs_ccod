#ifndef RTPPROXY_ECODE_H_2017_07_27
#define RTPPROXY_ECODE_H_2017_07_27

//Rtpproxy 错误码

enum RA_ECODE{
	RA_SUC=0,					//成功
	RA_FAIL=-6000,				//一般的失败
	RA_STOPPED,					//停止工作
	RA_VERSION_INCONFORMITY,	//Rtpproxy与RtpproxyClient版本不匹配
	RA_TIMEOUT,					//在设定时间之内操作未完成
	RA_ALLOC_MEMORY_FAILED,		//分配内存失败
	RA_UNSERIALIZE_FAILED,		//解析内部通信命令错误
	RA_SERIALIZE_FAILED,		//序列化失败，通常因为buffer太小
	RA_WRITE_CONNECTION,		//连接发送数据失败
	RA_CHANNEL_NOT_EXIST,       //Rtpproxy与RtpproxyClient之间的所有连接都被关闭了
	RA_AGENT_NOT_RUNNING,		//RtpproxyClient未运行
	RA_BAD_PARAMETER,			//参数错误
};
inline const char* show_error_code(RA_ECODE er)
{
    switch(er)
    {
        //所有接口都可能返回的错误码
    case RA_SUC:
        return "RA_SUC";
    case RA_FAIL:
        return "RA_FAIL";
	case RA_STOPPED:
		return "SA_STOPPED";
    case RA_VERSION_INCONFORMITY:
        return "SA_VERSION_INCONFORMITY";
    case RA_TIMEOUT:
        return "SA_TIMEOUT";
    case RA_ALLOC_MEMORY_FAILED:
        return "SA_ALLOC_MEMORY_FAILED";
    case RA_UNSERIALIZE_FAILED:
        return "RA_UNSERIALIZE_FAILED";
    case RA_SERIALIZE_FAILED:
        return "SA_SERIALIZE_FAILED";
    case RA_CHANNEL_NOT_EXIST:
        return "SA_CHANNEL_NOT_EXIST";
    case RA_WRITE_CONNECTION:
        return "SA_WRITE_CONNECTION";
    case RA_AGENT_NOT_RUNNING:
        return "SA_AGENT_NOT_RUNNING";
    case RA_BAD_PARAMETER:
        return "SA_BAD_PARAMETER";
    default:
        printf("undefine error code %d\n",er);
        return "undefine error code ";
    }
}

#endif
