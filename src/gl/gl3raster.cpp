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
#ifdef RW_OPENGL
#include <GL/glew.h>
#endif
#include "rwgl3.h"
#include "rwgl3shader.h"

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace gl3 {

int32 nativeRasterOffset;

#ifdef RW_OPENGL
static Raster*
rasterCreateTexture(Raster *raster)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(raster->format & 0xF00){
	case Raster::C8888:
		natras->internalFormat = GL_RGBA8;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		natras->bpp = 4;
		raster->depth = 32;
		break;
	case Raster::C888:
		natras->internalFormat = GL_RGB8;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		natras->bpp = 3;
		raster->depth = 24;
		break;
	case Raster::C1555:
		natras->internalFormat = GL_RGB5_A1;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		natras->bpp = 2;
		raster->depth = 16;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

#ifdef RW_GLES
	// glReadPixels only supports GL_RGBA
	natras->internalFormat = GL_RGBA8;
	natras->format = GL_RGBA;
	natras->type = GL_UNSIGNED_BYTE;
	natras->bpp = 4;
#endif

	raster->stride = raster->width*natras->bpp;

	glGenTextures(1, &natras->texid);
	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, nil);
	natras->filterMode = 0;
	natras->addressU = 0;
	natras->addressV = 0;

	glBindTexture(GL_TEXTURE_2D, 0);
	return raster;
}

// This is totally fake right now, can't render to it. Only used to copy into from FB
// For rendering the idea would probably be to render to the backbuffer and copy it here afterwards.
// alternatively just use FBOs but that probably needs some more infrastructure.
static Raster*
rasterCreateCameraTexture(Raster *raster)
{
	if(raster->format & (Raster::PAL4 | Raster::PAL8)){
		RWERROR((ERR_NOTEXTURE));
		return nil;
	}

	// TODO: figure out what the backbuffer is and use that as a default
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(raster->format & 0xF00){
	case Raster::C8888:
		natras->internalFormat = GL_RGBA8;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		natras->bpp = 4;
		break;
	case Raster::C888:
	default:
		natras->internalFormat = GL_RGB8;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		natras->bpp = 3;
		break;
	case Raster::C1555:
		natras->internalFormat = GL_RGB5_A1;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		natras->bpp = 2;
		break;
	}

#ifdef RW_GLES
	// glReadPixels only supports GL_RGBA
//	natras->internalFormat = GL_RGBA8;
//	natras->format = GL_RGBA;
//	natras->type = GL_UNSIGNED_BYTE;
//	natras->bpp = 4;
#endif

	raster->stride = raster->width*natras->bpp;

	glGenTextures(1, &natras->texid);
	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, nil);
	natras->filterMode = 0;
	natras->addressU = 0;
	natras->addressV = 0;

	glBindTexture(GL_TEXTURE_2D, 0);
	return raster;
}

static Raster*
rasterCreateCamera(Raster *raster)
{
	// TODO: set/check width, height, depth, format?
	raster->flags |= Raster::DONTALLOCATE;
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->stride = 0;
	raster->pixels = nil;
	return raster;
}

static Raster*
rasterCreateZbuffer(Raster *raster)
{
	// TODO: set/check width, height, depth, format?
	raster->flags |= Raster::DONTALLOCATE;
	return raster;
}

#endif

/*
{ 0, 0, 0 },
{ 16, 4, GL_RGBA },	// 1555
{ 16, 3, GL_RGB },	// 565
{ 16, 4, GL_RGBA },	// 4444
{ 0, 0, 0 },	// LUM8
{ 32, 4, GL_RGBA },	// 8888
{ 24, 3, GL_RGB },	// 888
{ 16, 3, GL_RGB },	// D16
{ 24, 3, GL_RGB },	// D24
{ 32, 4, GL_RGBA },	// D32
{ 16, 3, GL_RGB },	// 555

0,
GL_RGB5_A1,
GL_RGB5,
GL_RGBA4,
0,
GL_RGBA8,
GL_RGB8,
GL_RGB5,
GL_RGB8,
GL_RGBA8,
GL_RGB5
*/

Raster*
rasterCreate(Raster *raster)
{
	switch(raster->type){
#ifdef RW_OPENGL
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

	case Raster::CAMERATEXTURE:
		if(raster->flags & Raster::DONTALLOCATE)
			return raster;
		return rasterCreateCameraTexture(raster);

	case Raster::ZBUFFER:
		return rasterCreateZbuffer(raster);
	case Raster::CAMERA:
		return rasterCreateCamera(raster);
#endif

	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
#ifdef RW_OPENGL
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	uint8 *px;

	assert(raster->privateFlags == 0);

	switch(raster->type & 0xF00){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		px = (uint8*)rwMalloc(raster->stride*raster->height, MEMDUR_EVENT | ID_DRIVER);
memset(px, 0, raster->stride*raster->height);
		assert(raster->pixels == nil);
		raster->pixels = px;

		if(lockMode & Raster::LOCKREAD || !(lockMode & Raster::LOCKNOFETCH)){
#ifdef RW_GLES
			GLuint fbo;
GLenum e;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, natras->texid, 0);
			e = glCheckFramebufferStatus(GL_FRAMEBUFFER);
assert(natras->format == GL_RGBA);
			glReadPixels(0, 0, raster->width, raster->height, natras->format, natras->type, px);
//e = glGetError(); printf("GL err4 %x (%x)\n", e, natras->format);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &fbo);
#else
			uint32 prev = bindTexture(natras->texid);
			glGetTexImage(GL_TEXTURE_2D, level, natras->format, natras->type, px);
			bindTexture(prev);
#endif
		}

		raster->privateFlags = lockMode;
		break;

	default:
		assert(0 && "cannot lock this type of raster yet");
		return nil;
	}

	return px;
#else
	return nil;
#endif
}

void
rasterUnlock(Raster *raster, int32 level)
{
#ifdef RW_OPENGL
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	assert(raster->pixels);

	if(raster->privateFlags & Raster::LOCKWRITE){
		uint32 prev = bindTexture(natras->texid);
		glTexImage2D(GL_TEXTURE_2D, level, natras->internalFormat,
			     raster->width, raster->height,
			     0, natras->format, natras->type, raster->pixels);
		bindTexture(prev);
	}

	rwFree(raster->pixels);
	raster->pixels = nil;
	raster->privateFlags = 0;
#endif
}

int32
rasterNumLevels(Raster*)
{
	// TODO
	return 1;
}

// Almost the same as d3d9 and ps2 function
bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 width, height, depth, format;

	assert((type&0xF) == Raster::TEXTURE);

	for(width = 1; width < img->width; width <<= 1);
	for(height = 1; height < img->height; height <<= 1);

	depth = img->depth;

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

	case 8:
	case 4:
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
	if(image->depth <= 8){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unindex();
		image = truecolimg;
	}

	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(image->depth){
	case 32:
#ifdef RW_GLES
		conv = conv_RGBA8888_to_RGBA8888;
#else
		if(raster->format == Raster::C8888)
			conv = conv_RGBA8888_to_RGBA8888;
		else if(raster->format == Raster::C888)
			conv = conv_RGB888_to_RGB888;
		else
			goto err;
#endif
		break;
	case 24:
#ifdef RW_GLES
		conv = conv_RGB888_to_RGBA8888;
#else
		if(raster->format == Raster::C8888)
			conv = conv_RGB888_to_RGBA8888;
		else if(raster->format == Raster::C888)
			conv = conv_RGB888_to_RGB888;
		else
			goto err;
#endif
		break;
	case 16:
#ifdef RW_GLES
		conv = conv_RGBA1555_to_RGBA8888;
#else
		if(raster->format == Raster::C1555)
			conv = conv_RGBA1555_to_RGBA5551;
		else
			goto err;
#endif
		break;

	case 8:
	case 4:
	default:
	err:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	natras->hasAlpha = image->hasAlpha();

	uint8 *pixels = raster->lock(0, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
	assert(pixels);
	uint8 *imgpixels = image->pixels + (image->height-1)*image->stride;

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
		imgpixels -= image->stride;
		pixels += raster->stride;
	}

	raster->unlock(0);
	return 1;
}

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	Gl3Raster *ras = PLUGINOFFSET(Gl3Raster, object, offset);
	ras->texid = 0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	Gl3Raster *ras = PLUGINOFFSET(Gl3Raster, object, offset);
#ifdef RW_OPENGL
	glDeleteTextures(1, &ras->texid);
#endif
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	Gl3Raster *d = PLUGINOFFSET(Gl3Raster, dst, offset);
	d->texid = 0;
	return dst;
}

static uint32
getLevelSize(Raster *raster, int32 level)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	uint32 size = raster->stride*raster->height;
	while(level--)
		size /= 4;
	return size;
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
	if(platform != PLATFORM_GL3){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}
	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	// Texture
	tex->filterAddressing = stream->readU32();
	stream->read8(tex->name, 32);
	stream->read8(tex->mask, 32);

	// Raster
	uint32 format = stream->readU32();
	int32 width = stream->readI32();
	int32 height = stream->readI32();
	int32 depth = stream->readI32();
	int32 numLevels = stream->readI32();

	Raster *raster;
	Gl3Raster *natras;
	raster = Raster::create(width, height, depth, format | Raster::TEXTURE, PLATFORM_GL3);
	assert(raster);
	natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	tex->raster = raster;

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = stream->readU32();
		if(i < raster->getNumLevels()){
			data = raster->lock(i, Raster::LOCKWRITE|Raster::LOCKNOFETCH);
			stream->read8(data, size);
			raster->unlock(i);
		}else
			stream->seek(size);
	}
	return tex;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_STRUCT, chunksize-12);
	stream->writeU32(PLATFORM_GL3);

	// Texture
	stream->writeU32(tex->filterAddressing);
	stream->write8(tex->name, 32);
	stream->write8(tex->mask, 32);

	// Raster
	int32 numLevels = raster->getNumLevels();
	stream->writeI32(raster->format);
	stream->writeI32(raster->width);
	stream->writeI32(raster->height);
	stream->writeI32(raster->depth);
	stream->writeI32(numLevels);
	// TODO: compression? auto mipmaps?

	uint32 size;
	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		size = getLevelSize(raster, i);
		stream->writeU32(size);
		data = raster->lock(i, Raster::LOCKREAD);
		stream->write8(data, size);
		raster->unlock(i);
	}
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 72 + 20;
	int32 levels = tex->raster->getNumLevels();
	for(int32 i = 0; i < levels; i++)
		size += 4 + getLevelSize(tex->raster, i);
	return size;
}



void registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(Gl3Raster),
	                                            ID_RASTERGL3,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

}
}
