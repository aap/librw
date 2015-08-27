#ifndef GIF_H
#define GIF_H

#include "ps2.h"
#include "ee_regs.h"
#include "dma.h"

#define GIF_DECLARE_PACKET(NAME,N) \
	uint64 __attribute__((aligned (16))) NAME[N*2]; \
	int NAME##_cur; \
	int NAME##_size;

#define GIF_BEGIN_PACKET(NAME) \
	NAME##_cur = 0; \
	NAME##_size = 0

#define MAKE_GIF_TAG(NLOOP,EOP,PRE,PRIM,FLG,NREG)\
	(BIT64(NLOOP, 0) | \
	 BIT64(EOP,   15) | \
	 BIT64(PRE,   46) | \
	 BIT64(PRIM,  47) | \
	 BIT64(FLG,   58) | \
	 BIT64(NREG,  60));

#define GIF_TAG(NAME,NLOOP,EOP,PRE,PRIM,FLG,NREG,REGS) \
	NAME##_size += NLOOP + 1; \
	NAME[NAME##_cur++] = \
	(BIT64(NLOOP, 0) | \
	 BIT64(EOP,   15) | \
	 BIT64(PRE,   46) | \
	 BIT64(PRIM,  47) | \
	 BIT64(FLG,   58) | \
	 BIT64(NREG,  60)); \
	NAME[NAME##_cur++] = REGS

#define GIF_DATA_AD(NAME,REG,DATA) \
	NAME[NAME##_cur++] = DATA; \
	NAME[NAME##_cur++] = REG

#define GIF_DATA_RGBAQ(NAME,R,G,B,A) \
	NAME[NAME##_cur++] = \
	(BIT64(R, 0) | \
	 BIT64(G, 32)); \
	NAME[NAME##_cur++] = \
	(BIT64(B, 0) | \
	 BIT64(A, 32))

#define GIF_DATA_XYZ2(NAME,X,Y,Z,ADC) \
	NAME[NAME##_cur++] = \
	(BIT64(X, 0) | \
	 BIT64(Y, 32)); \
	NAME[NAME##_cur++] = \
	(BIT64(Z,   0) | \
	 BIT64(ADC, 47))

#define GIF_SEND_PACKET(NAME) \
	FlushCache(0); \
	SET_REG32(D2_QWC,  MAKE_DN_QWC(NAME##_cur/2)); \
	SET_REG32(D2_MADR, MAKE_DN_MADR(&(NAME), 0)); \
	SET_REG32(D2_CHCR, MAKE_DN_CHCR(1, 0, 0, 0, 0, 1)); \
	DMA_WAIT(D2_CHCR)

#endif
