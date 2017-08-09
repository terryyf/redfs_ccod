#ifndef NumberHelper_h__
#define NumberHelper_h__

class NumberHelper
{
public:
	static void U64ToU32(U64 src,U32& destLow,U32& destHigh)
	{
		destLow = (U32)src;
		destHigh = src >> 32;
	}

	static U64 U32ToU64(U32 srcLow,U32 srcHihg)
	{
		U64 num = srcHihg;
		num = num << 32;
		return num + srcLow;
	}
};

#endif // NumberHelper_h__
