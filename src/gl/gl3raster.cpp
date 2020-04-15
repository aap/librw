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

namespace rw {
namespace gl3 {

int32 nativeRasterOffset;

void
rasterCreate(Raster *raster)
{
	// Dummy to use as subraster
	if(raster->width == 0 || raster->height == 0){
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		return;
	}

	switch(raster->type){
	case Raster::CAMERA:
		// TODO: set/check width, height, depth, format?
		raster->flags |= Raster::DONTALLOCATE;
		raster->originalWidth = raster->width;
		raster->originalHeight = raster->height;
		raster->stride = 0;
		raster->pixels = nil;
		break;
	case Raster::ZBUFFER:
		// TODO: set/check width, height, depth, format?
		raster->flags |= Raster::DONTALLOCATE;
		break;
	case Raster::NORMAL:
	case Raster::TEXTURE:
		// continue below
		break;
	default:
		assert(0 && "unsupported format");	
	}

	if(raster->flags & Raster::DONTALLOCATE)
		return;

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
		assert(0 && "unsupported raster format");
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
#endif
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

void
rasterFromImage(Raster *raster, Image *image)
{
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
