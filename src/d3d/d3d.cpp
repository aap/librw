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

namespace rw {
namespace d3d {

bool32 isP8supported = 1;

#ifdef RW_D3D9
IDirect3DDevice9 *device = nil;
#else
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
	D3DFMT_BINARYBUFFER         = 199,
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
createVertexBuffer(uint32 length, uint32 fvf, int32 pool)
{
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vbuf;
	device->CreateVertexBuffer(length, D3DUSAGE_WRITEONLY, fvf, (D3DPOOL)pool, &vbuf, 0);
	return vbuf;
#else
	(void)fvf;
	(void)pool;
	return new uint8[length];
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
	device->CreateTexture(width, height, numlevels, 0,
	                      (D3DFORMAT)format, D3DPOOL_MANAGED, &tex, nil);
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
	uint8 *data = new uint8[sizeof(RasterLevels)+sizeof(RasterLevels::Level)*(numlevels-1)+size];
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

uint8*
lockTexture(void *texture, int32 level)
{
#ifdef RW_D3D9
	IDirect3DTexture9 *tex = (IDirect3DTexture9*)texture;
	D3DLOCKED_RECT lr;
	tex->LockRect(level, &lr, 0, 0);
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

void
deleteObject(void *object)
{
	if(object == nil)
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
rasterCreate(Raster *raster)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
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
	if(raster->flags & 0x80)
		return;
	uint32 format;
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		format = D3DFMT_P8;
		natras->palette = new uint8[4*256];
	}else
		format = formatMap[(raster->format >> 8) & 0xF];
	natras->format = 0;
	natras->hasAlpha = alphaMap[(raster->format >> 8) & 0xF];
	int32 levels = Raster::calculateNumLevels(raster->width, raster->height);
	natras->texture = createTexture(raster->width, raster->height,
	                                raster->format & Raster::MIPMAP ? levels : 1,
	                                format);
}

uint8*
rasterLock(Raster *raster, int32 level)
{
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	return lockTexture(natras->texture, level);
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

void
rasterFromImage(Raster *raster, Image *image)
{
	int32 format;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	switch(image->depth){
	case 32:
		format = image->hasAlpha() ? Raster::C8888 : Raster::C888;
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
		return;
	}
	format |= Raster::TEXTURE;

	raster->type = format & 0x7;
	raster->flags = format & 0xF8;
	raster->format = format & 0xFF00;
	rasterCreate(raster);

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
			out[0] = in[0];
			out[1] = in[1];
			out[2] = in[2];
			out[3] = in[3];
			in += 4;
			out += 4;
		}
	}

	int32 inc = image->depth/8;
	in = image->pixels;
	out = raster->lock(0);
	if(pallength)
		memcpy(out, in, raster->width*raster->height);
	else
		// TODO: stride
		for(int32 y = 0; y < image->height; y++)
			for(int32 x = 0; x < image->width; x++)
				switch(raster->format & 0xF00){
				case Raster::C8888:
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					out[3] = in[3];
					in += inc;
					out += 4;
					break;
				case Raster::C888:
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					out[3] = 0xFF;
					in += inc;
					out += 4;
					break;
				case Raster::C1555:
					out[0] = in[0];
					out[1] = in[1];
					in += 2;
					out += 2;
					break;
				}
	raster->unlock(0);
}

Image*
rasterToImage(Raster *raster)
{
	int32 depth;
	Image *image;
	D3dRaster *natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
	if(natras->format)
		assert(0 && "no custom formats yet");
	switch(raster->format & 0xF00){
	case Raster::C1555:
		depth = 16;
		break;
	case Raster::C8888:
		depth = 32;
		break;
	case Raster::C888:
		depth = 24;
		break;
	case Raster::C555:
		depth = 16;
		break;

	default:
	case Raster::C565:
	case Raster::C4444:
	case Raster::LUM8:
		assert(0 && "unsupported raster format");
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
			out[0] = in[0];
			out[1] = in[1];
			out[2] = in[2];
			out[3] = in[3];
			in += 4;
			out += 4;
		}
	}

	out = image->pixels;
	in = raster->lock(0);
	if(pallength)
		memcpy(out, in, raster->width*raster->height);
	else
		// TODO: stride
		for(int32 y = 0; y < image->height; y++)
			for(int32 x = 0; x < image->width; x++)
				switch(raster->format & 0xF00){
				case Raster::C8888:
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					out[3] = in[3];
					in += 4;
					out += 4;
					break;
				case Raster::C888:
					out[0] = in[2];
					out[1] = in[1];
					out[2] = in[0];
					in += 4;
					out += 3;
					break;
				case Raster::C1555:
					out[0] = in[0];
					out[1] = in[1];
					in += 2;
					out += 2;
					break;
				case Raster::C555:
					out[0] = in[0];
					out[1] = in[1] | 0x80;
					in += 2;
					out += 2;
					break;
				}
	raster->unlock(0);

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
	raster->flags &= ~0x80;
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
	uint8 *dst = raster->lock(level);
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
	// TODO:
	(void)offset;
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
