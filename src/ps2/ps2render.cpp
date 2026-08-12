#ifdef RW_PS2

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwps2.h"
#include "rwps2plg.h"

#include "rwps2impl.h"

#define PLUGIN_ID 2

#include <libgraph.h>

extern rw::ps2::VUdesc vu1_default_desc[3];
extern rw::uint32 vu1_default[];



namespace rw {
namespace ps2 {

void
uploadLights(WorldLights *lightData, Matrix *worldMat)
{
	int i;
	uint128 tmp;
	QWord q;
	RGBAf *col;
	V3d localDir;

	int dmasz = 1;
	if(lightData->numAmbients)
		dmasz++;
	dmasz += lightData->numDirectionals*2;

	Matrix lightmat;
	if(lightData->numDirectionals || lightData->numLocals)
		Matrix::invert(&lightmat, worldMat);

	assert(dmasz <= 0x3F0-0x3D0);
	MAKEQ(tmp, UNPACK(V4_32, dmasz, vuLight), STCYCL(4,4), 0, DMAcnt + dmasz);
	vifPacket[vifPacksz++].q_u128 = tmp;

	if(lightData->numAmbients){
		
		q.q_f[0] = lightData->ambient.red*255.0f;
		q.q_f[1] = lightData->ambient.green*255.0f;
		q.q_f[2] = lightData->ambient.blue*255.0f;
		q.q_u32[3] = 1;	// ambient
		vifPacket[vifPacksz++] = q;
	}
	for(i = 0; i < lightData->numDirectionals; i++){
		Light *l = lightData->directionals[i];
		col = &l->color;
		q.q_f[0] = col->red*255.0f;
		q.q_f[1] = col->green*255.0f;
		q.q_f[2] = col->blue*255.0f;
		q.q_u32[3] = 3;	// direct
		vifPacket[vifPacksz++] = q;
		V3d *dir = &l->getFrame()->getLTM()->at;
		V3d::transformVectors(&localDir, dir, 1, &lightmat);
		q.q_f[0] = localDir.x;
		q.q_f[1] = localDir.y;
		q.q_f[2] = localDir.z;
		q.q_f[3] = 0.0f;
		vifPacket[vifPacksz++] = q;
	}

	// TODO: local lights

	MAKEQ(tmp, 0, 0, 0, 0);
	vifPacket[vifPacksz++].q_u128 = tmp;	// end
}

void
lightingCB(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[8];
	Light *locals[8];
	lightData.directionals = directionals;
	lightData.numDirectionals = 8;
	lightData.locals = locals;
	lightData.numLocals = 8;
	if(atomic->geometry->flags & rw::Geometry::LIGHT){
		engine->currentWorld->enumerateLights(atomic, &lightData);
		if((atomic->geometry->flags & rw::Geometry::NORMALS) == 0){
			// Get rid of lights that need normals when we don't have any
			lightData.numDirectionals = 0;
			lightData.numLocals = 0;
		}
		return uploadLights(&lightData, atomic->getFrame()->getLTM());
	}else{
		memset(&lightData, 0, sizeof(lightData));
		return uploadLights(&lightData, nil);
	}
}


int
setupAtomic(Atomic *atomic)
{
	uint128 tmp;

	setMatrix((RawMatrix*)&vuConst.mat0, atomic->getFrame()->getLTM());

	lightingCB(atomic);

	MeshHeader *header = atomic->geometry->meshHeader;
	PrimitiveType type = header->flags == rw::MeshHeader::TRISTRIP ?
                rw::PRIMTYPETRISTRIP : rw::PRIMTYPETRILIST;
	uploadVUCode(vu1_default);

	// DMAcnt
	int dmasz = 12;	// general VIF unpacks
	MAKEQ(tmp, VIFflush, VIFflush, 0, DMAcnt + dmasz);
	vifPacket[vifPacksz++].q_u128 = tmp;

	// some uploads
	MAKEQ(tmp, UNPACK(V4_32, 7, vuMatrix), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	vifPacket[vifPacksz++] = vuConst.mat0;
	vifPacket[vifPacksz++] = vuConst.mat1;
	vifPacket[vifPacksz++] = vuConst.mat2;
	vifPacket[vifPacksz++] = vuConst.mat3;
	vifPacket[vifPacksz++] = vuConst.xyzwScale_3D;
	vifPacket[vifPacksz++] = vuConst.xyzwOffset_3D;
	vifPacket[vifPacksz++] = vuConst.clipConsts;

	MAKEQ(tmp, UNPACK(V4_32, 1, vuGifTag), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	MAKE128(tmp, 0x412, SCE_GIF_SET_TAG(0, 1, 1,rw2gsPrim[type], SCE_GIF_PACKED, 3));
	vifPacket[vifPacksz++].q_u128 = tmp;

	MAKEQ(tmp, UNPACK(V4_32, 1, vuVuSwitch), STCYCL(4,4), VIFnop, VIFnop);
	vifPacket[vifPacksz++].q_u128 = tmp;
	VUdesc *desc;
	if(doClipping){
		if(type == PRIMTYPETRILIST)
			desc = &vu1_default_desc[1];
		else if(type == PRIMTYPETRISTRIP)
			desc = &vu1_default_desc[2];
		else
			printf("invalid primitive type\n");
	}else
		desc = &vu1_default_desc[0];
	MAKEQ(tmp, 0, desc->buf2, desc->buf1, desc->process>>3);
	vifPacket[vifPacksz++].q_u128 = tmp;

	return 0;
}

void
setupMaterial(Material *mat)
{
	uint128 tmp;
	QWord q;
	RGBAf matcol;
	Texture *tex = mat->texture;

	// TODO: set texture alpha state
	if(tex)
		uploadRaster(tex->raster);

	// TODO: no vertex alpha, sad
	rw::SetRenderState(VERTEXALPHA, mat->color.alpha != 0xFF);

	QWord scale = (tex == nil || doModulate2) ? colorNoScale : colorTexScale;
	MAKEQ(tmp, UNPACK(V4_32, 2, vuColScale), STCYCL(4,4), 0, DMAcnt + 2);
	vifPacket[vifPacksz++].q_u128 = tmp;

	// material color
	convColor(&matcol, &mat->color);
	if(tex){
		setTexture(tex->raster, tex->getAddressU(), tex->getAddressV(), tex->getFilter());
		q.q_f[0] = matcol.red*scale.q_f[0];
		q.q_f[1] = matcol.green*scale.q_f[1];
		q.q_f[2] = matcol.blue*scale.q_f[2];
		q.q_f[3] = matcol.alpha*scale.q_f[3];
	}else{
		setTexture(nil, 0, 0, 0);
		q.q_f[0] = matcol.red*scale.q_f[0];
		q.q_f[1] = matcol.green*scale.q_f[1];
		q.q_f[2] = matcol.blue*scale.q_f[2];
		q.q_f[3] = matcol.alpha*scale.q_f[3];
	}
	vifPacket[vifPacksz++] = q;

	// surface props
	q.q_f[0] = mat->surfaceProps.ambient;
	q.q_f[1] = mat->surfaceProps.specular;
	q.q_f[2] = mat->surfaceProps.diffuse;
	q.q_f[3] = 0.0f;
	vifPacket[vifPacksz++] = q;

	flushGSRegs();
}

void
defaultAtomicRender(rw::ObjPipeline *pipe, Atomic *atomic)
{
	int i;
	uint128 tmp;
	ObjPipeline *op = (ObjPipeline*)pipe;

	Geometry *geo = atomic->geometry;
	op->instance(atomic);
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_PS2);

	InstanceDataHeader *instData = (InstanceDataHeader*)geo->instData;
	MeshHeader *header = geo->meshHeader;

	setupAtomic(atomic);

	for(i = 0; i < instData->numMeshes; i++){
		Material *mat = instData->instanceMeshes[i].material;
		MatPipeline *mp = op->groupPipeline;
		if(mp == nil)
			mp = (MatPipeline*)mat->pipeline;
		if(mp == nil)
			mp = defaultMatPipe;

		// TODO: this should be a method of the pipeline
		setupMaterial(mat);

		// call geometry
		MAKEQ(tmp, VIFoffset+mp->vifOffset, VIFbase + 0, (uint32)instData->instanceMeshes[i].data, DMAcall);
		vifPacket[vifPacksz++].q_u128 = tmp;
	}
}

}
}

#endif
