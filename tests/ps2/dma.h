#ifndef DMA_H
#define DMA_H
#include "ps2.h"
#include "ee_regs.h"

void dmaReset(int enable);

#define DMA_WAIT(REG) \
	while (GET_REG32(REG) & (1<<8))

/*
 * Common Registers
 */

#define MAKE_D_CTRL(DMAE,RELE,MFD,STS,STD,RCYC) \
	(BIT32(DMAE, 0) | \
	 BIT32(RELE, 1) | \
	 BIT32(MFD,  2) | \
	 BIT32(STS,  4) | \
	 BIT32(STD,  6) | \
	 BIT32(RCYC, 8))

#define MAKE_D_STAT(CIS0,CIS1,CIS2,CIS3,CIS4,CIS5,CIS6,CIS7,CIS8,CIS9,\
                    SIS,MEIS,BEIS,\
                    CIM0,CIM1,CIM2,CIM3,CIM4,CIM5,CIM6,CIM7,CIM8,CIM9,\
                    SIM,MEIM) \
	(BIT32(CIS0, 0) | \
	 BIT32(CIS1, 1) | \
	 BIT32(CIS2, 2) | \
	 BIT32(CIS3, 3) | \
	 BIT32(CIS4, 4) | \
	 BIT32(CIS5, 5) | \
	 BIT32(CIS6, 6) | \
	 BIT32(CIS7, 7) | \
	 BIT32(CIS8, 8) | \
	 BIT32(CIS9, 9) | \
	 BIT32(SIS,  13) | \
	 BIT32(MEIS, 14) | \
	 BIT32(BEIS, 15) | \
	 BIT32(CIM0, 16) | \
	 BIT32(CIM1, 17) | \
	 BIT32(CIM2, 18) | \
	 BIT32(CIM3, 19) | \
	 BIT32(CIM4, 20) | \
	 BIT32(CIM5, 21) | \
	 BIT32(CIM6, 21) | \
	 BIT32(CIM7, 22) | \
	 BIT32(CIM8, 23) | \
	 BIT32(CIM9, 24) | \
	 BIT32(SIM,  29) | \
	 BIT32(MEIM, 30))

#define MAKE_D_PCR(CPC0,CPC1,CPC2,CPC3,CPC4,CPC5,CPC6,CPC7,CPC8,CPC9,\
                   CDE0,CDE1,CDE2,CDE3,CDE4,CDE5,CDE6,CDE7,CDE8,CDE9, PCE) \
	(BIT32(CPC0, 0) | \
	 BIT32(CPC1, 1) | \
	 BIT32(CPC2, 2) | \
	 BIT32(CPC3, 3) | \
	 BIT32(CPC4, 4) | \
	 BIT32(CPC5, 5) | \
	 BIT32(CPC6, 6) | \
	 BIT32(CPC7, 7) | \
	 BIT32(CPC8, 8) | \
	 BIT32(CPC9, 9) | \
	 BIT32(CDE0, 16) | \
	 BIT32(CDE1, 17) | \
	 BIT32(CDE2, 18) | \
	 BIT32(CDE3, 19) | \
	 BIT32(CDE4, 20) | \
	 BIT32(CDE5, 21) | \
	 BIT32(CDE6, 22) | \
	 BIT32(CDE7, 23) | \
	 BIT32(CDE8, 24) | \
	 BIT32(CDE9, 25) | \
	 BIT32(PCE,  31))

#define MAKE_D_SQWC(SQWC,TQWC) \
	(BIT32(SQWC, 0) | \
	 BIT32(TQWC, 16))

#define MAKE_D_RBSR(RMSK) \
	(BIT32(RMSK, 4))

#define MAKE_D_RBOR(ADDR) \
	(BIT32(ADDR, 0))

#define MAKE_D_STADR(ADDR) \
	(BIT32(ADDR, 0))

#define MAKE_D_ENABLER(CPND) \
	(BIT32(CPND, 16))

#define MAKE_D_ENABLEW(CPND) \
	(BIT32(CPND, 16))

/*
 * Channel Registers
 */

#define MAKE_DN_CHCR(DIR,MOD,ASP,TTE,TIE,STR) \
	(BIT32(DIR, 0) | \
	 BIT32(MOD, 2) | \
	 BIT32(ASP, 4) | \
	 BIT32(TTE, 6) | \
	 BIT32(TIE, 7) | \
	 BIT32(STR, 8))

#define MAKE_DN_MADR(ADDR,SPR) \
	(BIT32(ADDR, 0) | \
	 BIT32(SPR,  31))

#define MAKE_DN_TADR(ADDR,SPR) \
	(BIT32(ADDR, 0) | \
	 BIT32(SPR,  31))

#define MAKE_DN_ASR(ADDR,SPR) \
	(BIT32(ADDR, 0) | \
	 BIT32(SPR,  31))

#define MAKE_DN_ASR(ADDR,SPR) \
	(BIT32(ADDR, 0) | \
	 BIT32(SPR,  31))

#define MAKE_DN_SADR(ADDR) \
	(BIT32(ADDR, 0))

#define MAKE_DN_QWC(QWC) \
	(BIT32(QWC, 0))

#endif

