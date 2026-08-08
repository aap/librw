#ifdef RW_PS2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwps2.h"
#include "rwps2plg.h"

#include "rwps2impl.h"
#include "rwgs.h"

#include <libpc.h>

#define PLUGIN_ID 2



#include <libgraph.h>
#include <libdma.h>

using namespace rw;

#define TIMERCONTROL (SCE_PC0_CPU_CYCLE | (SCE_PC_U0|SCE_PC_S0|SCE_PC_K0|SCE_PC_EXL0) | SCE_PC1_DCACHE_MISS | (SCE_PC_U1) | SCE_PC_CTE)

void
StartTime(void)
{
	DI();
	scePcStart(TIMERCONTROL, 0, 0);
	EI();
}

int GetTime(void) { return scePcGetCounter0(); }
float GetTimeF(void) { return (float)scePcGetCounter0()/294912.0f; }

/******
 *** GS
 ******/

GsCrtState gsCrtState;
GsCtx gsCtx;

uint32 gsAllocPtr;
uint32 gsStart;
const uint32 gsEnd = (4*1024*1024)/4/64;

#define SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh)\
    ((u_long)((dx)) | \
    ((u_long)((dy)) << 12) | \
    ((u_long)(magh) << 23)  | ((u_long)(magv) << 27) | \
    ((u_long)(dw) << 32)    | ((u_long)(dh) << 44))

void
GsInitDispCtx(GsDispCtx *disp, int width, int height, int psm)
{
	int magh, magv;
	int dx, dy;
	int dw, dh;

	dx = gsCrtState.mode == SCE_GS_NTSC ? 636 : 656;
	dy = gsCrtState.mode == SCE_GS_NTSC ? 25 : 36;
	magh = 2560/width - 1;
	magv = 0;
	dw = 2560-1;
	dh = height-1;

	if(gsCrtState.inter == SCE_GS_INTERLACE){
		dy *= 2;
		if(gsCrtState.ff == SCE_GS_FRAME)
			dh = (dh+1)*2-1;
	}

	disp->pmode = SCE_GS_SET_PMODE(0, 1, 1, 1, 1, 0, 0x00);
	disp->bgcolor = 0x000000;
	disp->dispfb1 = 0;
	disp->dispfb2 = SCE_GS_SET_DISPFB(0, width/64, psm, 0, 0);
	disp->display1 = 0;
	disp->display2 = SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh);
}

void
GsInitDrawCtx(GsDrawCtx *draw, int width, int height, int psm, int zpsm)
{
	MAKE128(draw->gifTag, 0xe,
		SCE_GIF_SET_TAG(8, 1, 0, 0, SCE_GIF_PACKED, 1));
	draw->frame1 = SCE_GS_SET_FRAME(0, width/64, psm, 0);
	draw->ad_frame1 = SCE_GS_FRAME_1;
	draw->frame2 = draw->frame1;
	draw->ad_frame2 = SCE_GS_FRAME_2;
	draw->zbuf1 = SCE_GS_SET_ZBUF(0, zpsm, 0);
	draw->ad_zbuf1 = SCE_GS_ZBUF_1;
	draw->zbuf2 = draw->zbuf1;
	draw->ad_zbuf2 = SCE_GS_ZBUF_2;
	draw->xyoffset1 = SCE_GS_SET_XYOFFSET((2048-width/2)<<4, (2048-height/2)<<4);
	draw->ad_xyoffset1 = SCE_GS_XYOFFSET_1;
	draw->xyoffset2 = draw->xyoffset1;
	draw->ad_xyoffset2 = SCE_GS_XYOFFSET_2;
	draw->scissor1 = SCE_GS_SET_SCISSOR(0, width-1, 0, height-1);
	draw->ad_scissor1 = SCE_GS_SCISSOR_1;
	draw->scissor2 = draw->scissor1;
	draw->ad_scissor2 = SCE_GS_SCISSOR_2;
}

int psmsizemap[64] = {
	4,      // PSMCT32
	4,      // PSMCT24
	2,      // PSMCT16
	0, 0, 0, 0, 0, 0, 0,
	2,      // PSMCT16S
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4,      // PSMZ32
	4,      // PSMZ24
	2,      // PSMZ16
	2,      // PSMZ16S
	0, 0, 0, 0, 0
};

void
GsInitCtx(GsCtx *ctx, int width, int height, int psm, int zpsm)
{
	uint32 fbsz, zbsz;
	uint32 fbp, zbp;
	fbsz = (width*height*psmsizemap[psm] + 2047)/2048;
	zbsz = (width*height*psmsizemap[0x30|zpsm] + 2047)/2048;
	gsAllocPtr = (2*fbsz + zbsz)*2048;
	fbp = fbsz;
	zbp = fbsz*2;

	// word addresses
	fbp >>= 2;
	zbp >>= 2;

	// display buffers
	GsInitDispCtx(&ctx->disp[0], width, height, psm);
	ctx->disp[1] = ctx->disp[0];
	ctx->disp[1].dispfb2 |= fbp;

	// draw buffers
	GsInitDrawCtx(&ctx->draw[0], width, height, psm, zpsm);
	ctx->draw[0].zbuf1 |= zbp;
	ctx->draw[0].zbuf2 |= zbp;
	ctx->draw[1] = ctx->draw[0];
	ctx->draw[1].frame1 |= fbp;
	ctx->draw[1].frame2 |= fbp;
}

void
GsSetDisp(GsDispCtx *disp)
{
	*GS_PMODE = disp->pmode;
	*GS_DISPFB1 = disp->dispfb1;
	*GS_DISPLAY1 = disp->display1;
	*GS_DISPFB2 = disp->dispfb2;
	*GS_DISPLAY2 = disp->display2;
	*GS_BGCOLOR = disp->bgcolor;
}

void
GsSetDraw(GsDrawCtx *draw)
{
	using namespace rw::ps2;
	vifPacket[vifPacksz].q_u128 = 0;
	vifPacket[vifPacksz].q_u32[0] = DMAref + 9;
	vifPacket[vifPacksz].q_u32[1] = (uint32)draw;
	// VIF DIRECT
	vifPacket[vifPacksz].q_u32[3] = VIFdirect + 9;
	vifPacksz++;
}

namespace rw {
namespace ps2 {

/*******
 *** DMA
 *******/

// texture upload method
int path2Textures = 0;
int synchTextures = 0;
int gateTextures = 1;

int finishCycle;
sceDmaChan *dmaGif;
sceDmaChan *dmaVif;

uint32 VIFgate[]  __attribute__((aligned (128))) = {
	// we FLUSHA before this in the DMAref tag
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFflush, VIFnop, VIFnop, VIFunmskpath3,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFnop, VIFnop, VIFnop, VIFnop,
	VIFmskpath3, VIFnop, VIFnop, VIFnop,
	////// for synchronous upload
	VIFflush, VIFflusha, VIFflush, VIFflusha,
};

uint32 VIFmask[]  __attribute__((aligned (128))) = {
	DMAend, 0, VIFmskpath3, VIFnop
};
uint32 VIFunmask[]  __attribute__((aligned (128))) = {
	DMAend, 0, VIFunmskpath3, VIFnop
};

void
dumpmatrix(rw::RawMatrix *m)
{
	printf("%f %f %f %f\n", m->right.x, m->up.x, m->at.x, m->pos.x);
	printf("%f %f %f %f\n", m->right.y, m->up.y, m->at.y, m->pos.y);
	printf("%f %f %f %f\n", m->right.z, m->up.z, m->at.z, m->pos.z);
	printf("%f %f %f %f\n", m->rightw, m->upw, m->atw, m->posw);
}

uint32 gifPacksz, gifBufSize;
uint32 vifPacksz, vifBufSize;
char *gifBufBase, *vifBufBase;
QWord *gifPacket, *vifPacket;
QWord *gifPacketEnd, *vifPacketEnd;
QWord *texPtrBuf[4 * 1024];
QWord **texListBuild, **texListDraw;
int numTexBuild, numTexDraw;
QWord *texMarkSlot;

// debug
int marks[1024];
int codes[1024];
int stats[1024];
int numVIFinterrupts;

void
dmaFlip(int i)
{
	i &= 1;
	gifPacksz = 0;
	vifPacksz = 0;
	gifPacket = (QWord*)(gifBufBase + i*gifBufSize);
	gifPacket = (QWord*)((uint32)gifPacket | 0x30000000);
	gifPacketEnd = (QWord*)((char*)gifPacket + gifBufSize);
	vifPacket = (QWord*)(vifBufBase + i*vifBufSize);
	vifPacket = (QWord*)((uint32)vifPacket | 0x30000000);
	vifPacketEnd = (QWord*)((char*)vifPacket + vifBufSize);
	texMarkSlot = nil;
}

void
dmaInit(void)
{
	/* 2*512kb each */
	gifBufSize = 512*1024;
	vifBufSize = 512*1024;
	// TODO: better alignment
	gifBufBase = (char*)malloc(2*gifBufSize);
	vifBufBase = (char*)malloc(2*vifBufSize);
	assert(((uint32)gifBufBase & 0xF) == 0);
	assert(((uint32)vifBufBase & 0xF) == 0);
	dmaFlip(0);
	texListBuild = texPtrBuf;
	texListDraw  = &texPtrBuf[sizeof(texPtrBuf)/2];
}

void
dmaKick(void)
{
	QWord **t;
	uint128 tmp;
	MAKE128(tmp, 0x0, DMAend);

	numVIFinterrupts = 0;
	gifPacket[gifPacksz++].q_u128 = tmp;
	if(!synchTextures && gateTextures)
		vifPacket[vifPacksz++].q_u128 = *(uint128*)VIFunmask;
	else
		vifPacket[vifPacksz++].q_u128 = tmp;

	assert(&vifPacket[vifPacksz] < vifPacketEnd);
	assert(&gifPacket[gifPacksz] < gifPacketEnd);

	numTexDraw = numTexBuild;
	numTexBuild = 0;
	t = texListDraw; texListDraw = texListBuild; texListBuild = t;

	FlushCache(0);
	if(!synchTextures && gateTextures){
		sceDmaSend(dmaVif, VIFmask);
		if(gifPacksz > 1)
			sceDmaSend(dmaGif, (void*)((uint32)gifPacket & ~0xF0000000));
	}

	if(vifPacksz > 1)
		sceDmaSend(dmaVif, (void*)((uint32)vifPacket & ~0xF0000000));
}

int
gsHandler(int id)
{
	if(*GS_CSR & GS_CSR_FINISH_M){
		finishCycle = GetTime();
		*GS_CSR = GS_CSR_FINISH_M;
	}
	ExitHandler();
	return 0;
}

int
dmacVif1Handler(int id)
{
	if(id == DMAC_VIF1){
		/* If tag interrupt, have to restart */
		if(dmaVif->chcr.TAG & 0x8000)
			dmaVif->chcr.STR = 1;
	}
	ExitHandler();
	return 0;
}

int
vif1Handler(int id)
{
	assert(!synchTextures && !gateTextures);
	uint32 mark = *VIF1_MARK;
//	uint32 code = *VIF1_CODE;
//	uint32 stat = *VIF1_STAT;
//	marks[numVIFinterrupts] = mark;
//	codes[numVIFinterrupts] = code;
//	stats[numVIFinterrupts] = stat;
//	numVIFinterrupts++;

	assert(mark < numTexDraw);
	sceDmaSend(dmaGif, texListDraw[mark]);
	/* wait for GIF chain to start before restarting VIF1 */
	while(dmaGif->chcr.STR == 0);

	/* restart VIF1 */
        *VIF1_FBRST = 8;

        ExitHandler();
        return 0;
}


/******
 *** RW
 ******/

uint32 *currentVUCode;

/*
It would be nice to be able to test against RW's microcode.
need to set up VU qwords like this:

        0x3F0   vuSDmat0                   combined matrix; w row = z row
        0x3F1   vuSDmat1                   combined matrix
        0x3F2   vuSDmat2                   combined matrix
        0x3F3   vuSDmat3                   combined matrix
        0x3F4   vuSDnearClip               xyzw: camera near plane                      vu1DataNearClip
        0x3F5   vuSDfarClip                xyzw: camera far plane                       vu1DataFarClip
        0x3F6   vuSDxMaxyMax               ?, ?, -255/(fogEnd-fogStart), fogEnd         xMaxYMax128
        0x3F7   vuSDcamWcamHzScale         raster width, raster height, zScale, 0       vu1DataXYZScale
        0x3F8   vuSDoffXoffYzShift         0, 0, zShift, 0                              vu1DataXYZShift
        0x3F9   vuSDrealOffset             xoff, yoff, 0, 0                             vu1DataOffset3D
        0x3FA   vuSDgifTag                                                              gifTag128
        0x3FB   vuSDcolScale               material color (scaled by 128/255 when textured)
        0x3FC   vuSDsurfProps              surface properties
        0x3FD   vuSDClipvec1               cull: 2047.9374-screenWidth/2, 1/x, zScaleClip, far plane            skyClipVect1
                                           clip: screenWidth/2 + 5, 1/x, zScaleClip, far plane                  skyCClipVect1
        0x3FE   vuSDClipvec2               cull: 2047.9374-screenHeight/2, 1/x, zShiftClip, near plane          skyClipVect2
                                           clip: screenHeight/2 + 5, 1/x, zShiftClip, near plane                skyCClipVect2
        0x3FF   vuSDVUSwitch               code path switch, render system, ?, (backface cull)

xoff = fb->width/2 + fb->offsetX - par->width/2 + 2048
yoff = fb->height/2 + fb->offsetY - par->height/2 + 2048

persp zScaleClip: -2*far*near/(far-near)
persp zShiftClip: (far+near)/(far-near)
paral zScaleClip: 2/(far-near)
paral zShiftClip: -(far+near)/(far-near)
 */

VuConst vuConst;
Raster *cachedTex;
RwStateCache rwStateCache;

float fogFarPlane;	// not sure if we want to keep this
float zScaleScreen;
float zShiftScreen;
float fogScale;
float fogShift;

int32 doClipping = 1;
int32 doModulate2;

#define COLSCALE (128.0f/255.0f)	// TODO: check rounding on VU1 of this

QWord colorNoScale;
QWord colorTexScale;

bool flushTex;
struct GSregs {
	uint64 alpha_1;
	uint64 test_1;
	uint64 prmode;
	uint64 fogcol;
	uint64 tex0_1;
	uint64 tex1_1;
	uint64 clamp_1;
} gsRegs, prevGsRegs;

int rw2gsPrim[] = {
	0,	// PRIMTYPENONE
	1,	// PRIMTYPELINELIST
	2,	// PRIMTYPEPOLYLINE
	3,	// PRIMTYPETRILIST
	4,	// PRIMTYPETRISTRIP
	5,	// PRIMTYPETRIFAN
	0,	// PRIMTYPEPOINTLIST
};

int primSize[] = {
	0,	// PRIMTYPENONE
	2,	// PRIMTYPELINELIST
	2,	// PRIMTYPEPOLYLINE
	3,	// PRIMTYPETRILIST
	3,	// PRIMTYPETRISTRIP
	3,	// PRIMTYPETRIFAN
	1,	// PRIMTYPEPOINTLIST
};

int primRepeat[] = {
	0,	// PRIMTYPENONE
	0,	// PRIMTYPELINELIST
	1,	// PRIMTYPEPOLYLINE
	0,	// PRIMTYPETRILIST
	2,	// PRIMTYPETRISTRIP
	2,	// PRIMTYPETRIFAN	special case
	0,	// PRIMTYPEPOINTLIST
};

void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	uint128 tmp;
	const int nstrips = SCREEN_WIDTH/32;

	// TODO: only clear subraster
	MAKEQ(tmp, VIFdirect + 6 + nstrips*2, VIFnop, 0, DMAcnt + 6 + nstrips*2);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, 0xe, SCE_GIF_SET_TAG(5 + nstrips*2, 1, 1,SCE_GS_PRIM_SPRITE, SCE_GIF_PACKED, 1));
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_TEST_1, SCE_GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1));
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_PRMODE, SCE_GS_SET_PRMODE(0, 0, 0, 0, 0, 0, 0, 0));
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_RGBAQ, SCE_GS_SET_RGBAQ(col->red, col->green, col->blue, 0, 0));
	vifPacket[vifPacksz++].q_u128 = tmp;

	for(int i = 0; i < nstrips; i++){
		int x = 2048 - SCREEN_WIDTH/2;
		int y = 2048 - SCREEN_HEIGHT/2;
		MAKE128(tmp, SCE_GS_XYZ2, SCE_GS_SET_XYZ((x+i*32)<<4, y<<4, 0));
		vifPacket[vifPacksz++].q_u128 = tmp;
		MAKE128(tmp, SCE_GS_XYZ2, SCE_GS_SET_XYZ((x+(i+1)*32)<<4, (y+SCREEN_HEIGHT)<<4, 0));
		vifPacket[vifPacksz++].q_u128 = tmp;
	}

	MAKE128(tmp, SCE_GS_TEST_1, prevGsRegs.test_1);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_PRMODE, prevGsRegs.prmode);
	vifPacket[vifPacksz++].q_u128 = tmp;
}

void
InitGSregs(void)
{
	uint128 tmp;

	int nregs = 3;
	MAKEQ(tmp, VIFdirect + nregs+1, VIFnop, 0, DMAcnt + nregs+1);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, 0xe, SCE_GIF_SET_TAG(nregs, 1, 0,0, SCE_GIF_PACKED, 1));
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_PRMODECONT, 0);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_PRIM, gsRegs.prmode);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, SCE_GS_PRMODE, gsRegs.prmode);
	vifPacket[vifPacksz++].q_u128 = tmp;
}

// cannot blend with color at all
int blendMap[] = {
	-1,	// invalid
	0,	// BLENDZERO
	1,	// BLENDONE
	-1,	// BLENDSRCCOLOR
	-1,	// BLENDINVSRCCOLOR
	2,	// BLENDSRCALPHA
	3,	// BLENDINVSRCALPHA
	4,	// BLENDDESTALPHA
	5,	// BLENDINVDESTALPHA
	-1,	// BLENDDESTCOLOR
	-1,	// BLENDINVDESTCOLOR
	-1	// BLENDSRCALPHASAT
};
int64 blendTable[6][6] = {	// [src][dst]
	0x000000008A,  0x000000004A,  0x0000000089,  0x0000000046,  0x0000000099,  0x0000000056,
	0x000000000A,  0x8000000029,  0x0000000009,            -1,  0x0000000019,            -1,
	0x0000000088,  0x0000000048,            -1,  0x0000000044,            -1,            -1,
	0x0000000002,            -1,  0x0000000001,            -1,            -1,            -1,
	0x0000000098,  0x0000000058,            -1,            -1,            -1,  0x0000000054,
	0x0000000012,            -1,            -1,            -1,  0x0000000011,            -1,
};

void
setAlphaBlend(int32 src, int32 dst)
{
	int64 alpha;
	src = blendMap[src];
	dst = blendMap[dst];
	if(src < 0 || dst < 0)
		return;
	alpha = blendTable[src][dst];
	if(alpha < 0)
		return;
	gsRegs.alpha_1 = alpha;
}

void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)pvalue;
	switch(state){
	case TEXTURERASTER:
		rwStateCache.raster = (Raster*)pvalue;
		if(rwStateCache.raster &&
		   ((rwStateCache.raster->format & 0xF00) == Raster::C1555 ||
		    (rwStateCache.raster->format & 0xF00) == Raster::C8888))
			rwStateCache.textureAlpha = 1;
		else
			rwStateCache.textureAlpha = 0;
		goto setblend;
	case TEXTUREADDRESS:
		rwStateCache.addressU = value;
		rwStateCache.addressV = value;
		break;
	case TEXTUREADDRESSU:
		rwStateCache.addressU = value;
		break;
	case TEXTUREADDRESSV:
		rwStateCache.addressV = value;
		break;
	case TEXTUREFILTER:
		rwStateCache.filterMode = value;
		break;

	case VERTEXALPHA:
		rwStateCache.vertexAlpha = value;
	setblend:
		if(rwStateCache.vertexAlpha || rwStateCache.textureAlpha)
			gsRegs.prmode |= 0x40;
		else
			gsRegs.prmode &= ~0x40;
		break;
	case SRCBLEND:
		rwStateCache.srcblend = value;
		setAlphaBlend(rwStateCache.srcblend, rwStateCache.destblend);
		break;
	case DESTBLEND:
		rwStateCache.destblend = value;
		setAlphaBlend(rwStateCache.srcblend, rwStateCache.destblend);
		break;

	case ZTESTENABLE:
		rwStateCache.ztest = value;
		if(rwStateCache.ztest)
			gsRegs.test_1 = (gsRegs.test_1 & ~0x60000) | 0x40000;
		else
			gsRegs.test_1 = (gsRegs.test_1 & ~0x60000) | 0x20000;
		break;
	case ZWRITEENABLE:
		// TODO
		break;

	case FOGENABLE:
		rwStateCache.fogEnable = value;
		if(rwStateCache.fogEnable)
			gsRegs.prmode |= 0x20;
		else
			gsRegs.prmode &= ~0x20;
		break;
	case FOGCOLOR:
		gsRegs.fogcol = value&0xFFFFFF;
		break;

	case ALPHATESTFUNC:
		break;
	case ALPHATESTREF:
		break;
	}
}

void*
getRenderState(int32 state)
{
}

// in librw
void printTEX0(uint64 tex0);
void printTEX1(uint64 tex1);
void calcTEX1(Raster *raster, uint64 *tex1, int32 filter);

void
printGIFtag(uint128 tag)
{
	QWord q;
	uint64 x;
	q.q_u128 = tag;
	x = q.q_u64[0];

	uint32 nloop = x & 0x7FFF; x >>= 15;
	uint32 eop = x & 0x1; x >>= 1;
	x >>= 30;
	uint32 pre = x & 0x1; x >>= 1;
	uint32 prim = x & 0x7FF; x >>= 11;
	uint32 flg = x & 0x3; x >>= 2;
	uint32 nreg = x & 0xF; x >>= 4;
	printf("%016lX %016lX ", q.q_u64[0], q.q_u64[1]);
	printf("NLOOP:%4X EOP:%X PRE:%X PRIM:%3X FLG:%X NREG:%X\n",
		nloop, eop, pre, prim, flg, nreg);
}

void
printBITBLTBUF(uint64 bitblt)
{
        printf("%016lX ", bitblt);
	uint32 sbp = bitblt & 0x3FFF; bitblt >>= 14;
	bitblt >>= 2;
	uint32 sbw = bitblt & 0x3F; bitblt >>= 6;
	bitblt >>= 2;
	uint32 spsm = bitblt & 0x3F; bitblt >>= 6;
	bitblt >>= 2;
	uint32 dbp = bitblt & 0x3FFF; bitblt >>= 14;
	bitblt >>= 2;
	uint32 dbw = bitblt & 0x3F; bitblt >>= 6;
	bitblt >>= 2;
	uint32 dpsm = bitblt & 0x3F; bitblt >>= 6;
	bitblt >>= 2;
	printf("SBP:%4X SBW:%2X SPSM:%2X DBP:%4X DBW:%2X DPSM:%2X\n",
		sbp, sbw, spsm, dbp, dbw, dpsm);
}

void
uploadRaster(Raster *raster)
{
	QWord q;
	uint128 tmp;
	int i;

	if(cachedTex == raster)
		return;
	cachedTex = raster;
	if(raster == nil)
		return;

	Ps2Raster *ext = GETPS2RASTEREXT(raster);
	Ps2Raster::PixelPtr *pp = (Ps2Raster::PixelPtr*)ext->data;
	uint128 *xferchain = (uint128*)(ext->data + 0x10);

	/* poor man's texture cache */
	static int pingpong;
	uint32 sz = (gsEnd - gsStart)/2;
	uint32 base = gsStart + pingpong*sz;
	pingpong = !pingpong;
	assert(sz*64 >= ext->totalSize);

	if(path2Textures){
		// This is really stupid right now:
		//  - no cache
		//  - PATH2
		for(i = 0; i < pp->numTransfers; i++){
			// add packets to upload texture levels & palette
			// DMAcnt
			vifPacket[vifPacksz++].q_u128 = *xferchain++;
			// GIFTAG
			vifPacket[vifPacksz++].q_u128 = *xferchain++;
			// BITBLTBUF
			q.q_u128 = *xferchain++;
			q.q_u32[1] += base;
			vifPacket[vifPacksz++] = q;
			// DMAref for actual GIF upload
			vifPacket[vifPacksz++].q_u128 = *xferchain++;
		}

		// End the primitive -- a bit ugly :/
		MAKEQ(tmp, VIFdirect + 1, VIFnop, 0, DMAcnt + 1);
		vifPacket[vifPacksz++].q_u128 = tmp;
		MAKE128(tmp, 0xe, SCE_GIF_SET_TAG(0, 1, 0,0, SCE_GIF_PACKED, 0));
		vifPacket[vifPacksz++].q_u128 = tmp;
	}else{
		if(synchTextures){
			MAKEQ(tmp, IntFlg|VIFnop, VIFmark | numTexBuild, 0, DMAcnt+1);
			vifPacket[vifPacksz++].q_u128 = tmp;
			MAKEQ(tmp, VIFflusha, VIFflusha, VIFflusha, VIFflusha);
			vifPacket[vifPacksz++].q_u128 = tmp;
		}else if(gateTextures){
			if(texMarkSlot == nil){
				/* Start chain of image uploads - first one is synchronous */
				MAKEQ(tmp, VIFflusha, VIFflush, VIFgate, DMAref + sizeof(VIFgate)/16);
				vifPacket[vifPacksz++].q_u128 = tmp;
			}else{
				/* Start image we need now at earlier point in chain */
				MAKEQ(tmp, VIFflusha, VIFflush, VIFgate, DMAref + sizeof(VIFgate)/16 - 1);
				texMarkSlot->q_u128 = tmp;
				/* and wait here for it to be transferred */
				MAKEQ(tmp, VIFflusha, VIFflusha, 0, DMAcnt);
				vifPacket[vifPacksz++].q_u128 = tmp;
			}
			/* Will start next transfer here */
			texMarkSlot = &vifPacket[vifPacksz];
			MAKEQ(tmp, VIFnop, VIFnop, 0, DMAcnt);
			vifPacket[vifPacksz++].q_u128 = tmp;
		}else{
			if(texMarkSlot == nil){
				/* Start chain of image uploads - first one is synchronous */
				MAKEQ(tmp, VIFnop, VIFflusha, 0, DMAcnt+2);
				vifPacket[vifPacksz++].q_u128 = tmp;
				MAKEQ(tmp, VIFnop, VIFflusha, IntFlg|VIFnop, VIFmark | numTexBuild);
				vifPacket[vifPacksz++].q_u128 = tmp;
			}else{
				/* Start image we need now at earlier point in chain */
				MAKEQ(tmp, VIFnop, VIFnop, IntFlg|VIFnop, VIFmark | numTexBuild);
				texMarkSlot->q_u128 = tmp;
				/* and wait here for it to be transferred */
				MAKEQ(tmp, VIFnop, VIFflusha, 0, DMAcnt+1);
				vifPacket[vifPacksz++].q_u128 = tmp;
			}
			/* Will start next transfer here */
			texMarkSlot = &vifPacket[vifPacksz];
			MAKEQ(tmp, VIFnop, VIFnop, VIFnop, VIFnop);
			vifPacket[vifPacksz++].q_u128 = tmp;
		}

		texListBuild[numTexBuild++] = gifPacket + gifPacksz;
		for(i = 0; i < pp->numTransfers; i++){
			// add packets to upload texture levels & palette
			// DMAcnt
			gifPacket[gifPacksz++].q_u128 = *xferchain++;
			// GIFTAG
			gifPacket[gifPacksz++].q_u128 = *xferchain++;
			// BITBLTBUF
			q.q_u128 = *xferchain++;
			q.q_u32[1] += base;
			gifPacket[gifPacksz++] = q;
			// DMAref for actual GIF upload
			gifPacket[gifPacksz++].q_u128 = *xferchain++;
		}

		// End the primitive -- a bit ugly :/
		if(!synchTextures && gateTextures)
			MAKEQ(tmp, VIFdirect + 1, VIFnop, 0, DMAcnt + 1);
		else
			MAKEQ(tmp, VIFdirect + 1, VIFnop, 0, DMAend + 1);
		gifPacket[gifPacksz++].q_u128 = tmp;
		MAKE128(tmp, 0xe, SCE_GIF_SET_TAG(0, 1, 0,0, SCE_GIF_PACKED, 0));
		gifPacket[gifPacksz++].q_u128 = tmp;
	}

	ext->tex0 = (ext->tex0 & ~0x3FFFUL) | base;
	if(ext->paletteBase)
		ext->tex0 = (ext->tex0 & ~(0x3FFFUL<<37)) | (uint64)(base+ext->paletteBase)<<37;
}

void
setTexture(Raster *raster, uint32 addressU, uint32 addressV, uint32 filterMode)
{
	uint64 tex1;
	if(raster == nil){
		gsRegs.prmode &= ~0x10;
		return;
	}
	gsRegs.prmode |= 0x10;
	Ps2Raster *ext = GETPS2RASTEREXT(raster);
	calcTEX1(raster, &tex1, filterMode);
	gsRegs.tex0_1 = ext->tex0;
	gsRegs.tex1_1 = tex1;
	gsRegs.clamp_1 = tex1;
	flushTex = true;
	if(addressU == Texture::WRAP || addressU == Texture::MIRROR)
		gsRegs.clamp_1 &= ~0x3UL;
	else
		gsRegs.clamp_1 = gsRegs.clamp_1&~0x3UL | 1;
	if(addressV == Texture::WRAP || addressV == Texture::MIRROR)
		gsRegs.clamp_1 &= ~0xCUL;
	else
		gsRegs.clamp_1 = gsRegs.clamp_1&~0xCUL | 4;
}

void
flushGSRegs(void)
{
	uint128 tmp;
	uint128 *p;

	// DMAcnt and GIFtag
	p = &vifPacket[vifPacksz].q_u128;
	vifPacksz += 2;

	uint32 oldPacksz = vifPacksz;
	if(prevGsRegs.alpha_1 != gsRegs.alpha_1){
		prevGsRegs.alpha_1 = gsRegs.alpha_1;
		MAKE128(tmp, SCE_GS_ALPHA_1, gsRegs.alpha_1);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
	if(prevGsRegs.prmode != gsRegs.prmode){
		prevGsRegs.prmode = gsRegs.prmode;
		MAKE128(tmp, SCE_GS_PRMODE, gsRegs.prmode);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
	if(prevGsRegs.fogcol != gsRegs.fogcol){
		prevGsRegs.fogcol = gsRegs.fogcol;
		MAKE128(tmp, SCE_GS_FOGCOL, gsRegs.fogcol);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}

	/* A new texture needs to write both TEXFLUSH *and* TEX0 for some reason.
	 * different pixels with the same TEX0 can still cause issues.
	 * probably because of CLUT buffer */
	if(flushTex){
		flushTex = false;
		prevGsRegs.tex0_1 = gsRegs.tex0_1;
		MAKE128(tmp, SCE_GS_TEXFLUSH, 0);
		vifPacket[vifPacksz++].q_u128 = tmp;
		MAKE128(tmp, SCE_GS_TEX0_1, gsRegs.tex0_1);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
	if(prevGsRegs.tex1_1 != gsRegs.tex1_1){
		prevGsRegs.tex1_1 = gsRegs.tex1_1;
		MAKE128(tmp, SCE_GS_TEX1_1, gsRegs.tex1_1);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
	if(prevGsRegs.clamp_1 != gsRegs.clamp_1){
		prevGsRegs.clamp_1 = gsRegs.clamp_1;
		MAKE128(tmp, SCE_GS_CLAMP_1, gsRegs.clamp_1);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
	if(prevGsRegs.test_1 != gsRegs.test_1){
		prevGsRegs.test_1 = gsRegs.test_1;
		MAKE128(tmp, SCE_GS_TEST_1, gsRegs.test_1);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}

	// nothing to add...
	int nregs = vifPacksz - oldPacksz;
	if(nregs == 0){
		vifPacksz -= 2;
		return;
	}

	MAKEQ(tmp, VIFdirect + nregs+1, VIFflush, 0, DMAcnt + nregs+1);
	*p++ = tmp;
	MAKE128(tmp, 0xe, SCE_GIF_SET_TAG(nregs, 1, 0,0, SCE_GIF_PACKED, 1));
	*p++ = tmp;
}

void
beginUpdate(Camera *cam)
{
	engine->currentCamera = cam;

	// Screw it, we're building our own matrices
	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  = -inv.at.x;
	view[9]  =  inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, &view, sizeof(RawMatrix));

	// Projection Matrix
	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		proj[10] = 2.0f*invz;
		proj[11] = 0.0f;

		proj[14] = -(cam->farPlane+cam->nearPlane)*invz;
		proj[15] = 1.0f;
	}
	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));

	float32 N = engine->device.zNear;
	float32 F = engine->device.zFar;
	// give us a small safe region for device z range
	// because clipping may not be totally exact
	N += (F - N)/10000.0f;
	F -= (F - N)/10000.0f;
	zScaleScreen = (F-N)/2.0f;
	zShiftScreen = (F+N)/2.0f;



	fogFarPlane = cam->farPlane;
//fogFarPlane = 3.0f;
//fogFarPlane = 10.0f;
	fogScale = -255.0f/(fogFarPlane - cam->fogPlane);
	fogShift = -fogFarPlane*fogScale;

	// TODO: subrastering
	int32 width = cam->frameBuffer->width;
	int32 height = cam->frameBuffer->height;

	// in 2D we only transform cam z to fog
	// and translate xy
	// NB: not all of those numbers are actually used
	vuConst.xyzwScale_2D.q_f[0] = 1.0f;
	vuConst.xyzwScale_2D.q_f[1] = 1.0f;
	vuConst.xyzwScale_2D.q_f[2] = 1.0f;
	vuConst.xyzwScale_2D.q_f[3] = fogScale;
	vuConst.xyzwOffset_2D.q_f[0] = 2048.0f - width/2;
	vuConst.xyzwOffset_2D.q_f[1] = 2048.0f - height/2;
	vuConst.xyzwOffset_2D.q_f[2] = 0.0f;
	vuConst.xyzwOffset_2D.q_f[3] = fogShift;

	// in 3D we do a bit more...
#ifdef CLIP_DEBUG
	vuConst.xyzwScale_3D.q_f[0] = width/4;
	vuConst.xyzwScale_3D.q_f[1] = -height/4;
#else
	vuConst.xyzwScale_3D.q_f[0] = width/2;
	vuConst.xyzwScale_3D.q_f[1] = -height/2;
#endif
//	vuConst.xyzwScale_3D.q_f[2] = zScaleScreen;
	vuConst.xyzwScale_3D.q_f[2] = cam->zScale;
	vuConst.xyzwScale_3D.q_f[3] = fogScale;
	vuConst.xyzwOffset_3D.q_f[0] = 2048.0f;
	vuConst.xyzwOffset_3D.q_f[1] = 2048.0f;
//	vuConst.xyzwOffset_3D.q_f[2] = zShiftScreen;
	vuConst.xyzwOffset_3D.q_f[2] = cam->zShift;
	vuConst.xyzwOffset_3D.q_f[3] = fogShift;

	vuConst.clipConsts.q_f[0] = cam->fogPlane;
	vuConst.clipConsts.q_f[1] = fogFarPlane;
	vuConst.clipConsts.q_f[2] = cam->nearPlane;
	vuConst.clipConsts.q_f[3] = cam->farPlane;
}

void
endUpdate(Camera *cam)
{
}

// has to be called outside of a dma packet
void
uploadVUCode(uint32 *code)
{
	uint128 tmp;
	if(code == currentVUCode)
		return;
	currentVUCode = code;
	MAKEQ(tmp, VIFnop, VIFnop, (uint32)code, DMAcall);
	vifPacket[vifPacksz++].q_u128 = tmp;
}

// Build combined matrix from world, view and projection
void
setMatrix(RawMatrix *combined, Matrix *world)
{
	RawMatrix tmp1, tmp2;
	if(world){
		convMatrix(&tmp1, world);
		RawMatrix::mult(&tmp2, &tmp1, &engine->currentCamera->devView);
		RawMatrix::mult(combined, &tmp2, &engine->currentCamera->devProj);
	}else{
		RawMatrix::mult(combined, &engine->currentCamera->devView, &engine->currentCamera->devProj);
	}
}

static int
startPS2(void)
{
	uint128 tmp;

	colorNoScale.q_f[0] = 1.0f;
	colorNoScale.q_f[1] = 1.0f;
	colorNoScale.q_f[2] = 1.0f;
	colorNoScale.q_f[3] = COLSCALE;
	colorTexScale.q_f[0] = COLSCALE;
	colorTexScale.q_f[1] = COLSCALE;
	colorTexScale.q_f[2] = COLSCALE;
	colorTexScale.q_f[3] = COLSCALE;

	rw::ps2::rwStateCache.addressU = rw::Texture::WRAP;
	rw::ps2::rwStateCache.addressV = rw::Texture::WRAP;
	rw::ps2::rwStateCache.filterMode = rw::Texture::NEAREST;
	rw::ps2::rwStateCache.srcblend = rw::BLENDSRCALPHA;
	rw::ps2::rwStateCache.destblend = rw::BLENDINVSRCALPHA;
	rw::ps2::setAlphaBlend(rw::ps2::rwStateCache.srcblend, rw::ps2::rwStateCache.destblend);
	// gouraud, no RW state for that right now
	// TODO: make sure RW states and GS regs are in synch on init
	rw::ps2::gsRegs.prmode |= 0x8;
//	rw::ps2::gsRegs.alpha_1 = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
	rw::ps2::gsRegs.test_1 = SCE_GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);

	sceGsResetPath();
	sceDmaReset(1);

	// TODO: get all this stuff from videomode
	gsCrtState.inter = SCE_GS_INTERLACE;
	gsCrtState.mode = VIDEOMODE;
	gsCrtState.ff = SCE_GS_FIELD;	// SCE_GS_FRAME;
	sceGsResetGraph(0, gsCrtState.inter, gsCrtState.mode, gsCrtState.ff);

	GsInitCtx(&gsCtx, SCREEN_WIDTH, SCREEN_HEIGHT, SCE_GS_PSMCT32, SCE_GS_PSMZ24);
	gsStart = gsAllocPtr/4/64;

	// Enable FINISH and SIGNAL interrupts (we're not even using SIGNAL)
	AddIntcHandler(INTC_GS, gsHandler, 0);
	EnableIntc(INTC_GS);
	*GS_CSR = GS_CSR_FINISH_M | GS_CSR_SIGNAL_M;
	*GS_IMR = ~(GS_IMR_FINISHMSK_M | GS_IMR_SIGMSK_M);

	AddDmacHandler(DMAC_VIF1, dmacVif1Handler, 0);
	EnableDmac(DMAC_VIF1);

	AddIntcHandler(INTC_VIF1, vif1Handler, 0);
	EnableIntc(INTC_VIF1);

	dmaGif = sceDmaGetChan(SCE_DMA_GIF);
	dmaGif->chcr.TTE = 0;
	*GIF_MODE = 4;	// IMT intermittent mode

	dmaVif = sceDmaGetChan(SCE_DMA_VIF1);
	dmaVif->chcr.TTE = 1;
	dmaVif->chcr.TIE = 1;

	dmaInit();

	dmaFlip(0);
	rw::ps2::InitGSregs();
	// init ROW register to write cleared ADC flag (adcHack variable)
	MAKEQ(tmp, VIFstrow, VIFstmod, 0, DMAcnt + 1);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKEQ(tmp, 0, 0, 0, 0);
	vifPacket[vifPacksz++].q_u128 = tmp;
	dmaKick();
	sceGsSyncPath(0, 0);

	return 1;
}

// TODO: implement the whole thing
int
deviceSystem(DeviceReq req, void *arg, int32 n)
{       
	switch(req){
	case DEVICEOPEN:
		// perhaps do this here?
		rw::engine->filefuncs.rwfopen = ps2fopen;
		rw::engine->filefuncs.rwfclose = ps2fclose;
		rw::engine->filefuncs.rwfseek = ps2fseek;
		rw::engine->filefuncs.rwftell = ps2ftell;
		rw::engine->filefuncs.rwfread = ps2fread;
		rw::engine->filefuncs.rwfwrite = ps2fwrite;
		rw::engine->filefuncs.rwfeof = ps2feof;

		// before querying videomodes. probably not much to do here
		break;
	case DEVICECLOSE:
		break;

	case DEVICEINIT:
		// before plugins are constructed
		return startPS2();
	case DEVICETERM:
		break;

	case DEVICEFINALIZE:
		// after plugins are constructed
		break;

	default:
		printf("system request %d not implemented\n", req);
		return 0;
	}
	return 1;
}

// This isn't optimal...
void
beginFrame(int frame)
{
	dmaFlip(frame&1);
	GsSetDisp(&gsCtx.disp[frame&1]);
	GsSetDraw(&gsCtx.draw[frame&1]);
}

void
endFrame(float *t1, float *t2)
{
	sceGsSyncPath(0, 0);
	*t1 = GetTimeF();
	// we could start uploading textures here now so they'll be ready after vsynch

	sceGsSyncV(0);
	*t2 = GetTimeF();

	dmaKick();
}

Device renderdevice = {
	16777215.0f, 0.0f,
	ps2::beginUpdate,
	ps2::endUpdate,
	ps2::clearCamera,
	null::showRaster,
	null::rasterRenderFast,
	ps2::setRenderState,
	ps2::getRenderState,
	null::im2DRenderLine,
	null::im2DRenderTriangle,
	ps2::renderPrim_VU,	//im2DRenderPrimitive,
	ps2::renderIndexedPrim_VU,	//im2DRenderIndexedPrimitive,
	ps2::vuIm3DTransform,
	null::im3DRenderPrimitive,
	ps2::vuIm3DRenderIndexed,	//vuIm3DRenderIndexedPrimitive,
	ps2::vuIm3DEnd,
	ps2::deviceSystem
};

}
}

#endif
