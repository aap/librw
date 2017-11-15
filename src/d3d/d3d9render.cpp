#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3d9.h"

namespace rw {
namespace d3d9 {
using namespace d3d;

#ifndef RW_D3D9
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
#else

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	RawMatrix world;
	Geometry *geo = atomic->geometry;

	int lighting = !!(geo->flags & rw::Geometry::LIGHT);
	if(lighting)
		d3d::lightingCB();
	else
		return;

	d3d::setRenderState(D3DRS_LIGHTING, lighting);

	Frame *f = atomic->getFrame();
	convMatrix(&world, f->getLTM());
	d3ddevice->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&world);

	d3ddevice->SetStreamSource(0, (IDirect3DVertexBuffer9*)header->vertexStream[0].vertexBuffer,
	                           0, header->vertexStream[0].stride);
	d3ddevice->SetIndices((IDirect3DIndexBuffer9*)header->indexBuffer);
	d3ddevice->SetVertexDeclaration((IDirect3DVertexDeclaration9*)header->vertexDeclaration);

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		d3d::setTexture(0, inst->material->texture);
		d3d::setMaterial(inst->material);

		d3d::setRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		d3d::setRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->flags & Geometry::PRELIT)
			d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
		else
			d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

		d3d::flushCache();
		d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
		                                0, inst->numVertices,
		                                inst->startIndex, inst->numPrimitives);
		inst++;
	}
}

#endif
}
}
