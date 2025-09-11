#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwengine.h"
#include "../rwplugins.h"
#include "rwd3d.h"
#include "rwd3d11.h"

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifndef RW_D3D11
void skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance) {}
void skinRenderCB(Atomic *atomic, InstanceDataHeader *header) {}
#else

static void *skin_amb_VS;
static void *skin_amb_dir_VS;
static void *skin_all_VS;

#define NUMDECLELT 14

void
skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	// TODO: Implement D3D11 skin instance callback
}

enum
{
	VSLOC_boneMatrices = VSLOC_afterLights
};

static float skinMatrices[64*16];

void
uploadSkinMatrices(Atomic *a)
{
	// TODO: Implement D3D11 skin matrix upload
}

void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	// TODO: Implement D3D11 skin render callback
}

void
createSkinShaders(void)
{
	// TODO: Implement D3D11 skin shader creation
}

void
destroySkinShaders(void)
{
	// TODO: Implement D3D11 skin shader destruction
}

#endif

static void*
skinOpen(void *o, int32, int32)
{
#ifdef RW_D3D11
	createSkinShaders();
#endif

	skinGlobals.pipelines[PLATFORM_D3D11] = makeSkinPipeline();
	return o;
}

static void*
skinClose(void *o, int32, int32)
{
#ifdef RW_D3D11
	destroySkinShaders();
#endif

	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_D3D11])->destroy();
	skinGlobals.pipelines[PLATFORM_D3D11] = nil;
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, ID_SKIN,
	                       skinOpen, skinClose);
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = nil;
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

}
}
