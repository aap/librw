#ifndef GS_H
#define GS_H
#include "ps2.h"
#include "ee_regs.h"

typedef struct GsState GsState;
struct GsState {
	int32 mode, interlaced, field;
	int32 width, height;
	int32 startx, starty;
	int32 dw, dh;
	int32 magh, magv;
	int32 xoff, yoff;
	int32 xmax, ymax;
	int32 psm;
	int32 depth;
	int32 bpp;
	int32 zpsm;
	int32 zdepth;
	int32 zbpp;
	int32 zmax;
	int32 currentMemPtr;
	uint32 fbAddr[2];
	uint32 zbAddr;

	int32 visibleFb;
	int32 activeFb;

	uint32 clearcol;

	int32 ztest;
	int32 zfunc;
	struct {
		int32 enable;
		int32 a, b, c, d;
		int32 fix;
	} blend;
	int32 shadingmethod;
};

extern GsState *gsCurState;

void gsDumpState(void);
void gsInitState(uint16 interlaced,
                 uint16 mode, uint16 field);
void gsInit(void);
void gsUpdateContext(void);
void gsSelectVisibleFb(int n);
void gsSelectActiveFb(int n);
void gsFlip(void);
void gsClear(void);
void gsNormalizedToScreen(float *v1, uint32 *v2);

void drawrect(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint32 col);
//void drawdata(float *data, int vertexCount);


#define NTSC		2
#define PAL		3
#define DTV480P		0x50	//(use with NONINTERLACED, FRAME)

#define NONINTERLACED	0
#define INTERLACED	1

#define FIELD		0
#define FRAME		1

#define PSMCT32		0
#define PSMCT24		1
#define PSMCT16		2
#define PSMCT16S	10
#define PS_GPU24	18

#define PSMCZ32		0
#define PSMCZ24		1
#define PSMCZ16		2
#define PSMCZ16S	10

#define ZTST_NEVER	0
#define ZTST_ALWAYS	1
#define ZTST_GREATER	2
#define ZTST_GEQUAL	3

#define PRIM_POINT	0
#define PRIM_LINE	1
#define PRIM_LINE_STRIP	2
#define PRIM_TRI	3
#define PRIM_TRI_STRIP	4
#define PRIM_TRI_FAN	5
#define PRIM_SPRITE	6
#define PRIM_NO_SPEC	7
#define IIP_FLAT	0
#define IIP_GOURAUD	1

/*
 * GS General Purpose Registers
 */

#define GS_PRIM		0x00
#define GS_RGBAQ	0x01
#define GS_ST		0x02
#define GS_UV		0x03
#define GS_XYZF2	0x04
#define GS_XYZ2		0x05
#define GS_TEX0_1	0x06
#define GS_TEX0_2	0x07
#define GS_CLAMP_1	0x08
#define GS_CLAMP_2	0x09
#define GS_FOG		0x0a
#define GS_XYZF3	0x0c
#define GS_XYZ3		0x0d
#define GS_TEX1_1	0x14
#define GS_TEX1_2	0x15
#define GS_TEX2_1	0x16
#define GS_TEX2_2	0x17
#define GS_XYOFFSET_1	0x18
#define GS_XYOFFSET_2	0x19
#define GS_PRMODECONT	0x1a
#define GS_PRMODE	0x1b
#define GS_TEXCLUT	0x1c
#define GS_SCANMSK	0x22
#define GS_MIPTBP1_1	0x34
#define GS_MIPTBP1_2	0x35
#define GS_MIPTBP2_1	0x36
#define GS_MIPTBP2_2	0x37
#define GS_TEXA		0x3b
#define GS_FOGCOL	0x3d
#define GS_TEXFLUSH	0x3f
#define GS_SCISSOR_1	0x40
#define GS_SCISSOR_2	0x41
#define GS_ALPHA_1	0x42
#define GS_ALPHA_2	0x43
#define GS_DIMX		0x44
#define GS_DTHE		0x45
#define GS_COLCLAMP	0x46
#define GS_TEST_1	0x47
#define GS_TEST_2	0x48
#define GS_PABE		0x49
#define GS_FBA_1	0x4a
#define GS_FBA_2	0x4b
#define GS_FRAME_1	0x4c
#define GS_FRAME_2	0x4d
#define GS_ZBUF_1	0x4e
#define GS_ZBUF_2	0x4f
#define GS_BITBLTBUF	0x50
#define GS_TRXPOS	0x51
#define GS_TRXREG	0x52
#define GS_TRXDIR	0x53
#define GS_HWREG	0x54
#define GS_SIGNAL	0x60
#define GS_FINISH	0x61
#define GS_LABEL	0x62

#define MAKE_GS_PRIM(PRIM,IIP,TME,FGE,ABE,AA1,FST,CTXT,FIX) \
	(BIT64(PRIM, 0) | \
	 BIT64(IIP,  3) | \
	 BIT64(TME,  4) | \
	 BIT64(FGE,  5) | \
	 BIT64(ABE,  6) | \
	 BIT64(AA1,  7) | \
	 BIT64(FST,  8) | \
	 BIT64(CTXT, 9) | \
	 BIT64(FIX,  10))

#define MAKE_GS_RGBAQ(R,G,B,A,Q) \
	(BIT64(R, 0) | \
	 BIT64(G, 8) | \
	 BIT64(B, 16) | \
	 BIT64(A, 24) | \
	 BIT64(Q, 32))

#define MAKE_GS_ST(S,T) \
	(BIT64(S, 0) | \
	 BIT64(T, 32))

#define MAKE_GS_UV(U,V) \
	(BIT64(U, 0) | \
	 BIT64(V, 16))

#define MAKE_GS_XYZF(X,Y,Z,F) \
	(BIT64(X, 0) | \
	 BIT64(Y, 16) | \
	 BIT64(Z, 32) | \
	 BIT64(F, 56))

#define MAKE_GS_XYZ(X,Y,Z) \
	(BIT64(X, 0) | \
	 BIT64(Y, 16) | \
	 BIT64(Z, 32))

#define MAKE_GS_TEX0(TBP0,TBW,PSM,TW,TH,TCC,TFX,CBP,CPSM,CSM,CSA,CLD) \
	(BIT64(TBP0, 0) | \
	 BIT64(TBW,  14) | \
	 BIT64(PSM,  20) | \
	 BIT64(TW,   26) | \
	 BIT64(TH,   30) | \
	 BIT64(TCC,  34) | \
	 BIT64(TFX,  35) | \
	 BIT64(CBP,  37) | \
	 BIT64(CPSM, 51) | \
	 BIT64(CSM,  55) | \
	 BIT64(CSA,  56) | \
	 BIT64(CLD,  61))

#define MAKE_GS_CLAMP(WMS,WMT,MINU,MAXU,MINV,MAXV) \
	(BIT64(WMS,  0) | \
	 BIT64(WMT,  2) | \
	 BIT64(MINU, 4) | \
	 BIT64(MAXU, 14) | \
	 BIT64(MINV, 24) | \
	 BIT64(MAXV, 34))

#define MAKE_GS_FOG(F) \
	(BIT64(F, 56))

#define MAKE_GS_TEX1(LCM,MXL,MMAG,MMIN,MTBA,L,K) \
	(BIT64(LCM,  0) | \
	 BIT64(MXL,  2) | \
	 BIT64(MMAG, 5) | \
	 BIT64(MMIN, 6) | \
	 BIT64(MTBA, 9) | \
	 BIT64(L,    19) | \
	 BIT64(K,    32))

#define MAKE_GS_TEX2(PSM,CBP,CPSM,CSM,CSA,CLD) \
	(BIT64(PSM,  20) | \
	 BIT64(CBP,  37) | \
	 BIT64(CPSM, 51) | \
	 BIT64(CSM,  55) | \
	 BIT64(CSA,  56) | \
	 BIT64(CLD,  61))

#define MAKE_GS_XYOFFSET(OFX,OFY) \
	(BIT64(OFX, 0) | \
	 BIT64(OFY, 32))

#define MAKE_GS_PRMODECONT(AC) \
	(BIT64(AC, 0))

#define MAKE_GS_PRMODE(IIP,TME,FGE,ABE,AA1,FST,CTXT,FIX) \
	(BIT64(IIP,  3) | \
	 BIT64(TME,  4) | \
	 BIT64(FGE,  5) | \
	 BIT64(ABE,  6) | \
	 BIT64(AA1,  7) | \
	 BIT64(FST,  8) | \
	 BIT64(CTXT, 9) | \
	 BIT64(FIX,  10))

#define MAKE_GS_TEXCLUT(CBW,COU,COV) \
	(BIT64(CBW, 0) | \
	 BIT64(COU, 6) | \
	 BIT64(COV, 12))

#define MAKE_GS_SCANMSK(MSK) \
	(BIT64(MSK, 0))

#define MAKE_GS_MIPTBP1(TBP1,TBW1,TBP2,TBW2,TBP3,TBW3) \
	(BIT64(TBP1, 0) | \
	 BIT64(TBW1, 14) | \
	 BIT64(TBP2, 20) | \
	 BIT64(TBW2, 34) | \
	 BIT64(TBP3, 40) | \
	 BIT64(TBW3, 54))

#define MAKE_GS_MIPTBP2(TBP4,TBW4,TBP5,TBW5,TBP6,TBW6) \
	(BIT64(TBP4, 0) | \
	 BIT64(TBW4, 14) | \
	 BIT64(TBP5, 20) | \
	 BIT64(TBW5, 34) | \
	 BIT64(TBP6, 40) | \
	 BIT64(TBW6, 54))

#define MAKE_GS_TEXA(TA0,AEM,TA1) \
	(BIT64(TA0, 0) | \
	 BIT64(AEM, 15) | \
	 BIT64(TA1, 32))

#define MAKE_GS_FOGCOL(FCR,FCG,FCB) \
	(BIT64(FCR, 0) | \
	 BIT64(FCG, 8) | \
	 BIT64(FCB, 16))

/* GS_TEXFLUSH */

#define MAKE_GS_SCISSOR(SCAX0,SCAX1,SCAY0,SCAY1) \
	(BIT64(SCAX0, 0) | \
	 BIT64(SCAX1, 16) | \
	 BIT64(SCAY0, 32) | \
	 BIT64(SCAY1, 48))

#define MAKE_GS_ALPHA(A,B,C,D,FIX) \
	(BIT64(A,   0) | \
	 BIT64(B,   2) | \
	 BIT64(C,   4) | \
	 BIT64(D,   6) | \
	 BIT64(FIX, 32))

#define MAKE_GS_DIMX(DM00,DM01,DM02,DM03, \
                     DM10,DM11,DM12,DM13, \
                     DM20,DM21,DM22,DM23, \
                     DM30,DM31,DM32,DM33) \
	(BIT64(DM00, 0) | \
	 BIT64(DM01, 4) | \
	 BIT64(DM02, 8) | \
	 BIT64(DM03, 12) | \
	 BIT64(DM10, 16) | \
	 BIT64(DM11, 20) | \
	 BIT64(DM12, 24) | \
	 BIT64(DM13, 28) | \
	 BIT64(DM20, 32) | \
	 BIT64(DM21, 36) | \
	 BIT64(DM22, 40) | \
	 BIT64(DM23, 44) | \
	 BIT64(DM30, 48) | \
	 BIT64(DM31, 52) | \
	 BIT64(DM32, 56) | \
	 BIT64(DM33, 60))

#define MAKE_GS_DITHE(DTHE) \
	(BIT64(DTHE, 0))

#define MAKE_GS_COLCLAMP(CLAMP) \
	(BIT64(CLAMP, 0))

#define MAKE_GS_TEST(ATE,ATST,AREF,AFAIL,DATE,DATM,ZTE,ZTST) \
	(BIT64(ATE,   0) | \
	 BIT64(ATST,  1) | \
	 BIT64(AREF,  4) | \
	 BIT64(AFAIL, 12) | \
	 BIT64(DATE,  14) | \
	 BIT64(DATM,  15) | \
	 BIT64(ZTE,   16) | \
	 BIT64(ZTST,  17))

#define MAKE_GS_PABE(PABE) \
	(BIT64(PABE, 0))

#define MAKE_GS_FBA(FBA) \
	(BIT64(FBA, 0))

#define MAKE_GS_FRAME(FBP,FBW,PSM,FBMSK) \
	(BIT64(FBP,   0) | \
	 BIT64(FBW,   16) | \
	 BIT64(PSM,   24) | \
	 BIT64(FBMSK, 32))

#define MAKE_GS_ZBUF(ZBP,PSM,ZMSK) \
	(BIT64(ZBP,  0) | \
	 BIT64(PSM,  24) | \
	 BIT64(ZMSK, 32))

#define MAKE_GS_BITBLTBUF(SBP,SBW,SPSM,DBP,DBW,DPSM) \
	(BIT64(SBP,  0) | \
	 BIT64(SBW,  16) | \
	 BIT64(SPSM, 24) | \
	 BIT64(DBP,  32) | \
	 BIT64(DBW,  48) | \
	 BIT64(DPSM, 56))

#define MAKE_GS_TRXPOS(SSAX,SSAY,DSAX,DSAY,DIR) \
	(BIT64(SSAX, 0) | \
	 BIT64(SSAY, 16) | \
	 BIT64(DSAX, 32) | \
	 BIT64(DSAY, 48) | \
	 BIT64(DIR,  59))

#define MAKE_GS_TRXREG(RRW,RRH) \
	(BIT64(RRW, 0) | \
	 BIT64(RRH, 32))

#define MAKE_GS_TRXDIR(XDIR) \
	(BIT64(XDIR, 0))

#define MAKE_GS_HWREG(DATA) \
	(BIT64(DATA, 0))

#define MAKE_GS_SIGNAL(ID,IDMSK) \
	(BIT64(ID,    0) | \
	 BIT64(IDMSK, 32))

/* GS_FINISH */

#define MAKE_GS_LABEL(ID,IDMSK) \
	(BIT64(ID,    0) | \
	 BIT64(IDMSK, 32))


/*
 * No documentation about these:
 * GS_SRFSH
 * GS_SYNCH1
 * GS_SYNCH2
 * GS_SYNCV
 */

#define MAKE_GS_PMODE(EN1,EN2,MMOD,AMOD,SLBG,ALP) \
	(BIT64(EN1,  0) | \
	 BIT64(EN2,  1) | \
	 BIT64(1,    2) | \
	 BIT64(MMOD, 5) | \
	 BIT64(AMOD, 6) | \
	 BIT64(SLBG, 7) | \
	 BIT64(ALP,  8))

#define MAKE_GS_SMODE(INT,FFMD,DPMS) \
	(BIT64(INT,  0) | \
	 BIT64(FFMD, 1) | \
	 BIT64(DPMS, 2))

#define MAKE_GS_DISPFB(FBP,FBW,PSM,DBX,DBY) \
	(BIT64(FBP, 0) | \
	 BIT64(FBW, 9) | \
	 BIT64(PSM, 15) | \
	 BIT64(DBX, 32) | \
	 BIT64(DBY, 43))

#define MAKE_GS_DISPLAY(DX,DY,MAGH,MAGV,DW,DH) \
	(BIT64(DX,   0) | \
	 BIT64(DY,   12) | \
	 BIT64(MAGH, 23) | \
	 BIT64(MAGV, 27) | \
	 BIT64(DW,   32) | \
	 BIT64(DH,   44))

#define MAKE_GS_EXTBUF(EXBP,EXBW,FBIN,WFFMD,EMODE,EMODC,WDX,WDY) \
	(BIT64(EXBP,  0) | \
	 BIT64(EXBW,  14) | \
	 BIT64(FBIN,  20) | \
	 BIT64(WFFMD, 22) | \
	 BIT64(EMODA, 23) | \
	 BIT64(EMODC, 25) | \
	 BIT64(WDX,   32) | \
	 BIT64(WDY,   43))

#define MAKE_GS_EXTDATA(SX,SY,SMPH,SMPV,WW,WH) \
	(BIT64(SX,   0) | \
	 BIT64(SY,   12) | \
	 BIT64(SMPH, 23) | \
	 BIT64(SMPV, 27) | \
	 BIT64(WW,   32) | \
	 BIT64(WH,   44))

#define MAKE_GS_EXTWRITE(WRITE) \
	(BIT64(WRITE, 0))

#define MAKE_GS_BGCOLOR(R,G,B) \
	(BIT64(R, 0) | \
	 BIT64(G, 8) | \
	 BIT64(B, 16))

#define MAKE_GS_CSR(SIGNAL,FINISH,HSINT,VSINT,EDWINT,FLUSH,RESET,NFIELD,\
                    FIELD,FIFO,REV,ID) \
	(BIT64(SIGNAL, 0) | \
	 BIT64(FINISH, 1) | \
	 BIT64(HSINT,  2) | \
	 BIT64(VSINT,  3) | \
	 BIT64(EDWINT, 4) | \
	 BIT64(FLUSH,  8) | \
	 BIT64(RESET,  9) | \
	 BIT64(NFIELD, 12) | \
	 BIT64(FIELD,  13) | \
	 BIT64(FIFO,   14) | \
	 BIT64(REV,    16) | \
	 BIT64(ID,     24))

#define MAKE_GS_IMR(SIGMSK,FINISHMSK,HSMSK,VSMSK,EDWMSK) \
	(BIT64(SIGMSK,    8) | \
	 BIT64(FINISHMSK, 9) | \
	 BIT64(HSMSK,     10) | \
	 BIT64(VSMSK,     11) | \
	 BIT64(EDWMSK,    12))

#define MAKE_GS_BUSDIR(DIR) \
	(BIT64(DIR, 0))

#define MAKE_GS_SIGLBLID(SIGID,LBLID) \
	(BIT64(SIGID, 0) | \
	 BIT64(LBLID, 32))

#define GS_RESET() \
	SET_REG64(GS_CSR, BIT64(1, 9))

#endif
