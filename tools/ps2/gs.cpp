#include <kernel.h>
#include <stdio.h>
#include "ps2.h"
#include "ee_regs.h"
#include "gs.h"
#include "gif.h"

#include "math.h"
#include "mesh.h"

GIF_DECLARE_PACKET(gifDmaBuf, 256)

struct GsState *gsCurState;

void
dumpdma(void)
{
	int i;
	for (i = 0; i < gifDmaBuf_cur; i += 2)
		printf("%016lX %016lX\n", gifDmaBuf[i+1], gifDmaBuf[i]);
}

void
gsDumpState(void)
{
	struct GsState *g = gsCurState;
	printf("mode: %d\tinterlaced: %d\tframe: %d\n"
	       "width,height: %d, %d\n"
	       "startx, starty: %d %d\n"
	       "dw, dh: %d %d\n"
	       "magh, magv: %d %d\n"
	       "xoff, yoff: %d %d\n"
	       "xmax, ymax: %d %d\n",
	       g->mode, g->interlaced, g->field,
	       g->width, g->height,
	       g->startx, g->starty,
	       g->dw, g->dh,
	       g->magh, g->magv,
	       g->xoff, g->yoff,
	       g->xmax, g->ymax);
}

void
gsInitState(uint16 interlaced, uint16 mode, uint16 field)
{
	struct GsState *g = gsCurState;
	int i;

	g->mode = mode;
	g->interlaced = interlaced;
	g->field = field;
	if(mode == PAL){
		g->width = 640;
		g->height = 256;
		g->startx = 680;
		g->starty = 37;
		g->dw = 2560;
		g->dh = 256;
	}else{
		g->width = 640;
		g->height = 224;
		g->startx = 652;
		g->starty = 26;
		g->dw = 2560;
		g->dh = 224;
	}

	if(interlaced == INTERLACED){
		if(field == FIELD)
			g->height *= 2;
		g->dh *= 2;
		g->starty = (g->starty-1)*2;
	}
	g->magh = 3;
	g->magv = 0;

	g->xoff = 2048<<4;
	g->yoff = 2048<<4;
	g->xmax = g->width - 1;
	g->ymax = g->height - 1;
	g->psm = PSMCT32;
	g->depth = 32;
	g->bpp = 4;
//	g->psm = PSMCT24;
//	g->depth = 24;
//	g->bpp = 4;
	g->zpsm = PSMCZ32;
	g->zdepth = 32;
	g->zbpp = 4;
	g->zmax = 0x0FFFFFFFF;
	g->currentMemPtr = 0;
	g->activeFb = 0;
	g->visibleFb = 0;

	g->ztest = 1;
	g->zfunc = ZTST_GREATER;
	g->blend.enable = 0;
	g->shadingmethod = IIP_GOURAUD;

	for(i = 0; i < 2; i++){
		g->fbAddr[i] = g->currentMemPtr;
		g->currentMemPtr += g->width*g->height*g->bpp;
	}
	g->zbAddr = g->currentMemPtr;
	g->currentMemPtr += g->width*g->height*g->zbpp;
}

void
gsInit(void)
{
	struct GsState *g = gsCurState;
	int addr0, addr1;

	g->visibleFb = 0;
	g->activeFb = 1;
	printf("%d %d\n", g->fbAddr[0], g->fbAddr[1]);
	addr0 = g->fbAddr[g->visibleFb]/4/2048;
	addr1 = g->fbAddr[g->activeFb]/4/2048;

	GS_RESET();
	__asm__("sync.p; nop");
	SET_REG64(GS_CSR, 0);
	GsPutIMR(0xff00);
	SetGsCrt(g->interlaced, g->mode, g->field);

	SET_REG64(GS_PMODE, MAKE_GS_PMODE(0, 1, 0, 1, 0, 0xFF));
	SET_REG64(GS_DISPFB2, MAKE_GS_DISPFB(addr0, g->width/64, g->psm, 0, 0));
	SET_REG64(GS_DISPFB1, MAKE_GS_DISPFB(addr0, g->width/64, g->psm, 0, 0));
	SET_REG64(GS_DISPLAY2, MAKE_GS_DISPLAY(g->startx, g->starty,
	                                       g->magh, g->magv,
	                                       g->dw-1, g->dh-1));
	SET_REG64(GS_DISPLAY1, MAKE_GS_DISPLAY(g->startx, g->starty,
	                                       g->magh, g->magv,
	                                       g->dw-1, g->dh-1));
	SET_REG64(GS_BGCOLOR, MAKE_GS_BGCOLOR(100, 100, 100));

	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, 7, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf, GS_PRMODECONT, MAKE_GS_PRMODECONT(1));
	GIF_DATA_AD(gifDmaBuf, GS_FRAME_1,
	            MAKE_GS_FRAME(addr1, g->width/64, g->psm, 0));
	GIF_DATA_AD(gifDmaBuf, GS_ZBUF_1,
	            MAKE_GS_ZBUF(g->zbAddr/4/2048, g->zpsm, 0));
	GIF_DATA_AD(gifDmaBuf, GS_XYOFFSET_1,
	            MAKE_GS_XYOFFSET(g->xoff, g->yoff));
	GIF_DATA_AD(gifDmaBuf, GS_SCISSOR_1,
	            MAKE_GS_SCISSOR(0, g->xmax, 0, g->ymax));
	GIF_DATA_AD(gifDmaBuf, GS_TEST_1,
	            MAKE_GS_TEST(0, 0, 0, 0, 0, 0, g->ztest, g->zfunc));
	GIF_DATA_AD(gifDmaBuf, GS_COLCLAMP, MAKE_GS_COLCLAMP(1));
	GIF_SEND_PACKET(gifDmaBuf);
}

void
gsUpdateContext(void)
{
	GsState *gs = gsCurState;
	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, 2, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf, GS_TEST_1,
	            MAKE_GS_TEST(0, 0, 0, 0, 0, 0, gs->ztest, gs->zfunc));
	GIF_DATA_AD(gifDmaBuf, GS_ALPHA_1,
	            MAKE_GS_ALPHA(gs->blend.a, gs->blend.b, gs->blend.c,
	                          gs->blend.d, gs->blend.fix));
	GIF_SEND_PACKET(gifDmaBuf);
}

void
gsPollVsynch(void)
{
	volatile uint64 *csr = (uint64 *) GS_CSR;
	*csr &= 8;
	while(!(*csr & 8));
}

void
gsFlip(int synch)
{
	struct GsState *g = gsCurState;
	g->activeFb = (g->activeFb+1) & 1;
	g->visibleFb = (g->visibleFb+1) & 1;
	if(synch)
		gsPollVsynch();
	gsSelectActiveFb(g->activeFb);
	gsSelectVisibleFb(g->visibleFb);
}

void
gsSelectActiveFb(int n)
{
	struct GsState *g = gsCurState;
	int addr;

	addr = g->fbAddr[n]/4/2048;
	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, 1, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf, GS_FRAME_1,
	            MAKE_GS_FRAME(addr, g->width/64, g->psm, 0));
	GIF_SEND_PACKET(gifDmaBuf);
}

void
gsSelectVisibleFb(int n)
{
	struct GsState *g = gsCurState;
	int addr;

	addr = g->fbAddr[n]/4/2048;
	SET_REG64(GS_DISPFB2, MAKE_GS_DISPFB(addr, g->width/64, g->psm, 0, 0));
}

void
gsClear(void)
{
	struct GsState *gs = gsCurState;
	uint16 x, y;
	int r = gs->clearcol & 0xFF;
	int g = (gs->clearcol >> 8) & 0xFF;
	int b = (gs->clearcol >> 16) & 0xFF;
	int a = (gs->clearcol >> 24) & 0xFF;

	x = (gs->width << 4) + gs->xoff;
	y = (gs->height << 4) + gs->yoff;

	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, 6, 1, 0, 0, 0, 1, 0x0e);
	/* clear z-buffer too */
	GIF_DATA_AD(gifDmaBuf, GS_TEST_1,
	            MAKE_GS_TEST(0, 0, 0, 0, 0, 0, 1, ZTST_ALWAYS));
	GIF_DATA_AD(gifDmaBuf, GS_PRIM,
	            MAKE_GS_PRIM(PRIM_SPRITE, 0, 0, 0, 0, 0, 0, 0, 0));
	GIF_DATA_AD(gifDmaBuf, GS_RGBAQ,
	            MAKE_GS_RGBAQ(r, g, b, a, 0));
	GIF_DATA_AD(gifDmaBuf, GS_XYZ2, MAKE_GS_XYZ(0, 0, 0));
	GIF_DATA_AD(gifDmaBuf, GS_XYZ2, MAKE_GS_XYZ(x, y, 0));
	/* re-enable z-test */
	GIF_DATA_AD(gifDmaBuf, GS_TEST_1,
	            MAKE_GS_TEST(0, 0, 0, 0, 0, 0, gs->ztest, gs->zfunc));
	GIF_SEND_PACKET(gifDmaBuf);
}

void
gsNormalizedToScreen(float *v1, uint32 *v2)
{
	struct GsState *g = gsCurState;
	uint64 d;

	d = g->zmax & 0xFFFFFFFF;
	v2[0] = (1.0f+v1[0])*g->width/2;
	v2[1] = (1.0f-v1[1])*g->height/2;
	v2[2] = (1.0f-v1[2])*d/2;
}

void
drawrect(uint16 x1, uint16 y1, uint16 x2, uint16 y2, uint32 col)
{
	struct GsState *gs = gsCurState;
	x1 = (x1<<4) + gs->xoff;
	y1 = (y1<<4) + gs->yoff;
	x2 = (x2<<4) + gs->xoff;
	y2 = (y2<<4) + gs->yoff;
	int r = col & 0xFF;
	int g = (col >> 8) & 0xFF;
	int b = (col >> 16) & 0xFF;
	int a = (col >> 24) & 0xFF;

	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, 4, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf, GS_PRIM,
	            MAKE_GS_PRIM(PRIM_SPRITE, 0, 0, 0,
	                         gs->blend.enable, 0, 0, 0, 0));
	GIF_DATA_AD(gifDmaBuf, GS_RGBAQ,
	            MAKE_GS_RGBAQ(r, g, b, a, 0));
	GIF_DATA_AD(gifDmaBuf, GS_XYZ2, MAKE_GS_XYZ(x1, y1, 0));
	GIF_DATA_AD(gifDmaBuf, GS_XYZ2, MAKE_GS_XYZ(x2, y2, 0));
	GIF_SEND_PACKET(gifDmaBuf);
}

/*
void
drawdata(float *data, int vertexCount)
{
	struct GsState *g = gsCurState;
	int i;
	float *verts, *colors;
	Vector4f vf;
	uint32   vi[3];
	uint8    c[4];

	verts = &data[0];
	colors = &data[vertexCount*3];

	GIF_BEGIN_PACKET(gifDmaBuf);
	GIF_TAG(gifDmaBuf, vertexCount, 1, 1,
	        MAKE_GS_PRIM(PRIM_TRI,IIP_GOURAUD,0,0,0,0,0,0,0), 0, 2, 0x51);

	for (i = 0; i < vertexCount; i++) {
		vec4fCopy(vf, &verts[i*3]);
		vf[3] = 1.0f;

		matMultVec(mathModelViewMat, vf);
		matMultVec(mathProjectionMat, vf);
		vf[0] /= vf[3];
		vf[1] /= vf[3];
		vf[2] /= vf[3];

		gsNormalizedToScreen(vf, vi);

		vi[0] = ((vi[0]*16) + g->xoff) & 0xFFFF;
		vi[1] = ((vi[1]*16) + g->yoff) & 0xFFFF;

		c[0] = colors[i*4+0];
		c[1] = colors[i*4+1];
		c[2] = colors[i*4+2];
		c[3] = colors[i*4+3];

		GIF_DATA_RGBAQ(gifDmaBuf, c[0], c[1], c[2], c[3]);
		GIF_DATA_XYZ2(gifDmaBuf, vi[0], vi[1], vi[2], 0);
	}
	GIF_SEND_PACKET(gifDmaBuf);
}
*/
