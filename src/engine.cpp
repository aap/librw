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
Driver driver[NUM_PLATFORMS];

PluginList Engine::s_plglist = {sizeof(Engine), sizeof(Engine), nil, nil};

void
Engine::init(void)
{
	ObjPipeline *defpipe = new ObjPipeline(PLATFORM_nil);
	for(uint i = 0; i < NUM_PLATFORMS; i++){
		driver[i].defaultPipeline = defpipe;

		driver[i].beginUpdate = null::beginUpdate;
		driver[i].endUpdate = null::endUpdate;

		driver[i].rasterCreate = null::rasterCreate;
		driver[i].rasterLock = null::rasterLock;
		driver[i].rasterUnlock = null::rasterUnlock;
		driver[i].rasterNumLevels = null::rasterNumLevels;
		driver[i].rasterFromImage = null::rasterFromImage;
	}
	Frame::dirtyList.init();

        rw::gl3::registerNativeRaster();
        rw::ps2::registerNativeRaster();
        rw::xbox::registerNativeRaster();
        rw::d3d::registerNativeRaster();
}

void
Engine::open(void)
{
	rw::engine = (Engine*)malloc(s_plglist.size);
	s_plglist.construct(rw::engine);

	ps2::initializePlatform();
	xbox::initializePlatform();
	d3d8::initializePlatform();
	d3d9::initializePlatform();
	gl3::initializePlatform();
	wdgl::initializePlatform();
}

namespace null {

void beginUpdate(Camera*) { }

void endUpdate(Camera*) { }


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

}
}
