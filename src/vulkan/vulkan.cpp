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

namespace rw {
namespace vulkan {

static void*
driverOpen(void *o, int32, int32)
{
	engine->driver[PLATFORM_VULKAN]->defaultPipeline = makeDefaultPipeline();
	engine->driver[PLATFORM_VULKAN]->rasterNativeOffset = nativeRasterOffset;
	engine->driver[PLATFORM_VULKAN]->rasterCreate       = rasterCreate;
	engine->driver[PLATFORM_VULKAN]->rasterLock         = rasterLock;
	engine->driver[PLATFORM_VULKAN]->rasterUnlock       = rasterUnlock;
	engine->driver[PLATFORM_VULKAN]->rasterNumLevels    = rasterNumLevels;
	engine->driver[PLATFORM_VULKAN]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_VULKAN]->rasterFromImage    = rasterFromImage;
	engine->driver[PLATFORM_VULKAN]->rasterToImage      = rasterToImage;

	return o;
}

static void*
driverClose(void *o, int32, int32)
{
	return o;
}

void
registerPlatformPlugins(void)
{
	Driver::registerPlugin(PLATFORM_VULKAN, 0, PLATFORM_VULKAN,
	                       driverOpen, driverClose);
	registerNativeRaster();
}

}
}

#endif
