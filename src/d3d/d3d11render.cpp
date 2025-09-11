#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "rwd3d.h"
#include "rwd3d11.h"

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifndef RW_D3D11
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
void defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header) {}
#else

void
drawInst_simple(d3d11::InstanceDataHeader *header, d3d11::InstanceData *inst)
{
	// TODO: Implement D3D11 simple draw instance
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(d3d11::InstanceDataHeader *header, InstanceData *inst)
{
	// TODO: Implement D3D11 GS emulation draw instance
}

void
drawInst(d3d11::InstanceDataHeader *header, d3d11::InstanceData *inst)
{
	// TODO: Implement D3D11 draw instance dispatcher
}

void
defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header)
{
	// TODO: Implement D3D11 default shader render callback
}

#endif
}
}
