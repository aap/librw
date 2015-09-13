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

void*
createTexture(int32 width, int32 height, int32 levels, uint32 format)
{
#ifdef RW_D3D9
	IDirect3DTexture9 *tex;
	device->CreateTexture(width, height, levels, 0,
	                      (D3DFORMAT)format, D3DPOOL_MANAGED, &tex, NULL);
	return tex;
#else
	assert(0 && "only supported with RW_D3D9");
#endif
}

uint8*
lockTexture(void *texture, int32 level)
{
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)texture;
	D3DLOCKED_RECT lr;
	tex->LockRect(level, &lr, 0, 0);
	return (uint8*)lr.pBits;
#else
	assert(0 && "only supported with RW_D3D9");
#endif
}

void
unlockTexture(void *texture, int32 level)
{
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)texture;
	tex->UnlockRect(level);
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

// Native Raster

int32 nativeRasterOffset;

void
makeNativeRaster(Raster *raster)
{
	static uint32 formatMap[] = {
		0,
		D3DFMT_A1R5G5B5,
		D3DFMT_R5G6B5,
		D3DFMT_A4R4G4B4,
		D3DFMT_L8,
		D3DFMT_A8R8G8B8,
		D3DFMT_X8R8G8B8,
		0, 0, 0,
		D3DFMT_X1R5G5B5,
		0, 0, 0, 0, 0
	};
	static bool32 alphaMap[] = {
		0,
		1,
		0,
		1,
		0,
		1,
		0,
		0, 0, 0,
		0,
		0, 0, 0, 0, 0
	};
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	if(raster->flags & 0x80)
		return;
	uint32 format = formatMap[(raster->format >> 8) & 0xF];
	ras->format = 0;
	ras->hasAlpha = alphaMap[(raster->format >> 8) & 0xF];
	ras->texture = createTexture(raster->width, raster->width,
	                             raster->format & Raster::MIPMAP ? 0 : 1,
	                             format);
	assert((raster->flags & (Raster::PAL4 | Raster::PAL8)) == 0);
}

uint8*
lockRaster(Raster *raster, int32 level)
{
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	return lockTexture(ras->texture, level);
}

void
unlockRaster(Raster *raster, int32 level)
{
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	unlockTexture(ras->texture, level);
}

int32
getNumLevels(Raster *raster)
{
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)ras->texture;
	return tex->GetLevelCount();
#else
	assert(0 && "only supported with RW_D3D9");
#endif
}

// stolen from d3d8to9
uint32
calculateTextureSize(uint32 width, uint32 height, uint32 depth, uint32 format)
{
#define D3DFMT_W11V11U10 65
	switch(format){
	default:
	case D3DFMT_UNKNOWN:
		return 0;
	case D3DFMT_R3G3B2:
	case D3DFMT_A8:
	case D3DFMT_P8:
	case D3DFMT_L8:
	case D3DFMT_A4L4:
		return width * height * depth;
	case D3DFMT_R5G6B5:
	case D3DFMT_X1R5G5B5:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_X4R4G4B4:
	case D3DFMT_A8P8:
	case D3DFMT_A8L8:
	case D3DFMT_V8U8:
	case D3DFMT_L6V5U5:
	case D3DFMT_D16_LOCKABLE:
	case D3DFMT_D15S1:
	case D3DFMT_D16:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
		return width * 2 * height * depth;
	case D3DFMT_R8G8B8:
		return width * 3 * height * depth;
	case D3DFMT_A8R8G8B8:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_X8B8G8R8:
	case D3DFMT_G16R16:
	case D3DFMT_X8L8V8U8:
	case D3DFMT_Q8W8V8U8:
	case D3DFMT_V16U16:
	case D3DFMT_W11V11U10:
	case D3DFMT_A2W10V10U10:
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_D24X8:
	case D3DFMT_D24X4S4:
		return width * 4 * height * depth;
	case D3DFMT_DXT1:
		assert(depth <= 1);
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 8;
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
		assert(depth <= 1);
		return ((width + 3) >> 2) * ((height + 3) >> 2) * 16;
	}
}

int32
getLevelSize(Raster *raster, int32 level)
{
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)ras->texture;
	D3DSURFACE_DESC desc;
	tex->GetLevelDesc(level, &desc);
	return calculateTextureSize(desc.Width, desc.Height, 1, desc.Format);
#else
	assert(0 && "only supported with RW_D3D9");
#endif
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	D3dRaster *raster = PLUGINOFFSET(D3dRaster, object, offset);
	raster->texture = NULL;
	raster->palette = NULL;
	raster->format = 0;
	raster->hasAlpha = 0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	// TODO:
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	D3dRaster *raster = PLUGINOFFSET(D3dRaster, dst, offset);
	raster->texture = NULL;
	raster->palette = NULL;
	raster->format = 0;
	raster->hasAlpha = 0;
	return dst;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(D3dRaster),
	                                            0x12340000 | PLATFORM_D3D9, 
                                                    createNativeRaster,
                                                    destroyNativeRaster,
                                                    copyNativeRaster);
}

}
}
