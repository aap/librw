#ifndef PS2_H
#define PS2_H

typedef char	int8;
typedef short	int16;
typedef int	int32;
typedef long	int64;
typedef int	int128 __attribute__ ((mode (TI)));

typedef unsigned char	uint8;
typedef unsigned short	uint16;
typedef unsigned int	uint32;
typedef unsigned long	uint64;
typedef unsigned int	uint128 __attribute__ ((mode (TI)));

#define BIT32(a,b) \
	((uint32) (a) << (b))

#define BIT64(a,b) \
	((uint64) (a) << (b))

#define BIT128(a,b) \
	((uint128) (a) << (b))
/*
#ifndef NULL
	#define NULL ((void *)0)
#endif

#define FALSE 0
#define TRUE (!FALSE)
*/

#define FTOI4(x) ((int16)(x)*16.0f)

#endif
