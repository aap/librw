#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
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
	Geometry *geo = atomic->geometry;
	Frame *f = atomic->getFrame();
	device->SetTransform(D3DTS_WORLD, (D3DMATRIX*)f->getLTM());

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(inst->material->texture)
			setTexture(inst->material->texture);
		else
			device->SetTexture(0, NULL);
		setMaterial(inst->material);
		device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(0xFF, 0x40, 0x40, 0x40));
		device->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		device->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->geoflags & Geometry::PRELIT)
			device->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);

		device->SetFVF(inst->vertexShader);
		device->SetStreamSource(0, (IDirect3DVertexBuffer9*)inst->vertexBuffer, 0, inst->stride);
		device->SetIndices((IDirect3DIndexBuffer9*)inst->indexBuffer);
		uint32 numPrim = inst->primType == D3DPT_TRIANGLESTRIP ? inst->numIndices-2 : inst->numIndices/3;
		device->DrawIndexedPrimitive((D3DPRIMITIVETYPE)inst->primType, inst->baseIndex,
		                             0, inst->numVertices, 0, numPrim);
		inst++;
	}
}

#endif
}
}