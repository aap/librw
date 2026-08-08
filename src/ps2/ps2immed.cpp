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

#include <eestruct.h>

#define PLUGIN_ID 2

extern rw::ps2::VUdesc vu1_im3d_desc[7];
extern rw::ps2::VUdesc vu1_im2d_desc[2];
extern rw::uint32 vu1_im2d[];
extern rw::uint32 vu1_im3d[];

namespace rw {
namespace ps2 {

struct BatchInfo
{
	uint32 numBatches;
	uint32 batchSize;
	int32 repeat;
};

int
prologue(BatchInfo *bi, PrimitiveType type, int32 numVerts)
{
	// 2 words for GIF tags, 2 input and output buffers, 3 qw per vertex
	bi->batchSize = (vuMatrix - 2)/(2*3 + 2*3);
	int primsz = primSize[type];
	bi->repeat = primRepeat[type];
	int numPrims = 0;
	int numPrimsBatch = 0;
	switch(type){
	case PRIMTYPEPOINTLIST:
	case PRIMTYPELINELIST:
	case PRIMTYPETRILIST:
		numPrims = numVerts/primsz;
		numPrimsBatch = bi->batchSize/primsz;
		numPrimsBatch &= ~3;
		bi->batchSize = numPrimsBatch*primsz;
		break;
	case PRIMTYPEPOLYLINE:
	case PRIMTYPETRISTRIP:
	case PRIMTYPETRIFAN:
		numPrims = numVerts - bi->repeat;
		bi->batchSize &= ~3;
		numPrimsBatch = bi->batchSize - bi->repeat;
		break;
	}
	// nothing to draw
	if(bi->batchSize < primSize[type])
		return 1;

	bi->numBatches = (numPrims + numPrimsBatch-1) / numPrimsBatch;

	uploadRaster(rwStateCache.raster);
	setTexture(rwStateCache.raster, rwStateCache.addressU, rwStateCache.addressV, rwStateCache.filterMode);

	flushGSRegs();
	return 0;
}

int
prologue2d(BatchInfo *bi, PrimitiveType type, int32 numVerts)
{
	uint128 tmp;

	prologue(bi, type, numVerts);

	uploadVUCode(vu1_im2d);

	// DMAcnt for everything
	int dmasz = 8	// general VIF unpacks
		+ 3*(numVerts + (bi->numBatches-1)*bi->repeat)	// vertices
		+ bi->numBatches*2;	// batch prolog and epilog
	assert(dmasz <= 0xFFFF);
	MAKEQ(tmp, VIFflush, VIFflush, 0, DMAcnt + dmasz);
	vifPacket[vifPacksz++].q_u128 = tmp;

	// some uploads and double buffer
	uint32 offset = bi->batchSize*3;
	MAKEQ(tmp, UNPACK(V4_32, 2, vuXyzwScale), STCYCL(4,4), VIFoffset + offset, VIFbase + 0);
	vifPacket[vifPacksz++].q_u128 = tmp;
	vifPacket[vifPacksz++] = vuConst.xyzwScale_2D;
	vifPacket[vifPacksz++] = vuConst.xyzwOffset_2D;

	MAKEQ(tmp, UNPACK(V4_32, 2, vuGifTag), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, 0x412, SCE_GIF_SET_TAG(0, 1, 1,rw2gsPrim[type], SCE_GIF_PACKED, 3));
	vifPacket[vifPacksz++].q_u128 = tmp;
	vifPacket[vifPacksz++] = (rwStateCache.raster == nil || doModulate2) ? colorNoScale : colorTexScale;

	MAKEQ(tmp, UNPACK(V4_32, 1, vuVuSwitch), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	VUdesc *desc = rwStateCache.fogEnable ? &vu1_im2d_desc[1] : &vu1_im2d_desc[0];
	MAKEQ(tmp, 0, 0, 0, desc->process>>3);
	vifPacket[vifPacksz++].q_u128 = tmp;

	return 0;
}

int
prologue3d(BatchInfo *bi, PrimitiveType type, int32 numVerts)
{
	uint128 tmp;

	prologue(bi, type, numVerts);

	uploadVUCode(vu1_im3d);

	// DMAcnt for everything
	int dmasz = 13	// general VIF unpacks
		+ 3*(numVerts + (bi->numBatches-1)*bi->repeat)	// vertices
		+ bi->numBatches*2;	// batch prolog and epilog
	assert(dmasz <= 0xFFFF);
	MAKEQ(tmp, VIFflush, VIFflush, 0, DMAcnt + dmasz);
	vifPacket[vifPacksz++].q_u128 = tmp;

	// some uploads and double buffer
	uint32 offset = bi->batchSize*3;
	MAKEQ(tmp, UNPACK(V4_32, 7, vuMatrix), STCYCL(4,4), VIFoffset + offset, VIFbase + 0);
	vifPacket[vifPacksz++].q_u128 = tmp;
	vifPacket[vifPacksz++] = vuConst.mat0;
	vifPacket[vifPacksz++] = vuConst.mat1;
	vifPacket[vifPacksz++] = vuConst.mat2;
	vifPacket[vifPacksz++] = vuConst.mat3;
	vifPacket[vifPacksz++] = vuConst.xyzwScale_3D;
	vifPacket[vifPacksz++] = vuConst.xyzwOffset_3D;
	vifPacket[vifPacksz++] = vuConst.clipConsts;

	MAKEQ(tmp, UNPACK(V4_32, 2, vuGifTag), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, 0x412, SCE_GIF_SET_TAG(0, 1, 1,rw2gsPrim[type], SCE_GIF_PACKED, 3));
	vifPacket[vifPacksz++].q_u128 = tmp;
	vifPacket[vifPacksz++] = (rwStateCache.raster == nil || doModulate2) ? colorNoScale : colorTexScale;

	MAKEQ(tmp, UNPACK(V4_32, 1, vuVuSwitch), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	VUdesc *desc;
	if(!doClipping)
		desc = &vu1_im3d_desc[0];
	else if(type != PRIMTYPETRIFAN && type <= PRIMTYPEPOINTLIST)
		desc = &vu1_im3d_desc[type];
	else
		printf("invalid primitive type\n");
	MAKEQ(tmp, 0, desc->buf2, desc->buf1, desc->process>>3);
	vifPacket[vifPacksz++].q_u128 = tmp;

	return 0;
}

void
renderPrim_VU(PrimitiveType type, void *verts, int32 numVerts)
{
	uint128 tmp;
	BatchInfo bi;

	if(prologue2d(&bi, type, numVerts))
		return;

	int i, j, vx;
	Im2DVertex *v;
	vx = 0;
	for(i = 0; i < bi.numBatches; i++){
		int32 vertCount = numVerts;
		if(vertCount > bi.batchSize)
			vertCount = bi.batchSize;

		MAKEQ(tmp, UNPACK(V4_32, vertCount*3, 0x8000 + 0), STCYCL(4,4), VIFnop, VIFnop);
		vifPacket[vifPacksz++].q_u128 = tmp;
		for(j = 0; j < vertCount; j++){
			if(j == 0 && type == PRIMTYPETRIFAN)
				v = &((Im2DVertex*)verts)[0];
			else
				v = &((Im2DVertex*)verts)[vx];
			memcpy(&vifPacket[vifPacksz], v, 3*16);
			vifPacksz += 3;
			vx++;
		}
		uint64 call, flush;
		if(i == 0)
			call = UINT64(VIFmscalf + 0, VIFitop + vertCount);
		else
			call = UINT64(VIFmscnt, VIFitop + vertCount);
		if(i == bi.numBatches-1)
			flush = UINT64(VIFflush, VIFflush);
		else
			flush = UINT64(VIFnop, VIFnop);
		MAKE128(tmp, flush, call);
		vifPacket[vifPacksz++].q_u128 = tmp;

		numVerts -= vertCount;
		numVerts += bi.repeat;
		vx -= bi.repeat;
	}
}

void
renderIndexedPrim_VU(PrimitiveType type, void *verts, int32 numVerts, void *indices, int32 numIndices)
{
	uint128 tmp;
	BatchInfo bi;

	if(prologue2d(&bi, type, numIndices))
		return;

	int i, j, ix, vx;
	Im2DVertex *v;
	ix = 0;
	for(i = 0; i < bi.numBatches; i++){
		int32 vertCount = numIndices;
		if(vertCount > bi.batchSize)
			vertCount = bi.batchSize;

		MAKEQ(tmp, UNPACK(V4_32, vertCount*3, 0x8000 + 0), STCYCL(4,4), VIFnop, VIFnop);
		vifPacket[vifPacksz++].q_u128 = tmp;
		for(j = 0; j < vertCount; j++){
			if(j == 0 && type == PRIMTYPETRIFAN)
				vx = ((uint16*)indices)[0];
			else
				vx = ((uint16*)indices)[ix];
			v = &((Im2DVertex*)verts)[vx];
			memcpy(&vifPacket[vifPacksz], v, 3*16);
			vifPacksz += 3;
			ix++;
		}
		uint64 call, flush;
		if(i == 0)
			call = UINT64(VIFmscalf + 0, VIFitop + vertCount);
		else
			call = UINT64(VIFmscnt, VIFitop + vertCount);
		if(i == bi.numBatches-1)
			flush = UINT64(VIFflush, VIFflush);
		else
			flush = UINT64(VIFnop, VIFnop);
		MAKE128(tmp, flush, call);
		vifPacket[vifPacksz++].q_u128 = tmp;

		numIndices -= vertCount;
		numIndices += bi.repeat;
		ix -= bi.repeat;
	}
}

/*********
 * VU Im3D
 *********/

static Im3DVertex *im3dVerts;

void
vuIm3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	im3dVerts = (Im3DVertex*)vertices;
	setMatrix((RawMatrix*)&vuConst.mat0, world);
}

void
vuIm3DEnd(void)
{
}

void
vuIm3DRenderIndexed(PrimitiveType type, void *indices, int32 numIndices)
{
	uint128 tmp;
	BatchInfo bi;
	QWord q;

	if(prologue3d(&bi, type, numIndices))
		return;

	int i, j, ix, vx;
	Im3DVertex *v;
	ix = 0;
	int first = 1;
	for(i = 0; i < bi.numBatches; i++){
		int32 vertCount = numIndices;
		if(vertCount > bi.batchSize)
			vertCount = bi.batchSize;

		MAKEQ(tmp, UNPACK(V4_32, vertCount*3, 0x8000 + 0), STCYCL(4,4), VIFnop, VIFnop);
		vifPacket[vifPacksz++].q_u128 = tmp;
		for(j = 0; j < vertCount; j++){
			if(j == 0 && type == PRIMTYPETRIFAN)
				vx = ((uint16*)indices)[0];
			else
				vx = ((uint16*)indices)[ix];
			v = &im3dVerts[vx];
			q.q_f[0] = v->position.x;
			q.q_f[1] = v->position.y;
			q.q_f[2] = v->position.z;
			q.q_f[3] = 0.0f;		// ADC flag
			vifPacket[vifPacksz++] = q;
			q.q_f[0] = v->u;
			q.q_f[1] = v->v;
			q.q_f[2] = 0.0f;
			q.q_f[3] = 0.0f;
			vifPacket[vifPacksz++] = q;
			ix++;
			q.q_u32[0] = v->r;
			q.q_u32[1] = v->g;
			q.q_u32[2] = v->b;
			q.q_u32[3] = v->a;
			vifPacket[vifPacksz++] = q;
		}
		uint64 call, flush;
		if(i == 0)
			call = UINT64(VIFmscalf + 0, VIFitop + vertCount);
		else
			call = UINT64(VIFmscnt, VIFitop + vertCount);
		if(i == bi.numBatches-1)
			flush = UINT64(VIFflush, VIFflush);
		else
			flush = UINT64(VIFnop, VIFnop);
		MAKE128(tmp, flush, call);
		vifPacket[vifPacksz++].q_u128 = tmp;

		numIndices -= vertCount;
		numIndices += bi.repeat;
		ix -= bi.repeat;
	}
}

}
}

#endif
