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
	d3d::setTexture(0, engine->imtexture);
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

#endif
}
}
