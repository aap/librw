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
#include "rwd3d11.h"

namespace rw {
namespace d3d11 {

#ifdef RW_D3D11
int32 nativeRasterOffset;

static void*
driverOpen(void *o, int32, int32)
{
	engine->driver[PLATFORM_D3D11]->defaultPipeline = makeDefaultPipeline();

	engine->driver[PLATFORM_D3D11]->rasterNativeOffset = nativeRasterOffset;
	engine->driver[PLATFORM_D3D11]->rasterCreate       = rasterCreate;
	engine->driver[PLATFORM_D3D11]->rasterLock         = rasterLock;
	engine->driver[PLATFORM_D3D11]->rasterUnlock       = rasterUnlock;
	engine->driver[PLATFORM_D3D11]->rasterNumLevels    = rasterNumLevels;
	engine->driver[PLATFORM_D3D11]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_D3D11]->rasterFromImage    = rasterFromImage;
	engine->driver[PLATFORM_D3D11]->rasterToImage      = rasterToImage;
	return o;
}

static void*
driverClose(void *o, int32, int32)
{
	return o;
}
#endif

void
registerPlatformPlugins(void)
{
#ifdef RW_D3D11
	Driver::registerPlugin(PLATFORM_D3D11, 0, PLATFORM_D3D11,
	                       driverOpen, driverClose);
	if(nativeRasterOffset == 0)
		registerNativeRaster();
#endif
}

}
}
