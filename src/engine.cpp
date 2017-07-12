#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "rwbase.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "ps2/rwps2.h"
#include "d3d/rwxbox.h"
#include "d3d/rwd3d.h"
#include "d3d/rwd3d8.h"
#include "d3d/rwd3d9.h"
#include "gl/rwgl3.h"
#include "gl/rwwdgl.h"

namespace rw {

Engine *engine;
Driver *driver[NUM_PLATFORMS];

void
Engine::init(void)
{
	engine = new Engine;
	PluginList init = { sizeof(Driver), sizeof(Driver), nil, nil };

	for(uint i = 0; i < NUM_PLATFORMS; i++)
		Driver::s_plglist[i] = init;

	Frame::dirtyList.init();

	engine->currentCamera = nil;
	engine->currentWorld = nil;
	engine->currentTexDictionary = nil;
	engine->imtexture = nil;
	engine->loadTextures = 1;
	engine->makeDummies = 1;

	engine->beginUpdate = null::beginUpdate;
	engine->endUpdate = null::endUpdate;
	engine->clearCamera = null::clearCamera;
	engine->setRenderState = null::setRenderState;
	engine->getRenderState = null::getRenderState;
	engine->im2DRenderIndexedPrimitive = null::im2DRenderIndexedPrimitive;
	engine->zNear = 0.0f;	// random values
	engine->zFar  = 1.0f;

	ps2::initializePlatform();
	xbox::initializePlatform();
	d3d8::initializePlatform();
	d3d9::initializePlatform();
	wdgl::initializePlatform();
	gl3::initializePlatform();
}

PluginList Driver::s_plglist[NUM_PLATFORMS];

void
Driver::open(void)
{
	ObjPipeline *defpipe = new ObjPipeline(PLATFORM_NULL);

	for(uint i = 0; i < NUM_PLATFORMS; i++){
		rw::driver[i] = (Driver*)malloc(s_plglist[i].size);

		driver[i]->defaultPipeline = defpipe;

		driver[i]->rasterCreate = null::rasterCreate;
		driver[i]->rasterLock = null::rasterLock;
		driver[i]->rasterUnlock = null::rasterUnlock;
		driver[i]->rasterNumLevels = null::rasterNumLevels;
		driver[i]->rasterFromImage = null::rasterFromImage;
		driver[i]->rasterToImage = null::rasterToImage;


		s_plglist[i].construct(rw::driver[i]);
	}
}

namespace null {

void beginUpdate(Camera*) { }
void endUpdate(Camera*) { }
void clearCamera(Camera*,RGBA*,uint32) { }

void   setRenderState(int32, uint32) { }
uint32 getRenderState(int32) { return 0; }

void im2DRenderIndexedPrimitive(PrimitiveType, void*, int32, void*, int32) { }

void
rasterCreate(Raster*)
{
	assert(0 && "rasterCreate not implemented");
}

uint8*
rasterLock(Raster*, int32)
{
	assert(0 && "lockRaster not implemented");
	return nil;
}

void
rasterUnlock(Raster*, int32)
{
	assert(0 && "unlockRaster not implemented");
}

int32
rasterNumLevels(Raster*)
{
	assert(0 && "rasterNumLevels not implemented");
	return 0;
}

void
rasterFromImage(Raster*, Image*)
{
	assert(0 && "rasterFromImage not implemented");
}

Image*
rasterToImage(Raster*)
{
	assert(0 && "rasterToImage not implemented");
	return nil;
}

}
}
