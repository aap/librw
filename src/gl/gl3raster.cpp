#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

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

static Raster*
rasterCreateTexture(Raster *raster)
{
#ifdef RW_OPENGL
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(raster->format & 0xF00){
	case Raster::C8888:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		break;
	case Raster::C888:
		natras->internalFormat = GL_RGB;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		break;
	case Raster::C1555:
		// TODO: check if this is correct
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

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
#else
	return nil;
#endif
}

#ifdef RW_OPENGL

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
	default:
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		break;
	case Raster::C888:
		natras->internalFormat = GL_RGB;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		break;
	case Raster::C1555:
		// TODO: check if this is correct
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_5_5_5_1;
		natras->hasAlpha = 1;
		break;
	}

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

Raster*
rasterCreate(Raster *raster)
{
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

#ifdef RW_OPENGL
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
		assert(0 && "unsupported format");	
	}
	return nil;
}

uint8*
rasterLock(Raster*, int32 level, int32 lockMode)
{
	printf("locking\n");
	return nil;
}

void
rasterUnlock(Raster*, int32)
{
	printf("unlocking\n");
}

int32
rasterNumLevels(Raster*)
{
	printf("numlevels\n");
	return 0;
}

bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *width, int32 *height, int32 *depth, int32 *format)
{
	assert(0 && "not yet");
	return 0;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	assert(0 && "not yet");
	int32 format;
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	switch(image->depth){
	case 32:
		format = Raster::C8888;
		break;
	case 24:
		format = Raster::C888;
		break;
	case 16:
		format = Raster::C1555;
		break;
	default:
		assert(0 && "image depth\n");
	}
	format |= Raster::TEXTURE;

	raster->type = format & 0x7;
	raster->flags = format & 0xF8;
	raster->format = format & 0xFF00;
	rasterCreate(raster);

	natras->hasAlpha = image->hasAlpha();

#ifdef RW_OPENGL
	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, image->pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
#endif
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
destroyNativeRaster(void *object, int32, int32)
{
	//Gl3Raster *ras = PLUGINOFFSET(Gl3Raster, object, offset);
	// TODO
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	Gl3Raster *d = PLUGINOFFSET(Gl3Raster, dst, offset);
	d->texid = 0;
	return dst;
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
