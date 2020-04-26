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

#ifdef RW_OPENGL
static Raster*
rasterCreateTexture(Raster *raster)
{
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
		natras->type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
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
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		break;
	case Raster::C888:
	default:
		natras->internalFormat = GL_RGB;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		break;
	case Raster::C1555:
		// TODO: check if this is correct
		natras->internalFormat = GL_RGBA;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
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

static uint8*
flipImage(Image *image)
{
	int i;
	uint8 *newPx = (uint8*)rwMalloc(image->stride*image->height, 0);
	for(i = 0; i < image->height; i++)
		memcpy(&newPx[i*image->stride], &image->pixels[(image->height-1-i)*image->stride], image->stride);
	return newPx;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type&0xF) != Raster::TEXTURE)
		return 0;

#ifdef RW_OPENGL
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

	// NB: important to set the format of the input data here!
	Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
	switch(image->depth){
	case 32:
		if(raster->format != Raster::C8888 &&
		   raster->format != Raster::C888)
			goto err;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 1;
		break;
	case 24:
		if(raster->format != Raster::C888) goto err;
		natras->format = GL_RGB;
		natras->type = GL_UNSIGNED_BYTE;
		natras->hasAlpha = 0;
		break;
	case 16:
		if(raster->format != Raster::C1555) goto err;
		natras->format = GL_RGBA;
		natras->type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
		natras->hasAlpha = 1;
		break;

	case 8:
	case 4:
	default:
	err:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	natras->hasAlpha = image->hasAlpha();


	uint8 *flipped = flipImage(image);

	glBindTexture(GL_TEXTURE_2D, natras->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, natras->internalFormat,
	             raster->width, raster->height,
	             0, natras->format, natras->type, flipped);
	glBindTexture(GL_TEXTURE_2D, 0);

	rwFree(flipped);
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
