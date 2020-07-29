#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3dimpl.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace d3d {

bool32 isP8supported = 1;

#ifndef RW_D3D9
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
            ((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |       \
            ((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))
enum {
	D3DFMT_UNKNOWN              =  0,

	D3DFMT_R8G8B8               = 20,
	D3DFMT_A8R8G8B8             = 21,
	D3DFMT_X8R8G8B8             = 22,
	D3DFMT_R5G6B5               = 23,
	D3DFMT_X1R5G5B5             = 24,
	D3DFMT_A1R5G5B5             = 25,
	D3DFMT_A4R4G4B4             = 26,
	D3DFMT_R3G3B2               = 27,
	D3DFMT_A8                   = 28,
	D3DFMT_A8R3G3B2             = 29,
	D3DFMT_X4R4G4B4             = 30,
	D3DFMT_A2B10G10R10          = 31,
	D3DFMT_A8B8G8R8             = 32,
	D3DFMT_X8B8G8R8             = 33,
	D3DFMT_G16R16               = 34,
	D3DFMT_A2R10G10B10          = 35,
	D3DFMT_A16B16G16R16         = 36,

	D3DFMT_A8P8                 = 40,
	D3DFMT_P8                   = 41,

	D3DFMT_L8                   = 50,
	D3DFMT_A8L8                 = 51,
	D3DFMT_A4L4                 = 52,

	D3DFMT_V8U8                 = 60,
	D3DFMT_L6V5U5               = 61,
	D3DFMT_X8L8V8U8             = 62,
	D3DFMT_Q8W8V8U8             = 63,
	D3DFMT_V16U16               = 64,
	D3DFMT_A2W10V10U10          = 67,

	D3DFMT_UYVY                 = MAKEFOURCC('U', 'Y', 'V', 'Y'),
	D3DFMT_R8G8_B8G8            = MAKEFOURCC('R', 'G', 'B', 'G'),
	D3DFMT_YUY2                 = MAKEFOURCC('Y', 'U', 'Y', '2'),
	D3DFMT_G8R8_G8B8            = MAKEFOURCC('G', 'R', 'G', 'B'),
	D3DFMT_DXT1                 = MAKEFOURCC('D', 'X', 'T', '1'),
	D3DFMT_DXT2                 = MAKEFOURCC('D', 'X', 'T', '2'),
	D3DFMT_DXT3                 = MAKEFOURCC('D', 'X', 'T', '3'),
	D3DFMT_DXT4                 = MAKEFOURCC('D', 'X', 'T', '4'),
	D3DFMT_DXT5                 = MAKEFOURCC('D', 'X', 'T', '5'),

	D3DFMT_D16_LOCKABLE         = 70,
	D3DFMT_D32                  = 71,
	D3DFMT_D15S1                = 73,
	D3DFMT_D24S8                = 75,
	D3DFMT_D24X8                = 77,
	D3DFMT_D24X4S4              = 79,
	D3DFMT_D16                  = 80,

	D3DFMT_D32F_LOCKABLE        = 82,
	D3DFMT_D24FS8               = 83,

	// d3d9ex only
	/* Z-Stencil formats valid for CPU access */
	D3DFMT_D32_LOCKABLE         = 84,
	D3DFMT_S8_LOCKABLE          = 85,

	D3DFMT_L16                  = 81,

	D3DFMT_VERTEXDATA           =100,
	D3DFMT_INDEX16              =101,
	D3DFMT_INDEX32              =102,

	D3DFMT_Q16W16V16U16         =110,

	D3DFMT_MULTI2_ARGB8         = MAKEFOURCC('M','E','T','1'),

	// Floating point surface formats

	// s10e5 formats (16-bits per channel)
	D3DFMT_R16F                 = 111,
	D3DFMT_G16R16F              = 112,
	D3DFMT_A16B16G16R16F        = 113,

	// IEEE s23e8 formats (32-bits per channel)
	D3DFMT_R32F                 = 114,
	D3DFMT_G32R32F              = 115,
	D3DFMT_A32B32G32R32F        = 116,

	D3DFMT_CxV8U8               = 117,

	// d3d9ex only
	// Monochrome 1 bit per pixel format
	D3DFMT_A1                   = 118,
	// 2.8 biased fixed point
	D3DFMT_A2B10G10R10_XR_BIAS  = 119,
	// Binary format indicating that the data has no inherent type
	D3DFMT_BINARYBUFFER         = 199
};
#endif

// stolen from d3d8to9
static uint32
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

int vertFormatMap[] = {
	-1, VERT_FLOAT2, VERT_FLOAT3, VERT_FLOAT4, VERT_ARGB, VERT_RGBA /* blend indices */
};

void*
createIndexBuffer(uint32 length, bool dynamic)
{
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf;
	if(dynamic)
		d3ddevice->CreateIndexBuffer(length, D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ibuf, 0);
	else
		d3ddevice->CreateIndexBuffer(length, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, 0);
	if(ibuf)
		d3d9Globals.numIndexBuffers++;
	return ibuf;
#else
	return rwNewT(uint8, length, MEMDUR_EVENT | ID_DRIVER);
#endif
}

void
destroyIndexBuffer(void *indexBuffer)
{
#ifdef RW_D3D9
	if(indexBuffer){
		if(((IUnknown*)indexBuffer)->Release() != 0)
			printf("indexBuffer wasn't destroyed\n");
		d3d9Globals.numIndexBuffers--;
	}
#else
	rwFree(indexBuffer);
#endif
}

uint16*
lockIndices(void *indexBuffer, uint32 offset, uint32 size, uint32 flags)
{
	if(indexBuffer == nil)
		return nil;
#ifdef RW_D3D9
	uint16 *indices;
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Lock(offset, size, (void**)&indices, flags);
	return indices;
#else
	(void)offset;
	(void)size;
	(void)flags;
	return (uint16*)indexBuffer;
#endif
}

void
unlockIndices(void *indexBuffer)
{
	if(indexBuffer == nil)
		return;
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Unlock();
#endif
}

void*
createVertexBuffer(uint32 length, uint32 fvf, bool dynamic)
{
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vbuf;
	if(dynamic)
		d3ddevice->CreateVertexBuffer(length, D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC, fvf, D3DPOOL_DEFAULT, &vbuf, 0);
	else
		d3ddevice->CreateVertexBuffer(length, D3DUSAGE_WRITEONLY, fvf, D3DPOOL_MANAGED, &vbuf, 0);
	if(vbuf)
		d3d9Globals.numVertexBuffers++;
	return vbuf;
#else
	(void)fvf;
	return rwNewT(uint8, length, MEMDUR_EVENT | ID_DRIVER);
#endif
}

void
destroyVertexBuffer(void *vertexBuffer)
{
#ifdef RW_D3D9
	if(vertexBuffer){
		if(((IUnknown*)vertexBuffer)->Release() != 0)
			printf("vertexBuffer wasn't destroyed\n");
		d3d9Globals.numVertexBuffers--;
	}
#else
	rwFree(vertexBuffer);
#endif
}

uint8*
lockVertices(void *vertexBuffer, uint32 offset, uint32 size, uint32 flags)
{
	if(vertexBuffer == nil)
		return nil;
#ifdef RW_D3D9
	uint8 *verts;
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Lock(offset, size, (void**)&verts, flags);
	return verts;
#else
	(void)offset;
	(void)size;
	(void)flags;
	return (uint8*)vertexBuffer;
#endif
}

void
unlockVertices(void *vertexBuffer)
{
	if(vertexBuffer == nil)
		return;
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Unlock();
#endif
}

void*
createTexture(int32 width, int32 height, int32 numlevels, uint32 format)
{
#ifdef RW_D3D9
	IDirect3DTexture9 *tex;
	d3ddevice->CreateTexture(width, height, numlevels, 0,
	                      (D3DFORMAT)format, D3DPOOL_MANAGED, &tex, nil);
	if(tex)
		d3d9Globals.numTextures++;
	return tex;
#else
	int32 w = width;
	int32 h = height;
	int32 size = 0;
	for(int32 i = 0; i < numlevels; i++){
		size += calculateTextureSize(w, h, 1, format);
		w /= 2;
		if(w == 0) w = 1;
		h /= 2;
		if(h == 0) h = 1;
	}
	uint8 *data = (uint8*)rwNew(sizeof(RasterLevels)+sizeof(RasterLevels::Level)*(numlevels-1)+size,
		MEMDUR_EVENT | ID_DRIVER);
	RasterLevels *levels = (RasterLevels*)data;
	data += sizeof(RasterLevels)+sizeof(RasterLevels::Level)*(numlevels-1);
	levels->numlevels = numlevels;
	levels->format = format;
	w = width;
	h = height;
	for(int32 i = 0; i < numlevels; i++){
		levels->levels[i].width = w;
		levels->levels[i].height = h;
		levels->levels[i].data = data;
		levels->levels[i].size = calculateTextureSize(w, h, 1, format);
		data += levels->levels[i].size;
		w /= 2;
		if(w == 0) w = 1;
		h /= 2;
		if(h == 0) h = 1;
	}
	return levels;
#endif
}

void
destroyTexture(void *texture)
{
#ifdef RW_D3D9
	if(texture){
		if(((IUnknown*)texture)->Release() != 0)
			printf("texture wasn't destroyed\n");
		d3d9Globals.numTextures--;
	}
#else
	rwFree(texture);
#endif
}

uint8*
lockTexture(void *texture, int32 level, int32 lockMode)
{
	// TODO: don't ignore this
	(void)lockMode;
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)texture;
	D3DLOCKED_RECT lr;
	tex->LockRect(level, &lr, 0, D3DLOCK_NOSYSLOCK);
	return (uint8*)lr.pBits;
#else
	RasterLevels *levels = (RasterLevels*)texture;
	return levels->levels[level].data;
#endif
}

void
unlockTexture(void *texture, int32 level)
{
	(void)texture;
	(void)level;
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)texture;
	tex->UnlockRect(level);
#endif
}

// Native Raster

int32 nativeRasterOffset;

struct RasterFormatInfo
{
	uint32 d3dformat;
	int32 depth;
	bool32 hasAlpha;
	uint32 rwFormat;
};

// indexed directly by RW format
static RasterFormatInfo formatInfoRW[16] = {
	{ 0, 0, 0},
	{ D3DFMT_A1R5G5B5, 16, 1, Raster::C1555 },
	{ D3DFMT_R5G6B5,   16, 0, Raster::C565 },
	{ D3DFMT_A4R4G4B4, 16, 1, Raster::C4444 },
	{ D3DFMT_L8,        8, 0, Raster::LUM8 },
	{ D3DFMT_A8R8G8B8, 32, 1, Raster::C8888 },
	{ D3DFMT_X8R8G8B8, 32, 0, Raster::C888 },
	{ D3DFMT_D16,      16, 0, Raster::D16 },
	{ D3DFMT_D24X8,    32, 0, Raster::D24 },
	{ D3DFMT_D32,      32, 0, Raster::D32 },
	{ D3DFMT_X1R5G5B5, 16, 0, Raster::C555 },
};

static RasterFormatInfo formatInfoFull[] = {
	{ D3DFMT_R8G8B8, 0, 24, 0 },
	{ D3DFMT_A8R8G8B8, 1, 32, Raster::C8888 },
	{ D3DFMT_X8R8G8B8, 0, 32, Raster::C888 },
	{ D3DFMT_R5G6B5, 0, 16, Raster::C565 },
	{ D3DFMT_X1R5G5B5, 0, 16, Raster::C555 },
	{ D3DFMT_A1R5G5B5, 1, 16, Raster::C1555 },
	{ D3DFMT_A4R4G4B4, 1, 16, Raster::C4444 },
	{ D3DFMT_R3G3B2, 0, 8, 0 },
	{ D3DFMT_A8, 1, 8, 0 },
	{ D3DFMT_A8R3G3B2, 1, 16, 0 },
	{ D3DFMT_X4R4G4B4, 0, 16, 0 },
	{ D3DFMT_A2B10G10R10, 1, 32, 0 },
	{ D3DFMT_A8B8G8R8, 1, 32, 0 },
	{ D3DFMT_X8B8G8R8, 0, 32, 0 },
	{ D3DFMT_G16R16, 0, 32, 0 },
	{ D3DFMT_A2R10G10B10, 1, 32, 0 },
	{ D3DFMT_A16B16G16R16, 1, 64, 0 },
	{ D3DFMT_A8P8, 1, 16, 0 },
//	{ D3DFMT_P8, 0, 8, ... },
//	{ D3DFMT_L8, 0, 8, ... },
	{ D3DFMT_A8L8, 1, 16, 0 },
	{ D3DFMT_A4L4, 1, 8, 0 },
	{ D3DFMT_V8U8, 0, 16, 0 },
	{ D3DFMT_L6V5U5, 0, 16, 0 },
	{ D3DFMT_X8L8V8U8, 0, 32, 0 },
	{ D3DFMT_Q8W8V8U8, 0, 32, 0 },
	{ D3DFMT_V16U16, 0, 32, 0 },
	{ D3DFMT_A2W10V10U10, 1, 32, 0 },
	{ D3DFMT_D16_LOCKABLE, 0, 16, Raster::D16 },
	{ D3DFMT_D32, 0, 32, Raster::D32 },
	{ D3DFMT_D15S1, 0, 16, Raster::D16 },
	{ D3DFMT_D24S8, 0, 32, Raster::D32 },
	{ D3DFMT_D24X8, 0, 32, Raster::D32 },
	{ D3DFMT_D24X4S4, 0, 32, Raster::D32 },
	{ D3DFMT_D16, 0, 16, Raster::D16 },
	{ D3DFMT_D32F_LOCKABLE, 0, 32, Raster::D32 },
	{ D3DFMT_D24FS8, 0, 32, Raster::D32 },
	{ D3DFMT_L16, 0, 16, 0 },
	{ D3DFMT_Q16W16V16U16, 0, 64, 0 },
	{ D3DFMT_R16F, 0, 16, 0 },
	{ D3DFMT_G16R16F, 0, 32, 0 },
	{ D3DFMT_A16B16G16R16F, 1, 64, 0 },
	{ D3DFMT_R32F, 0, 32, 0 },
	{ D3DFMT_G32R32F, 0, 64, 0 },
	{ D3DFMT_A32B32G32R32F, 1, 128, 0 },
	{ D3DFMT_CxV8U8, 0, 16, 0 },
};

RasterFormatInfo*
findFormatInfoD3D(uint32 d3dformat)
{
	static RasterFormatInfo fake = { 0, 0, 0, 0 };
	int i;
	for(i = 0; i < nelem(formatInfoFull); i++)
		if(formatInfoFull[i].d3dformat == d3dformat)
			return &formatInfoFull[i];
	return &fake;
}


static void
rasterSetFormat(Raster *raster)
{
	if(raster->format == 0){
		// have to find a format first
		// this could perhaps be a bit more intelligent

		switch(raster->type){
		case Raster::NORMAL:
		case Raster::TEXTURE:
			raster->format = Raster::C8888;
			break;

#ifdef RW_D3D9
		case Raster::ZBUFFER:
			raster->format = findFormatInfoD3D(d3d9Globals.present.AutoDepthStencilFormat)->rwFormat;
			// can this even happen? just do something...
			if(raster->format == 0)
				raster->format = Raster::D32;
			break;

		case Raster::CAMERATEXTURE:
// let's not use this because we apparently don't want alpha
//			raster->format = findFormatInfoD3D(d3d9Globals.present.BackBufferFormat)->rwFormat;
			raster->format = findFormatInfoD3D(d3d9Globals.startMode.mode.Format)->rwFormat;
			// can this even happen? just do something...
			if(raster->format == 0)
				raster->format = Raster::C888;
			break;
		case Raster::CAMERA:
			raster->format = findFormatInfoD3D(d3d9Globals.present.BackBufferFormat)->rwFormat;
			// can this even happen? just do something...
			if(raster->format == 0)
				raster->format = Raster::C8888;
			break;
#endif
		}
	}


	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		// TODO: do we even allow PAL4?
		natras->format = D3DFMT_P8;
		raster->depth = 8;
	}else{
		natras->format = formatInfoRW[(raster->format >> 8) & 0xF].d3dformat;
		raster->depth = formatInfoRW[(raster->format >> 8) & 0xF].depth;
	}
	natras->bpp = raster->depth/8;
	natras->hasAlpha = formatInfoRW[(raster->format >> 8) & 0xF].hasAlpha;
	raster->stride = raster->width*natras->bpp;
}

static Raster*
rasterCreateTexture(Raster *raster)
{
	int32 levels;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);

	if(natras->format == D3DFMT_P8)
		natras->palette = (uint8*)rwNew(4*256, MEMDUR_EVENT | ID_DRIVER);
	levels = Raster::calculateNumLevels(raster->width, raster->height);
	assert(natras->texture == nil);
	natras->texture = createTexture(raster->width, raster->height,
	                                raster->format & Raster::MIPMAP ? levels : 1,
	                                natras->format);
	if(natras->texture == nil){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	return raster;
}

#ifdef RW_D3D9

static Raster*
rasterCreateCameraTexture(Raster *raster)
{
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	int32 levels;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	levels = Raster::calculateNumLevels(raster->width, raster->height);

	IDirect3DTexture9 *tex;
	d3ddevice->CreateTexture(raster->width, raster->height,
				raster->format & Raster::MIPMAP ? levels : 1,
				D3DUSAGE_RENDERTARGET,
				(D3DFORMAT)natras->format, D3DPOOL_DEFAULT, &tex, nil);
	assert(natras->texture == nil);
	natras->texture = tex;
	if(natras->texture == nil){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}
	d3d9Globals.numTextures++;
	addVidmemRaster(raster);
	return raster;
}

static Raster*
rasterCreateCamera(Raster *raster)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	raster->flags |= Raster::DONTALLOCATE;
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;

	natras->format = d3d9Globals.present.BackBufferFormat;
	raster->depth = findFormatDepth(natras->format);
	return raster;
}

static Raster*
rasterCreateZbuffer(Raster *raster)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	raster->flags |= Raster::DONTALLOCATE;
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;

	natras->format = d3d9Globals.present.AutoDepthStencilFormat;
	raster->depth = findFormatDepth(natras->format);
	return raster;
}
#endif

Raster*
rasterCreate(Raster *raster)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);

	rasterSetFormat(raster);

	switch(raster->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
		// Dummy to use as subraster
		// ^ what did i do there?
		if(raster->width == 0 || raster->height == 0){
			raster->flags |= Raster::DONTALLOCATE;
			raster->stride = 0;
			return raster;
		}

		if(raster->flags & Raster::DONTALLOCATE)
			return raster;
		return rasterCreateTexture(raster);

#ifdef RW_D3D9
	case Raster::CAMERATEXTURE:
		if(raster->flags & Raster::DONTALLOCATE)
			return raster;
		return rasterCreateCameraTexture(raster);

	case Raster::ZBUFFER:
		return rasterCreateZbuffer(raster);
	case Raster::CAMERA:
		return rasterCreateCamera(raster);
#endif
	}
	return nil;
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	return lockTexture(natras->texture, level, lockMode);
}

void
rasterUnlock(Raster *raster, int32 level)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	unlockTexture(natras->texture, level);
}

int32
rasterNumLevels(Raster *raster)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)natras->texture;
	return tex->GetLevelCount();
#else
	RasterLevels *levels = (RasterLevels*)natras->texture;
	return levels->numlevels;
#endif
}

// Almost the same as ps2 and gl3 function
bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 width, height, depth, format;

	assert((type&0xF) == Raster::TEXTURE);

//	for(width = 1; width < img->width; width <<= 1);
//	for(height = 1; height < img->height; height <<= 1);
	// Perhaps non-power-of-2 textures are acceptable?
	width = img->width;
	height = img->height;

	depth = img->depth;

	if(depth <= 8 && !isP8supported)
		depth = 32;

	switch(depth){
	case 32:
		if(img->hasAlpha())
			format = Raster::C8888;
		else{
			format = Raster::C888;
			depth = 24;
		}
		break;
	case 24:
		format = Raster::C888;
		break;
	case 16:
		format = Raster::C1555;
		break;
	case 8:
		format = Raster::PAL8 | Raster::C8888;
		break;
	case 4:
		format = Raster::PAL4 | Raster::C8888;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	format |= type;

	*pWidth = width;
	*pHeight = height;
	*pDepth = depth;
	*pFormat = format;

	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type&0xF) != Raster::TEXTURE)
		return 0;

	void (*conv)(uint8 *out, uint8 *in) = nil;

	// Unpalettize image if necessary but don't change original
	Image *truecolimg = nil;
	if(image->depth <= 8 && !isP8supported){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unpalettize();
		image = truecolimg;
	}

	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	switch(image->depth){
	case 32:
		if(raster->format == Raster::C8888)
			conv = conv_BGRA8888_from_RGBA8888;
		else if(raster->format == Raster::C888)
			conv = conv_BGR888_from_RGB888;
		else
			goto err;
		break;
	case 24:
		if(raster->format == Raster::C8888)
			conv = conv_BGRA8888_from_RGB888;
		else if(raster->format == Raster::C888)
			conv = conv_BGR888_from_RGB888;
		else
			goto err;
		break;
	case 16:
		if(raster->format == Raster::C1555)
			conv = conv_ARGB1555_from_ARGB1555;
		else
			goto err;
		break;
	case 8:
		if(raster->format == (Raster::PAL8 | Raster::C8888))
			conv = conv_8_from_8;
		else
			goto err;
		break;
	case 4:
		if(raster->format == (Raster::PAL4 | Raster::C8888) ||
		   raster->format == (Raster::PAL8 | Raster::C8888))
			conv = conv_8_from_8;
		else
			goto err;
	default:
	err:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	uint8 *in, *out;
	int pallength = 0;
	if(raster->format & Raster::PAL4)
		pallength = 16;
	else if(raster->format & Raster::PAL8)
		pallength = 256;
	if(pallength){
		in = image->palette;
		out = (uint8*)natras->palette;
		for(int32 i = 0; i < pallength; i++){
			conv_RGBA8888_from_RGBA8888(out, in);
			in += 4;
			out += 4;
		}
	}

	uint8 *pixels = raster->lock(0, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
	assert(pixels);
	uint8 *imgpixels = image->pixels;

	int x, y;
	assert(image->width == raster->width);
	assert(image->height == raster->height);
	for(y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(x = 0; x < image->width; x++){
			conv(rasrow, imgrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels += image->stride;
		pixels += raster->stride;
	}
	raster->unlock(0);

	if(truecolimg)
		truecolimg->destroy();

	return 1;
}

Image*
rasterToImage(Raster *raster)
{
	int32 depth;
	Image *image;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	if(natras->customFormat){
		image = Image::create(raster->width, raster->height, 32);
		image->allocate();
		uint8 *pix = raster->lock(0, Raster::LOCKREAD);
		switch(natras->format){
		case D3DFMT_DXT1:
			image->setPixelsDXT(1, pix);
			if((raster->format & 0xF00) == Raster::C565)
				image->removeMask();
			break;
		case D3DFMT_DXT3:
			image->setPixelsDXT(3, pix);
			break;
		case D3DFMT_DXT5:
			image->setPixelsDXT(5, pix);
			break;
		default:
			raster->unlock(0);
			image->destroy();
			return nil;
		}
		raster->unlock(0);
		return image;
	}

	void (*conv)(uint8 *out, uint8 *in) = nil;
	switch(raster->format & 0xF00){
	case Raster::C1555:
		depth = 16;
		conv = conv_ARGB1555_from_ARGB1555;
		break;
	case Raster::C8888:
		depth = 32;
		conv = conv_RGBA8888_from_BGRA8888;
		break;
	case Raster::C888:
		depth = 24;
		conv = conv_RGB888_from_BGR888;
		break;
	case Raster::C555:
		depth = 16;
		conv = conv_ARGB1555_from_RGB555;
		break;

	default:
	case Raster::C565:
	case Raster::C4444:
	case Raster::LUM8:
		RWERROR((ERR_INVRASTER));
		return nil;
	}
	int32 pallength = 0;
	if((raster->format & Raster::PAL4) == Raster::PAL4){
		depth = 4;
		pallength = 16;
	}else if((raster->format & Raster::PAL8) == Raster::PAL8){
		depth = 8;
		pallength = 256;
	}

	uint8 *in, *out;
	image = Image::create(raster->width, raster->height, depth);
	image->allocate();

	if(pallength){
		out = image->palette;
		in = (uint8*)natras->palette;
		for(int32 i = 0; i < pallength; i++){
			conv_RGBA8888_from_RGBA8888(out, in);
			in += 4;
			out += 4;
		}
	}

	uint8 *imgpixels = image->pixels;
	uint8 *pixels = raster->lock(0, Raster::LOCKREAD);

	int x, y;
	assert(image->width == raster->width);
	assert(image->height == raster->height);
	for(y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(x = 0; x < image->width; x++){
			conv(imgrow, rasrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels += image->stride;
		pixels += raster->stride;
	}
	raster->unlock(0);

	image->compressPalette();

	return image;
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
	RasterLevels *levels = (RasterLevels*)ras->texture;
	return levels->levels[level].size;
#endif
}

void
allocateDXT(Raster *raster, int32 dxt, int32 numLevels, bool32 hasAlpha)
{
	static uint32 dxtMap[] = {
		0x31545844,	// DXT1
		0x32545844,	// DXT2
		0x33545844,	// DXT3
		0x34545844,	// DXT4
		0x35545844,	// DXT5
	};
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	ras->format = dxtMap[dxt-1];
	ras->hasAlpha = hasAlpha;
	ras->texture = createTexture(raster->width, raster->height,
	                             raster->format & Raster::MIPMAP ? numLevels : 1,
	                             ras->format);
	raster->flags &= ~Raster::DONTALLOCATE;
}

void
setPalette(Raster *raster, void *palette, int32 size)
{
	D3dRaster *ras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	memcpy(ras->palette, palette, 4*size);
}

void
setTexels(Raster *raster, void *texels, int32 level)
{
	uint8 *dst = raster->lock(level, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
	memcpy(dst, texels, getLevelSize(raster, level));
	raster->unlock(level);
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	D3dRaster *raster = PLUGINOFFSET(D3dRaster, object, offset);
	raster->texture = nil;
	raster->palette = nil;
	raster->format = 0;
	raster->hasAlpha = 0;
	raster->customFormat = 0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	Raster *raster = (Raster*)object;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, offset);
#ifdef RW_D3D9
	destroyD3D9Raster(raster);
#endif
	destroyTexture(natras->texture);
	rwFree(natras->palette);
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	D3dRaster *raster = PLUGINOFFSET(D3dRaster, dst, offset);
	raster->texture = nil;
	raster->palette = nil;
	raster->format = 0;
	raster->hasAlpha = 0;
	raster->customFormat = 0;
	return dst;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(D3dRaster),
	                                            ID_RASTERD3D9,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

}
}
