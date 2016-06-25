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
#include "../rwplugins.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#endif
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

int32 nativeRasterOffset;

static void
rasterCreate(Raster *raster)
{
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	if(raster->flags & Raster::DONTALLOCATE)
		return;

	assert(raster->depth == 32);

#ifdef RW_OPENGL
	glGenTextures(1, &natras->texid);
	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raster->width, raster->height,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, nil);

	glBindTexture(GL_TEXTURE_2D, 0);
#endif
}

static uint8*
rasterLock(Raster*, int32 level)
{
	printf("locking\n");
	return nil;
}

static void
rasterUnlock(Raster*, int32)
{
	printf("unlocking\n");
}

static int32
rasterNumLevels(Raster*)
{
	printf("numlevels\n");
	return 0;
}

static void
rasterFromImage(Raster *raster, Image *image)
{
	int32 format;
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);

	format = Raster::C8888;
	format |= 4;

	raster->type = format & 0x7;
	raster->flags = format & 0xF8;
	raster->format = format & 0xFF00;
	rasterCreate(raster);

	assert(image->depth == 32);

	natras->hasAlpha = image->hasAlpha();

#ifdef RW_OPENGL
	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, raster->width, raster->height,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
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

static void*
nativeOpen(void*, int32 offset, int32)
{
        driver[PLATFORM_GL3].rasterNativeOffset = nativeRasterOffset;
	driver[PLATFORM_GL3].rasterCreate = rasterCreate;
	driver[PLATFORM_GL3].rasterLock = rasterLock;
	driver[PLATFORM_GL3].rasterUnlock = rasterUnlock;
	driver[PLATFORM_GL3].rasterNumLevels = rasterNumLevels;
	driver[PLATFORM_GL3].rasterFromImage = rasterFromImage;
}

static void*
nativeClose(void*, int32 offset, int32)
{
	printf("native close\n");
}

void registerNativeRaster(void)
{
	Engine::registerPlugin(0, 0x1234, nativeOpen, nativeClose);
        nativeRasterOffset = Raster::registerPlugin(sizeof(Gl3Raster),
                                                    0x12340000 | PLATFORM_GL3,
                                                    createNativeRaster,
                                                    destroyNativeRaster,
                                                    copyNativeRaster);
}

}
}
