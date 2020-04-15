#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "rwd3d.h"
#include "rwd3d9.h"

namespace rw {
namespace d3d9 {
using namespace d3d;

#ifndef RW_D3D9
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
#else

void
drawInst(d3d9::InstanceDataHeader *header, d3d9::InstanceData *inst)
{
	d3d::flushCache();
	d3ddevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)header->primType, inst->baseIndex,
	                                0, inst->numVertices,
	                                inst->startIndex, inst->numPrimitives);
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(d3d9::InstanceDataHeader *header, InstanceData *inst)
{
	uint32 hasAlpha;
	int alphafunc;
	int zwrite;
	d3d::getRenderState(D3DRS_ALPHABLENDENABLE, &hasAlpha);
	if(hasAlpha){
		zwrite = rw::GetRenderState(rw::ZWRITEENABLE);
		alphafunc = rw::GetRenderState(rw::ALPHATESTFUNC);
		if(zwrite){
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAGREATEREQUAL);
			drawInst(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHALESS);
			SetRenderState(rw::ZWRITEENABLE, 0);
			drawInst(header, inst);
			SetRenderState(rw::ZWRITEENABLE, 1);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
		}else{
			SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAALWAYS);
			drawInst(header, inst);
			SetRenderState(rw::ALPHATESTFUNC, alphafunc);
		}
	}else
		drawInst(header, inst);
}

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	RawMatrix world;
	Geometry *geo = atomic->geometry;

	int lighting = !!(geo->flags & rw::Geometry::LIGHT);
	if(lighting)
		d3d::lightingCB(!!(atomic->geometry->flags & Geometry::NORMALS));

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
		SetRenderState(VERTEXALPHA, inst->vertexAlpha || inst->material->color.alpha != 255);
		const static rw::RGBA white = { 255, 255, 255, 255 };
		d3d::setMaterial(inst->material->surfaceProps, white);

		d3d::setRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
		if(geo->flags & Geometry::PRELIT)
			d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1);
		else
			d3d::setRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);
		d3d::setRenderState(D3DRS_DIFFUSEMATERIALSOURCE, inst->vertexAlpha ? D3DMCS_COLOR1 : D3DMCS_MATERIAL);

		if(inst->material->texture){
			// Texture
			d3d::setTexture(0, inst->material->texture);
			d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			d3d::setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_CURRENT);
			d3d::setTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
			d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			d3d::setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			d3d::setTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
		}else{
			d3d::setTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			d3d::setTextureStageState(0, D3DTSS_COLORARG1, D3DTA_CURRENT);
			d3d::setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			d3d::setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
		}

		// Material colour
		const rw::RGBA *col = &inst->material->color;
		d3d::setTextureStageState(1, D3DTSS_CONSTANT, D3DCOLOR_ARGB(col->alpha,col->red,col->green,col->blue));
		d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		d3d::setTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		d3d::setTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CONSTANT);
		d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		d3d::setTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
		d3d::setTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_CONSTANT);

		drawInst(header, inst);
		inst++;
	}
	d3d::setTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	d3d::setTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

#endif
}
}
