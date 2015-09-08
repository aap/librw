#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwd3d.h"

using namespace std;

namespace rw {
namespace d3d {

#ifdef RW_D3D9
IDirect3DDevice9 *device = NULL;
#endif

int vertFormatMap[] = {
	-1, VERT_FLOAT2, VERT_FLOAT3, -1, VERT_ARGB
};

void*
createIndexBuffer(uint32 length)
{
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf;
	device->CreateIndexBuffer(length, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, 0);
	return ibuf;
#else
	return new uint8[length];
#endif
}

uint16*
lockIndices(void *indexBuffer, uint32 offset, uint32 size, uint32 flags)
{
	if(indexBuffer == NULL)
		return NULL;
#ifdef RW_D3D9
	uint16 *indices;
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Lock(offset, size, (void**)&indices, flags);
	return indices;
#else
	return (uint16*)indexBuffer;
#endif
}

void
unlockIndices(void *indexBuffer)
{
	if(indexBuffer == NULL)
		return;
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Unlock();
#endif
}

void*
createVertexBuffer(uint32 length, uint32 fvf, int32 pool)
{
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vbuf;
	device->CreateVertexBuffer(length, D3DUSAGE_WRITEONLY, fvf, (D3DPOOL)pool, &vbuf, 0);
	return vbuf;
#else
	return new uint8[length];
#endif
}

uint8*
lockVertices(void *vertexBuffer, uint32 offset, uint32 size, uint32 flags)
{
	if(vertexBuffer == NULL)
		return NULL;
#ifdef RW_D3D9
	uint8 *verts;
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Lock(offset, size, (void**)&verts, flags);
	return verts;
#else
	return (uint8*)vertexBuffer;
#endif
}

void
unlockVertices(void *vertexBuffer)
{
	if(vertexBuffer == NULL)
		return;
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Unlock();
#endif
}

void
deleteObject(void *object)
{
	if(object == NULL)
		return;
#ifdef RW_D3D9
	IUnknown *unk = (IUnknown*)object;
	unk->Release();
#else
	delete[] (uint*)object;
#endif
}

}
}
