#ifdef RW_VULKAN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"

#include "rwvulkan.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace vulkan {

int32 nativeRasterOffset;

static void
getLevelDimensions(Raster *raster, int32 level, int32 *width, int32 *height)
{
	int32 w = raster->originalWidth ? raster->originalWidth : raster->width;
	int32 h = raster->originalHeight ? raster->originalHeight : raster->height;

	while(level-- > 0){
		if(w > 1)
			w /= 2;
		if(h > 1)
			h /= 2;
	}

	*width = w;
	*height = h;
}

static uint32
getLevelSize(Raster *raster, int32 level)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	int32 w, h;
	getLevelDimensions(raster, level, &w, &h);
	return w*h*natras->bpp;
}

static void
freeLevels(VulkanRaster *natras)
{
	if(natras->levels){
		rwFree(natras->levels);
		natras->levels = nil;
	}
}

static bool32
allocateLevels(Raster *raster, int32 numLevels)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	uint32 size = 0;
	int32 i;

	if(numLevels < 1)
		numLevels = 1;

	freeLevels(natras);
	natras->numLevels = numLevels;

	for(i = 0; i < numLevels; i++)
		size += getLevelSize(raster, i);

	uint8 *data = (uint8*)rwNew(sizeof(RasterLevels) + sizeof(RasterLevels::Level)*(numLevels-1) + size,
		MEMDUR_EVENT | ID_DRIVER);
	natras->levels = (RasterLevels*)data;
	data += sizeof(RasterLevels) + sizeof(RasterLevels::Level)*(numLevels-1);
	natras->levels->numlevels = numLevels;
	natras->levels->format = raster->format;

	for(i = 0; i < numLevels; i++){
		int32 w, h;
		getLevelDimensions(raster, i, &w, &h);
		natras->levels->levels[i].width = w;
		natras->levels->levels[i].height = h;
		natras->levels->levels[i].size = getLevelSize(raster, i);
		natras->levels->levels[i].data = data;
		memset(data, 0, natras->levels->levels[i].size);
		data += natras->levels->levels[i].size;
	}
	raster->flags &= ~Raster::DONTALLOCATE;
	return 1;
}

static bool32
setRasterFormat(Raster *raster)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);

	switch(raster->format & 0xF00){
	case Raster::DEFAULT:
	case Raster::C8888:
		raster->format = (raster->format & ~0xF00) | Raster::C8888;
		raster->depth = 32;
		natras->bpp = 4;
		natras->hasAlpha = 1;
		break;
	case Raster::C888:
		raster->depth = 24;
		natras->bpp = 3;
		natras->hasAlpha = 0;
		break;
	case Raster::C1555:
		raster->depth = 16;
		natras->bpp = 2;
		natras->hasAlpha = 1;
		break;
	case Raster::C565:
	case Raster::C555:
		raster->depth = 16;
		natras->bpp = 2;
		natras->hasAlpha = 0;
		break;
	case Raster::C4444:
		raster->depth = 16;
		natras->bpp = 2;
		natras->hasAlpha = 1;
		break;
	default:
		RWERROR((ERR_INVRASTER, ""));
		return 0;
	}

	raster->stride = raster->width*natras->bpp;
	return 1;
}

Raster*
rasterCreate(Raster *raster)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);

	natras->bpp = 0;
	natras->hasAlpha = 0;
	natras->numLevels = 1;
	natras->levels = nil;
	natras->image = VK_NULL_HANDLE;
	natras->imageMemory = VK_NULL_HANDLE;
	natras->imageView = VK_NULL_HANDLE;
	natras->sampler = VK_NULL_HANDLE;
	natras->descriptorSet = VK_NULL_HANDLE;
	natras->imageFormat = VK_FORMAT_UNDEFINED;
	natras->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	natras->gpuDirty = 1;
	natras->gpuReady = 0;
	raster->originalWidth = 0;
	raster->originalHeight = 0;
	raster->originalStride = 0;
	raster->originalPixels = nil;

	if(raster->width == 0 || raster->height == 0){
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		goto ret;
	}

	switch(raster->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		if(raster->format & (Raster::PAL4 | Raster::PAL8)){
			RWERROR((ERR_NOTEXTURE, ""));
			return nil;
		}
		if(!setRasterFormat(raster))
			return nil;
		if(raster->format & Raster::MIPMAP)
			natras->numLevels = Raster::calculateNumLevels(raster->width, raster->height);
		if((raster->flags & Raster::DONTALLOCATE) == 0)
			allocateLevels(raster, natras->numLevels);
		break;
	case Raster::CAMERA:
		raster->format = Raster::C8888;
		raster->depth = 32;
		natras->bpp = 4;
		natras->hasAlpha = 1;
		raster->stride = raster->width*natras->bpp;
		break;
	case Raster::ZBUFFER:
		raster->format = Raster::D24;
		raster->depth = 24;
		natras->bpp = 4;
		raster->stride = raster->width*natras->bpp;
		break;
	default:
		RWERROR((ERR_INVRASTER, ""));
		return nil;
	}

ret:
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->originalStride = raster->stride;
	raster->originalPixels = raster->pixels;
	return raster;
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	uint32 allocSz;
	uint8 *px;

	assert(raster->privateFlags == 0);

	if(raster->type == Raster::CAMERA){
		allocSz = raster->width*raster->height*4;
		raster->pixels = (uint8*)rwMalloc(allocSz, MEMDUR_EVENT | ID_DRIVER);
		if(raster->pixels)
			memset(raster->pixels, 0, allocSz);
		raster->privateFlags = lockMode;
		return raster->pixels;
	}

	if(raster->type != Raster::NORMAL &&
	   raster->type != Raster::TEXTURE &&
	   raster->type != Raster::CAMERATEXTURE){
		RWERROR((ERR_INVRASTER, ""));
		return nil;
	}

	if(natras->levels == nil)
		allocateLevels(raster, natras->numLevels);

	if(level < 0 || level >= natras->levels->numlevels)
		return nil;

	raster->width = natras->levels->levels[level].width;
	raster->height = natras->levels->levels[level].height;
	raster->stride = raster->width*natras->bpp;
	allocSz = natras->levels->levels[level].size;
	px = (uint8*)rwMalloc(allocSz, MEMDUR_EVENT | ID_DRIVER);
	if(px == nil)
		return nil;

	if((lockMode & Raster::LOCKREAD) || (lockMode & Raster::LOCKNOFETCH) == 0)
		memcpy(px, natras->levels->levels[level].data, allocSz);
	else
		memset(px, 0, allocSz);

	raster->pixels = px;
	raster->privateFlags = lockMode;
	return px;
}

void
rasterUnlock(Raster *raster, int32 level)
{
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);

	if(raster->pixels && natras->levels && level >= 0 && level < natras->levels->numlevels){
		if(raster->privateFlags & Raster::LOCKWRITE){
			memcpy(natras->levels->levels[level].data,
			       raster->pixels,
			       natras->levels->levels[level].size);
			natras->gpuDirty = 1;
			natras->gpuReady = 0;
		}
	}

	rwFree(raster->pixels);
	raster->width = raster->originalWidth;
	raster->height = raster->originalHeight;
	raster->stride = raster->originalStride;
	raster->pixels = raster->originalPixels;
	raster->privateFlags = 0;
	if(natras->gpuDirty)
		ensureTextureUploaded(raster);
}

int32
rasterNumLevels(Raster *raster)
{
	return GETVULKANRASTEREXT(raster)->numLevels;
}

bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 depth = img->depth;
	int32 format;

	assert((type&0xF) == Raster::TEXTURE);

	if(depth <= 8)
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
	default:
		RWERROR((ERR_INVRASTER, ""));
		return 0;
	}

	*pWidth = img->width;
	*pHeight = img->height;
	*pDepth = depth;
	*pFormat = format | type;
	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type&0xF) != Raster::TEXTURE)
		return 0;

	void (*conv)(uint8 *out, uint8 *in) = nil;
	Image *truecolimg = nil;
	if(image->depth <= 8){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unpalettize();
		image = truecolimg;
	}

	switch(image->depth){
	case 32:
		if((raster->format & 0xF00) == Raster::C8888)
			conv = conv_RGBA8888_from_RGBA8888;
		else if((raster->format & 0xF00) == Raster::C888)
			conv = conv_RGB888_from_RGB888;
		break;
	case 24:
		if((raster->format & 0xF00) == Raster::C8888)
			conv = conv_RGBA8888_from_RGB888;
		else if((raster->format & 0xF00) == Raster::C888)
			conv = conv_RGB888_from_RGB888;
		break;
	case 16:
		if((raster->format & 0xF00) == Raster::C1555)
			conv = conv_RGBA5551_from_ARGB1555;
		break;
	}
	if(conv == nil){
		if(truecolimg)
			truecolimg->destroy();
		RWERROR((ERR_INVRASTER, ""));
		return 0;
	}

	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	natras->hasAlpha = image->hasAlpha();

	bool unlock = false;
	if(raster->pixels == nil){
		raster->lock(0, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
		unlock = true;
	}

	uint8 *pixels = raster->pixels;
	uint8 *imgpixels = image->pixels;
	for(int y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(int x = 0; x < image->width; x++){
			conv(rasrow, imgrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels += image->stride;
		pixels += raster->stride;
	}

	if(unlock)
		raster->unlock(0);
	else{
		natras->gpuDirty = 1;
		natras->gpuReady = 0;
		ensureTextureUploaded(raster);
	}
	if(truecolimg)
		truecolimg->destroy();
	return 1;
}

Image*
rasterToImage(Raster *raster)
{
	int32 depth;
	void (*conv)(uint8 *out, uint8 *in) = nil;

	bool unlock = false;
	if(raster->pixels == nil){
		raster->lock(0, Raster::LOCKREAD);
		unlock = true;
	}

	switch(raster->format & 0xF00){
	case Raster::C1555:
		depth = 16;
		conv = conv_ARGB1555_from_RGBA5551;
		break;
	case Raster::C8888:
		depth = 32;
		conv = conv_RGBA8888_from_RGBA8888;
		break;
	case Raster::C888:
		depth = 24;
		conv = conv_RGB888_from_RGB888;
		break;
	default:
		RWERROR((ERR_INVRASTER, ""));
		return nil;
	}

	Image *image = Image::create(raster->width, raster->height, depth);
	image->allocate();
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	uint8 *imgpixels = image->pixels;
	uint8 *pixels = raster->pixels;
	for(int y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(int x = 0; x < image->width; x++){
			conv(imgrow, rasrow);
			imgrow += image->bpp;
			rasrow += natras->bpp;
		}
		imgpixels += image->stride;
		pixels += raster->stride;
	}

	if(unlock)
		raster->unlock(0);
	return image;
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	VulkanRaster *ras = PLUGINOFFSET(VulkanRaster, object, offset);
	ras->bpp = 0;
	ras->hasAlpha = 0;
	ras->numLevels = 1;
	ras->levels = nil;
	ras->image = VK_NULL_HANDLE;
	ras->imageMemory = VK_NULL_HANDLE;
	ras->imageView = VK_NULL_HANDLE;
	ras->sampler = VK_NULL_HANDLE;
	ras->descriptorSet = VK_NULL_HANDLE;
	ras->imageFormat = VK_FORMAT_UNDEFINED;
	ras->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ras->gpuDirty = 1;
	ras->gpuReady = 0;
	ras->uploadSubmitted = 0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	VulkanRaster *ras = PLUGINOFFSET(VulkanRaster, object, offset);
	destroyRasterTexture((Raster*)object);
	freeLevels(ras);
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	VulkanRaster *d = PLUGINOFFSET(VulkanRaster, dst, offset);
	d->bpp = 0;
	d->hasAlpha = 0;
	d->numLevels = 1;
	d->levels = nil;
	d->image = VK_NULL_HANDLE;
	d->imageMemory = VK_NULL_HANDLE;
	d->imageView = VK_NULL_HANDLE;
	d->sampler = VK_NULL_HANDLE;
	d->descriptorSet = VK_NULL_HANDLE;
	d->imageFormat = VK_FORMAT_UNDEFINED;
	d->imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	d->gpuDirty = 1;
	d->gpuReady = 0;
	d->uploadSubmitted = 0;
	return dst;
}

Texture*
readNativeTexture(Stream *stream)
{
	uint32 platform;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	platform = stream->readU32();
	if(platform != PLATFORM_VULKAN){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}

	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	uint32 format = stream->readU32();
	int32 width = stream->readI32();
	int32 height = stream->readI32();
	int32 depth = stream->readI32();
	int32 numLevels = stream->readI32();
	if(numLevels > 1)
		format |= Raster::MIPMAP;

	Raster *raster = Raster::create(width, height, depth, format | Raster::TEXTURE, PLATFORM_VULKAN);
	if(raster == nil){
		tex->destroy();
		return nil;
	}
	tex->raster = raster;

	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	if(natras->numLevels != numLevels)
		allocateLevels(raster, numLevels);

	for(int32 i = 0; i < numLevels; i++){
		uint32 size = stream->readU32();
		if(size != (uint32)natras->levels->levels[i].size){
			tex->destroy();
			RWERROR((ERR_INVRASTER, ""));
			return nil;
		}
		stream->read8(natras->levels->levels[i].data, size);
	}
	natras->gpuDirty = 1;
	natras->gpuReady = 0;
	ensureTextureUploaded(raster);
	return tex;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);

	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_STRUCT, chunksize-12);
	stream->writeU32(PLATFORM_VULKAN);
	stream->writeU32(tex->filterAddressing);
	stream->write8(tex->name, 32);
	stream->write8(tex->mask, 32);
	stream->writeI32(raster->format);
	stream->writeI32(raster->width);
	stream->writeI32(raster->height);
	stream->writeI32(raster->depth);
	stream->writeI32(natras->numLevels);

	for(int32 i = 0; i < natras->numLevels; i++){
		uint32 size = natras->levels ? natras->levels->levels[i].size : getLevelSize(raster, i);
		stream->writeU32(size);
		if(natras->levels)
			stream->write8(natras->levels->levels[i].data, size);
	}
}

uint32
getSizeNativeTexture(Texture *tex)
{
	Raster *raster = tex->raster;
	VulkanRaster *natras = GETVULKANRASTEREXT(raster);
	uint32 size = 12 + 4 + 4 + 32 + 32 + 20;
	for(int32 i = 0; i < natras->numLevels; i++)
		size += 4 + (natras->levels ? natras->levels->levels[i].size : getLevelSize(raster, i));
	return size;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(VulkanRaster),
	                                            ID_RASTERVULKAN,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

}
}

#endif
