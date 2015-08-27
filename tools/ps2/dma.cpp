#include <stdio.h>
#include "ps2.h"
#include "ee_regs.h"
#include "dma.h"

struct DmaChannel {
	uint64 chcr; uint64 pad0;
	uint64 madr; uint64 pad1;
	uint64 qwc; uint64 pad2;
	uint64 tadr; uint64 pad3;
	uint64 asr0; uint64 pad4;
	uint64 asr1; uint64 pad5;
	uint64 pad6[4];
	uint64 sadr;
};

static struct DmaChannel *dmaChannels[] = {
	(struct DmaChannel *) D0_CHCR,
	(struct DmaChannel *) D1_CHCR,
	(struct DmaChannel *) D2_CHCR,
	(struct DmaChannel *) D3_CHCR,
	(struct DmaChannel *) D4_CHCR,
	(struct DmaChannel *) D5_CHCR,
	(struct DmaChannel *) D6_CHCR,
	(struct DmaChannel *) D7_CHCR,
	(struct DmaChannel *) D8_CHCR,
	(struct DmaChannel *) D9_CHCR
};

void
dmaReset(int enable)
{
	/* don't clear the SIF channels */
	int doclear[] = { 1, 1, 1, 1, 1, 0, 0, 0, 1, 1 };
	int i;

	printf("%x %x %x %x %x %x %x\n",
		&dmaChannels[0]->chcr,
		&dmaChannels[0]->madr,
		&dmaChannels[0]->qwc,
		&dmaChannels[0]->tadr,
		&dmaChannels[0]->asr0,
		&dmaChannels[0]->asr1,
		&dmaChannels[0]->sadr);

	SET_REG64(D_CTRL, 0);
	for(i = 0; i < 10; i++)
		if(doclear[i]){
			dmaChannels[i]->chcr = 0;
			dmaChannels[i]->madr = 0;
			dmaChannels[i]->qwc = 0;
			dmaChannels[i]->tadr = 0;
			dmaChannels[i]->asr0 = 0;
			dmaChannels[i]->asr1 = 0;
			dmaChannels[i]->sadr = 0;
		}

	if(enable)
		SET_REG64(D_CTRL, MAKE_D_CTRL(1,0,0,0,0,0));
}
