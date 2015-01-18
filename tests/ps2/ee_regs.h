#ifndef EE_REGS_H
#define EE_REGS_H
#include "ps2.h"

/*
 * EE Registers
 */

#define SET_REG32(r, x) \
	*((volatile uint32 *) r) = (x)
#define SET_REG64(r, x) \
	*((volatile uint64 *) r) = (x)
#define SET_REG128(r, x) \
	*((volatile uint128 *) r) = (x)

#define GET_REG32(r) \
	*((volatile uint32 *) r)
#define GET_REG64(r) \
	*((volatile uint64 *) r)
#define GET_REG128(r) \
	*((volatile uint128 *) r)

/* Module TIMER */
#define T0_COUNT	0x10000000
#define T0_MODE		0x10000010
#define T0_COMP		0x10000020
#define T0_HOLD		0x10000030
#define T1_COUNT	0x10000800
#define T1_MODE		0x10000810
#define T1_COMP		0x10000820
#define T1_HOLD		0x10000830
#define T2_COUNT	0x10001000
#define T2_MODE		0x10001010
#define T2_COMP		0x10001020
#define T3_COUNT	0x10001800
#define T3_MODE		0x10001810
#define T3_COMP		0x10001820

/* Module IPU */
#define IPU_CMD		0x10002000
#define IPU_CTRL	0x10002010
#define IPU_BP		0x10002020
#define IPU_TOP		0x10002030

/* Module GIF */
#define GIF_CTRL	0x10003000
#define GIF_MODE	0x10003010
#define GIF_STAT	0x10003020
#define GIF_TAG0	0x10003040
#define GIF_TAG1	0x10003050
#define GIF_TAG2	0x10003060
#define GIF_TAG3	0x10003070
#define GIF_CNT		0x10003080
#define GIF_P3CNT	0x10003090
#define GIF_P3TAG	0x100030a0

/* Module VIF0 */
#define VIF0_STAT	0x10003800
#define VIF0_FBRST	0x10003810
#define VIF0_ERR	0x10003820
#define VIF0_MARK	0x10003830
#define VIF0_CYCLE	0x10003840
#define VIF0_MODE	0x10003850
#define VIF0_NUM	0x10003860
#define VIF0_MASK	0x10003870
#define VIF0_CODE	0x10003880
#define VIF0_ITOPS	0x10003890
#define VIF0_ITOP	0x100038d0
#define VIF0_R0		0x10003900
#define VIF0_R1		0x10003910
#define VIF0_R2		0x10003920
#define VIF0_R3		0x10003930
#define VIF0_C0		0x10003940
#define VIF0_C1		0x10003950
#define VIF0_C2		0x10003960
#define VIF0_C3		0x10003970

/* Module VIF1 */
#define VIF1_STAT	0x10003c00
#define VIF1_FBRST	0x10003c10
#define VIF1_ERR	0x10003c20
#define VIF1_MARK	0x10003c30
#define VIF1_CYCLE	0x10003c40
#define VIF1_MODE	0x10003c50
#define VIF1_NUM	0x10003c60
#define VIF1_MASK	0x10003c70
#define VIF1_CODE	0x10003c80
#define VIF1_ITOPS	0x10003c90
#define VIF1_BASE	0x10003ca0
#define VIF1_OFST	0x10003cb0
#define VIF1_TOPS	0x10003cc0
#define VIF1_ITOP	0x10003cd0
#define VIF1_TOP	0x10003ce0
#define VIF1_R0		0x10003d00
#define VIF1_R1		0x10003d10
#define VIF1_R2		0x10003d20
#define VIF1_R3		0x10003d30
#define VIF1_C0		0x10003d40
#define VIF1_C1		0x10003d50
#define VIF1_C2		0x10003d60
#define VIF1_C3		0x10003d70

/* Module FIFO */
#define VIF0_FIFO	 0x1000400
#define VIF1_FIFO	 0x1000500
#define GIF_FIFO	 0x1000600
#define IPU_out_FIFO	 0x1000700
#define IPU_in_FIFO	 0x1000701

/* Module DMAC */
#define D0_CHCR		0x10008000
#define D0_MADR		0x10008010
#define D0_QWC		0x10008020
#define D0_TADR		0x10008030
#define D0_ASR0		0x10008040
#define D0_ASR1		0x10008050

#define D1_CHCR		0x10009000
#define D1_MADR		0x10009010
#define D1_QWC		0x10009020
#define D1_TADR		0x10009030
#define D1_ASR0		0x10009040
#define D1_ASR1		0x10009050

#define D2_CHCR		0x1000a000
#define D2_MADR		0x1000a010
#define D2_QWC		0x1000a020
#define D2_TADR		0x1000a030
#define D2_ASR0		0x1000a040
#define D2_ASR1		0x1000a050

#define D3_CHCR		0x1000b000
#define D3_MADR		0x1000b010
#define D3_QWC		0x1000b020

#define D4_CHCR		0x1000b400
#define D4_MADR		0x1000b410
#define D4_QWC		0x1000b420
#define D4_TADR		0x1000b430

#define D5_CHCR		0x1000c000
#define D5_MADR		0x1000c010
#define D5_QWC		0x1000c020

#define D6_CHCR		0x1000c400
#define D6_MADR		0x1000c410
#define D6_QWC		0x1000c420
#define D6_TADR		0x1000c430

#define D7_CHCR		0x1000c800
#define D7_MADR		0x1000c810
#define D7_QWC		0x1000c820

#define D8_CHCR		0x1000d000
#define D8_MADR		0x1000d010
#define D8_QWC		0x1000d020
#define D8_SADR		0x1000d080

#define D9_CHCR		0x1000d400
#define D9_MADR		0x1000d410
#define D9_QWC		0x1000d420
#define D9_TADR		0x1000d430
#define D9_SADR		0x1000d480

#define D_CTRL		0x1000e000
#define D_STAT		0x1000e010
#define D_PCR		0x1000e020
#define D_SQWC		0x1000e030
#define D_RBSR		0x1000e040
#define D_RBOR		0x1000e050
#define D_STADR		0x1000e060

#define D_ENABLER	0x1000f520
#define D_ENABLEW	0x1000f590

/* Module INTC */
#define I_STAT		0x1000f000
#define I_MASK		0x1000f010

/* Module SIF */
#define SB_SMFLG	0x1000f230

/* Module GS Special */
#define GS_PMODE	0x12000000
#define GS_SMODE1	0x12000010
#define GS_SMODE2	0x12000020
#define GS_SRFSH	0x12000030
#define GS_SYNCH1	0x12000040
#define GS_SYNCH2	0x12000050
#define GS_SYNCV	0x12000060
#define GS_DISPFB1	0x12000070
#define GS_DISPLAY1	0x12000080
#define GS_DISPFB2	0x12000090
#define GS_DISPLAY2	0x120000a0
#define GS_EXTBUF	0x120000b0
#define GS_EXTDATA	0x120000c0
#define GS_EXTWRITE	0x120000d0
#define GS_BGCOLOR	0x120000e0
#define GS_CSR		0x12001000
#define GS_IMR		0x12001010
#define GS_BUSDIR	0x12001040
#define GS_SIGLBLID	0x12001080

#endif
