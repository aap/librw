#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3d9.h"

namespace rw {
namespace d3d {

#ifdef RW_D3D9

// might want to tweak this
#define NUMINDICES 10000
#define NUMVERTICES 10000

static int primTypeMap[] = {
	D3DPT_POINTLIST,	// invalid
	D3DPT_LINELIST,
	D3DPT_LINESTRIP,
	D3DPT_TRIANGLELIST,
	D3DPT_TRIANGLESTRIP,
	D3DPT_TRIANGLEFAN,
	D3DPT_POINTLIST,	// actually not supported!
};

static IDirect3DVertexDeclaration9 *im2ddecl;
static IDirect3DVertexBuffer9 *im2dvertbuf;
static IDirect3DIndexBuffer9 *im2dindbuf;

void
openIm2D(void)
{
	D3DVERTEXELEMENT9 elements[4] = {
		{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
		{ 0, offsetof(Im2DVertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		{ 0, offsetof(Im2DVertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	d3ddevice->CreateVertexDeclaration((D3DVERTEXELEMENT9*)elements, &im2ddecl);
	im2dvertbuf = (IDirect3DVertexBuffer9*)createVertexBuffer(NUMVERTICES*sizeof(Im2DVertex), 0, D3DPOOL_MANAGED);
	im2dindbuf = (IDirect3DIndexBuffer9*)createIndexBuffer(NUMINDICES*sizeof(uint16));
}

void
closeIm2D(void)
{
	deleteObject(im2ddecl);
	deleteObject(im2dvertbuf);
	deleteObject(im2dindbuf);
}

void
im2DRenderIndexedPrimitive(PrimitiveType primType,
   void *vertices, int32 numVertices, void *indices, int32 numIndices)
{
	if(numVertices > NUMVERTICES ||
	   numIndices > NUMINDICES){
		// TODO: error
		return;
	}
	uint16 *lockedindices = lockIndices(im2dindbuf, 0, numIndices*sizeof(uint16), 0);
	memcpy(lockedindices, indices, numIndices*sizeof(uint16));
	unlockIndices(im2dindbuf);

	uint8 *lockedvertices = lockVertices(im2dvertbuf, 0, numVertices*sizeof(Im2DVertex), D3DLOCK_NOSYSLOCK);
	memcpy(lockedvertices, vertices, numVertices*sizeof(Im2DVertex));
	unlockVertices(im2dvertbuf);

	d3ddevice->SetStreamSource(0, im2dvertbuf, 0, sizeof(Im2DVertex));
	d3ddevice->SetIndices(im2dindbuf);
	d3ddevice->SetVertexDeclaration(im2ddecl);
	setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);

	d3d::flushCache();

	uint32 primCount = 0;
	switch(primType){
	case PRIMTYPELINELIST:
		primCount = numIndices/2;
		break;
	case PRIMTYPEPOLYLINE:
		primCount = numIndices-1;
		break;
	case PRIMTYPETRILIST:
		primCount = numIndices/3;
		break;
	case PRIMTYPETRISTRIP:
		primCount = numIndices-2;
		break;
	case PRIMTYPETRIFAN:
		primCount = numIndices-2;
		break;
	case PRIMTYPEPOINTLIST:
		primCount = numIndices;
		break;
	}
	d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)primTypeMap[primType], 0,
	                                0, numVertices,
	                                0, primCount);
}


// Im3D


static IDirect3DVertexDeclaration9 *im3ddecl;
static IDirect3DVertexBuffer9 *im3dvertbuf;
static IDirect3DIndexBuffer9 *im3dindbuf;
static int32 num3DVertices;

void
openIm3D(void)
{
	D3DVERTEXELEMENT9 elements[4] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, offsetof(Im3DVertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		{ 0, offsetof(Im3DVertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};
	d3ddevice->CreateVertexDeclaration((D3DVERTEXELEMENT9*)elements, &im3ddecl);
	im3dvertbuf = (IDirect3DVertexBuffer9*)createVertexBuffer(NUMVERTICES*sizeof(Im3DVertex), 0, D3DPOOL_MANAGED);
	im3dindbuf = (IDirect3DIndexBuffer9*)createIndexBuffer(NUMINDICES*sizeof(uint16));
}

void
closeIm3D(void)
{
	deleteObject(im3ddecl);
	deleteObject(im3dvertbuf);
	deleteObject(im3dindbuf);
}

void
im3DTransform(void *vertices, int32 numVertices, Matrix *world)
{
	RawMatrix d3dworld;
	d3d::setRenderState(D3DRS_LIGHTING, 0);

	if(world == nil){
		Matrix ident;
		ident.setIdentity();
		world = &ident;
	}
	convMatrix(&d3dworld, world);
	d3ddevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&d3dworld);

	uint8 *lockedvertices = lockVertices(im3dvertbuf, 0, numVertices*sizeof(Im3DVertex), D3DLOCK_NOSYSLOCK);
	memcpy(lockedvertices, vertices, numVertices*sizeof(Im3DVertex));
	unlockVertices(im3dvertbuf);

	d3ddevice->SetStreamSource(0, im3dvertbuf, 0, sizeof(Im3DVertex));
	d3ddevice->SetVertexDeclaration(im3ddecl);

	num3DVertices = numVertices;
}

void
im3DRenderIndexed(PrimitiveType primType, void *indices, int32 numIndices)
{
	uint16 *lockedindices = lockIndices(im3dindbuf, 0, numIndices*sizeof(uint16), 0);
	memcpy(lockedindices, indices, numIndices*sizeof(uint16));
	unlockIndices(im3dindbuf);

	d3ddevice->SetIndices(im3dindbuf);
	setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
	d3d::flushCache();

	uint32 primCount = 0;
	switch(primType){
	case PRIMTYPELINELIST:
		primCount = numIndices/2;
		break;
	case PRIMTYPEPOLYLINE:
		primCount = numIndices-1;
		break;
	case PRIMTYPETRILIST:
		primCount = numIndices/3;
		break;
	case PRIMTYPETRISTRIP:
		primCount = numIndices-2;
		break;
	case PRIMTYPETRIFAN:
		primCount = numIndices-2;
		break;
	case PRIMTYPEPOINTLIST:
		primCount = numIndices;
		break;
	}
	d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
	d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)primTypeMap[primType], 0,
	                                0, num3DVertices,
	                                0, primCount);
}

void
im3DEnd(void)
{
}

#endif
}
}
