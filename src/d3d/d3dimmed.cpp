#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3d9.h"
#include "rwd3dimpl.h"

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
	assert(im2ddecl);
	d3d9Globals.numVertexDeclarations++;
	im2dvertbuf = (IDirect3DVertexBuffer9*)createVertexBuffer(NUMVERTICES*sizeof(Im2DVertex), 0, true);
	assert(im2dvertbuf);
	addDynamicVB(NUMVERTICES*sizeof(Im2DVertex), 0, &im2dvertbuf);
	im2dindbuf = (IDirect3DIndexBuffer9*)createIndexBuffer(NUMINDICES*sizeof(uint16));
	assert(im2dindbuf);
}

void
closeIm2D(void)
{
	deleteObject(im2ddecl);
	d3d9Globals.numVertexDeclarations--;
	im2ddecl = nil;

	removeDynamicVB(&im2dvertbuf);
	deleteObject(im2dvertbuf);
	d3d9Globals.numVertexBuffers--;
	im2dvertbuf = nil;

	deleteObject(im2dindbuf);
	d3d9Globals.numIndexBuffers--;
	im2dindbuf = nil;
}

static Im2DVertex tmpprimbuf[3];

void
im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	im2DRenderPrimitive(PRIMTYPELINELIST, tmpprimbuf, 2);
}

void
im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmpprimbuf[0] = verts[vert1];
	tmpprimbuf[1] = verts[vert2];
	tmpprimbuf[2] = verts[vert3];
	im2DRenderPrimitive(PRIMTYPETRILIST, tmpprimbuf, 3);
}

void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	if(numVertices > NUMVERTICES){
		// TODO: error
		return;
	}
	uint8 *lockedvertices = lockVertices(im2dvertbuf, 0, numVertices*sizeof(Im2DVertex), D3DLOCK_DISCARD);
	memcpy(lockedvertices, vertices, numVertices*sizeof(Im2DVertex));
	unlockVertices(im2dvertbuf);

	d3ddevice->SetStreamSource(0, im2dvertbuf, 0, sizeof(Im2DVertex));
	d3ddevice->SetVertexDeclaration(im2ddecl);

	if(engine->device.getRenderState(TEXTURERASTER))
		setPixelShader(default_color_tex_PS);
	else
		setPixelShader(default_color_PS);

	d3d::flushCache();

	uint32 primCount = 0;
	switch(primType){
	case PRIMTYPELINELIST:
		primCount = numVertices/2;
		break;
	case PRIMTYPEPOLYLINE:
		primCount = numVertices-1;
		break;
	case PRIMTYPETRILIST:
		primCount = numVertices/3;
		break;
	case PRIMTYPETRISTRIP:
		primCount = numVertices-2;
		break;
	case PRIMTYPETRIFAN:
		primCount = numVertices-2;
		break;
	case PRIMTYPEPOINTLIST:
		primCount = numVertices;
		break;
	}
	d3ddevice->DrawPrimitive((D3DPRIMITIVETYPE)primTypeMap[primType], 0, primCount);

	setPixelShader(nil);
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

	uint8 *lockedvertices = lockVertices(im2dvertbuf, 0, numVertices*sizeof(Im2DVertex), D3DLOCK_DISCARD);
	memcpy(lockedvertices, vertices, numVertices*sizeof(Im2DVertex));
	unlockVertices(im2dvertbuf);

	d3ddevice->SetStreamSource(0, im2dvertbuf, 0, sizeof(Im2DVertex));
	d3ddevice->SetIndices(im2dindbuf);
	d3ddevice->SetVertexDeclaration(im2ddecl);

	if(engine->device.getRenderState(TEXTURERASTER))
		setPixelShader(default_color_tex_PS);
	else
		setPixelShader(default_color_PS);

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

	setPixelShader(nil);
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
	assert(im3ddecl);
	d3d9Globals.numVertexDeclarations++;
	im3dvertbuf = (IDirect3DVertexBuffer9*)createVertexBuffer(NUMVERTICES*sizeof(Im3DVertex), 0, true);
	assert(im3dvertbuf);
	addDynamicVB(NUMVERTICES*sizeof(Im3DVertex), 0, &im3dvertbuf);
	im3dindbuf = (IDirect3DIndexBuffer9*)createIndexBuffer(NUMINDICES*sizeof(uint16));
	assert(im3dindbuf);
}

void
closeIm3D(void)
{
	deleteObject(im3ddecl);
	d3d9Globals.numVertexDeclarations--;
	im3ddecl = nil;

	removeDynamicVB(&im3dvertbuf);
	deleteObject(im3dvertbuf);
	d3d9Globals.numVertexBuffers--;
	im3dvertbuf = nil;

	deleteObject(im3dindbuf);
	d3d9Globals.numIndexBuffers--;
	im3dindbuf = nil;
}

void
im3DTransform(void *vertices, int32 numVertices, Matrix *world)
{
	if(world == nil)
		uploadMatrices();
	else
		uploadMatrices(world);

	uint8 *lockedvertices = lockVertices(im3dvertbuf, 0, numVertices*sizeof(Im3DVertex), D3DLOCK_DISCARD);
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

	if(engine->device.getRenderState(TEXTURERASTER))
		setPixelShader(default_color_tex_PS);
	else
		setPixelShader(default_color_PS);

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

	setPixelShader(nil);
}

void
im3DEnd(void)
{
}

#endif
}
}
