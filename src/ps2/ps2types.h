#ifdef RW_PS2

#include <eetypes.h>

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned int uintptr_t;
typedef u_long128 uint128_t;

typedef union QWord QWord;
union QWord {
	u_long128 q_u128;
	u_long q_u64[2];
	u_int q_u32[4];
	float q_f[4];
};

#define MAKE128(RES,MSB,LSB) \
	__asm__ ( "pcpyld %0, %1, %2" : "=r" (RES) : "r" ((uint64)MSB), "r" ((uint64)LSB))
#define UINT64(HIGH,LOW) (((uint64)(uint32)HIGH)<<32 | ((uint64)(uint32)LOW))
#define MAKEQ(RES,W3,W2,W1,W0) MAKE128(RES,UINT64(W3,W2),UINT64(W1,W0))

enum {
	IntFlg	= 0x80000000,

	DMAcnt	= 0x10000000,
	DMAref	= 0x30000000,
	DMAcall	= 0x50000000,
	DMAret	= 0x60000000,
	DMAend	= 0x70000000,

	VIFnop	= 0,
	VIFoffset	= 0x02000000,
	VIFbase		= 0x03000000,
	VIFitop		= 0x04000000,
	VIFstmod	= 0x05000000,
	VIFmskpath3	= 0x06008000,
	VIFunmskpath3	= 0x06000000,
	VIFmark		= 0x07000000,
	VIFflushe	= 0x10000000,
	VIFflush	= 0x11000000,
	VIFflusha	= 0x13000000,
	VIFmscal	= 0x14000000,
	VIFmscalf	= 0x15000000,
	VIFmscnt	= 0x17000000,
	VIFstmask	= 0x20000000,
	VIFstrow	= 0x30000000,
	VIFstcol	= 0x31000000,
	VIFdirect	= 0x50000000,

	V4_32 = 0x6C
};

#define UNPACK(type, nq, offset) ((type)<<24 | (nq)<<16 | (offset))
#define STCYCL(WL,CL) (0x01000000 | (WL)<<8 | (CL))

#endif
