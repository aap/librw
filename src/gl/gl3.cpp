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

#include "rwgl3impl.h"

namespace rw {
namespace gl3 {

// TODO: make some of these things platform-independent

void*
driverOpen(void *o, int32, int32)
{
#ifdef RW_OPENGL
	driver[PLATFORM_GL3]->defaultPipeline = makeDefaultPipeline();
#endif
	driver[PLATFORM_GL3]->rasterNativeOffset = nativeRasterOffset;
	driver[PLATFORM_GL3]->rasterCreate       = rasterCreate;
	driver[PLATFORM_GL3]->rasterLock         = rasterLock;
	driver[PLATFORM_GL3]->rasterUnlock       = rasterUnlock;
	driver[PLATFORM_GL3]->rasterNumLevels    = rasterNumLevels;
	driver[PLATFORM_GL3]->rasterFromImage    = rasterFromImage;

	return o;
}

void*
driverClose(void *o, int32, int32)
{
	return o;
}

void
initializePlatform(void)
{
	Driver::registerPlugin(PLATFORM_GL3, 0, PLATFORM_GL3,
	                       driverOpen, driverClose);

	registerNativeRaster();

#ifdef RW_OPENGL
	// uniforms need to be registered before any shaders are created
	registerBlock("Scene");
	registerBlock("Object");
	registerBlock("State");
	registerUniform("u_matColor");
	registerUniform("u_surfaceProps");
#endif
}

}
}
