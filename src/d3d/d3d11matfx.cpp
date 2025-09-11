#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "../rwplugins.h"
#include "rwd3d.h"
#include "rwd3d11.h"

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifndef RW_D3D11
void matfxRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header) {}
#else

static void *matfx_env_amb_VS;
static void *matfx_env_amb_dir_VS;
static void *matfx_env_all_VS;
static void *matfx_env_PS;
static void *matfx_env_tex_PS;

enum
{
	VSLOC_texMat = VSLOC_afterLights,
	VSLOC_colorClamp = VSLOC_texMat + 4,
	VSLOC_envColor,

	PSLOC_shininess = 1,
};

void
matfxRender_Default(InstanceDataHeader *header, InstanceData *inst, int32 lightBits)
{
	// TODO: Implement D3D11 default MatFX rendering
}

static Frame *lastEnvFrame;

static RawMatrix normal2texcoord = {
	{ 0.5f,  0.0f, 0.0f }, 0.0f,
	{ 0.0f, -0.5f, 0.0f }, 0.0f,
	{ 0.0f,  0.0f, 1.0f }, 0.0f,
	{ 0.5f,  0.5f, 0.0f }, 1.0f
};

void
uploadEnvMatrix(Frame *frame)
{
	// TODO: Implement D3D11 environment matrix upload
}

void
matfxRender_EnvMap(InstanceDataHeader *header, InstanceData *inst, int32 lightBits, MatFX::Env *env)
{
	// TODO: Implement D3D11 environment map rendering
}

void
matfxRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header)
{
	// TODO: Implement D3D11 MatFX shader render callback
}

void
createMatFXShaders(void)
{
	// TODO: Implement D3D11 MatFX shader creation
}

void
destroyMatFXShaders(void)
{
	// TODO: Implement D3D11 MatFX shader destruction
}

#endif

static void*
matfxOpen(void *o, int32, int32)
{
#ifdef RW_D3D11
	createMatFXShaders();
#endif

	matFXGlobals.pipelines[PLATFORM_D3D11] = makeMatFXPipeline();
	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
#ifdef RW_D3D11
	destroyMatFXShaders();
#endif

	((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_D3D11])->destroy();
	matFXGlobals.pipelines[PLATFORM_D3D11] = nil;
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = matfxRenderCB_Shader;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

}
}
