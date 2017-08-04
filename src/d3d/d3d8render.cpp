#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "rwd3d.h"
#include "rwd3d8.h"

namespace rw {
namespace d3d8 {
using namespace d3d;

#ifndef RW_D3D9
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
#else

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	RawMatrix world;

	Geometry *geo = atomic->geometry;
	Frame *f = atomic->getFrame();
	convMatrix(&world, f->getLTM());
	device->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&world);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		d3d::setTexture(0, inst->material->texture);
		d3d::setMaterial(inst->material);
		d3d::setRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(0xFF, 0x40, 0x40, 0x40));
		d3d::setRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		d3d::setRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->flags & Geometry::PRELIT)
			d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);

		device->SetFVF(inst->vertexShader);
		device->SetStreamSource(0, (IDirect3DVertexBuffer9*)inst->vertexBuffer, 0, inst->stride);
		device->SetIndices((IDirect3DIndexBuffer9*)inst->indexBuffer);
		uint32 numPrim = inst->primType == D3DPT_TRIANGLESTRIP ? inst->numIndices-2 : inst->numIndices/3;
		d3d::flushCache();
		device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)inst->primType, inst->baseIndex,
		                             0, inst->numVertices, 0, numPrim);
		inst++;
	}
}

#endif
}
}
