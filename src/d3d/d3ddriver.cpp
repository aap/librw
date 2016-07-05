#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "rwd3d.h"

namespace rw {
namespace d3d {

#ifdef RW_D3D9

#define MAXNUMSTATES D3DRS_BLENDOPALPHA
#define MAXNUMSTAGES 8
#define MAXNUMTEXSTATES D3DTSS_CONSTANT
#define MAXNUMSAMPLERSTATES D3DSAMP_DMAPOFFSET

static int32 numDirtyStates;
static uint32 dirtyStates[MAXNUMSTATES];
static struct {
	uint32 value;
	bool32 dirty;
} stateCache[MAXNUMSTATES];
static uint32 d3dStates[MAXNUMSTATES];

static int32 numDirtyTextureStageStates;
static struct {
	uint32 stage;
	uint32 type;
} dirtyTextureStageStates[MAXNUMTEXSTATES*MAXNUMSTAGES];
static struct {
	uint32 value;
	bool32 dirty;
} textureStageStateCache[MAXNUMSTATES][MAXNUMSTAGES];
static uint32 d3dTextureStageStates[MAXNUMSTATES][MAXNUMSTAGES];

static uint32 d3dSamplerStates[MAXNUMSAMPLERSTATES][MAXNUMSTAGES];

static Raster *d3dRaster[MAXNUMSTAGES];

static D3DMATERIAL9 d3dmaterial;

void
setRenderState(uint32 state, uint32 value)
{
	if(stateCache[state].value != value){
		stateCache[state].value = value;
		if(!stateCache[state].dirty){
			stateCache[state].dirty = 1;
			dirtyStates[numDirtyStates++] = state;
		}
	}
}

void
setTextureStageState(uint32 stage, uint32 type, uint32 value)
{
	if(textureStageStateCache[type][stage].value != value){
		textureStageStateCache[type][stage].value = value;
		if(!textureStageStateCache[type][stage].dirty){
			textureStageStateCache[type][stage].dirty = 1;
			dirtyTextureStageStates[numDirtyTextureStageStates].stage = stage;
			dirtyTextureStageStates[numDirtyTextureStageStates].type = type;
			numDirtyTextureStageStates++;
		}
	}
}

void
flushCache(void)
{
	uint32 s, t;
	uint32 v;
	for(int32 i = 0; i < numDirtyStates; i++){
		s = dirtyStates[i];
		v = stateCache[s].value;
		stateCache[s].dirty = 0;
		if(d3dStates[s] != v){
			device->SetRenderState((D3DRENDERSTATETYPE)s, v);
			d3dStates[s] = v;
		}
	}
	numDirtyStates = 0;
	for(int32 i = 0; i < numDirtyTextureStageStates; i++){
		s = dirtyTextureStageStates[i].stage;
		t = dirtyTextureStageStates[i].type;
		v = textureStageStateCache[t][s].value;
		textureStageStateCache[t][s].dirty = 0;
		if(d3dTextureStageStates[t][s] != v){
			device->SetTextureStageState(s, (D3DTEXTURESTAGESTATETYPE)t, v);
			d3dTextureStageStates[t][s] = v;
		}
	}
	numDirtyTextureStageStates = 0;
}

void
setSamplerState(uint32 stage, uint32 type, uint32 value)
{
	if(d3dSamplerStates[type][stage] != value){
		device->SetSamplerState(stage, (D3DSAMPLERSTATETYPE)type, value);
		d3dSamplerStates[type][stage] = value;
	}
}

void
setRasterStage(uint32 stage, Raster *raster)
{
	D3dRaster *d3draster = nil;
	if(raster != d3dRaster[stage]){
		d3dRaster[stage] = raster;
		if(raster){
			d3draster = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
			device->SetTexture(stage, (IDirect3DTexture9*)d3draster->texture);
		}else
			device->SetTexture(stage, nil);
	}
}

void
setTexture(uint32 stage, Texture *tex)
{
	static DWORD filternomip[] = {
		0, D3DTEXF_POINT, D3DTEXF_LINEAR,
		   D3DTEXF_POINT, D3DTEXF_LINEAR,
		   D3DTEXF_POINT, D3DTEXF_LINEAR
	};
	static DWORD wrap[] = {
		0, D3DTADDRESS_WRAP, D3DTADDRESS_MIRROR,
		D3DTADDRESS_CLAMP, D3DTADDRESS_BORDER
	};
	if(tex == nil){
		setRasterStage(stage, nil);
		return;
	}
	if(tex->raster){
		setSamplerState(stage, D3DSAMP_MAGFILTER, filternomip[tex->filterAddressing & 0xFF]);
		setSamplerState(stage, D3DSAMP_MINFILTER, filternomip[tex->filterAddressing & 0xFF]);
		setSamplerState(stage, D3DSAMP_ADDRESSU, wrap[(tex->filterAddressing >> 8) & 0xF]);
		setSamplerState(stage, D3DSAMP_ADDRESSV, wrap[(tex->filterAddressing >> 12) & 0xF]);
	}
	setRasterStage(stage, tex->raster);
}

void
setMaterial(Material *mat)
{
	D3DMATERIAL9 mat9;
	D3DCOLORVALUE black = { 0, 0, 0, 0 };
	float ambmult = mat->surfaceProps.ambient/255.0f;
	float diffmult = mat->surfaceProps.diffuse/255.0f;
	mat9.Ambient.r = mat->color.red*ambmult;
	mat9.Ambient.g = mat->color.green*ambmult;
	mat9.Ambient.b = mat->color.blue*ambmult;
	mat9.Ambient.a = mat->color.alpha*ambmult;
	mat9.Diffuse.r = mat->color.red*diffmult;
	mat9.Diffuse.g = mat->color.green*diffmult;
	mat9.Diffuse.b = mat->color.blue*diffmult;
	mat9.Diffuse.a = mat->color.alpha*diffmult;
	mat9.Power = 0.0f;
	mat9.Emissive = black;
	mat9.Specular = black;
	if(d3dmaterial.Diffuse.r != mat9.Diffuse.r ||
	   d3dmaterial.Diffuse.g != mat9.Diffuse.g ||
	   d3dmaterial.Diffuse.b != mat9.Diffuse.b ||
	   d3dmaterial.Diffuse.a != mat9.Diffuse.a ||
	   d3dmaterial.Ambient.r != mat9.Ambient.r ||
	   d3dmaterial.Ambient.g != mat9.Ambient.g ||
	   d3dmaterial.Ambient.b != mat9.Ambient.b ||
	   d3dmaterial.Ambient.a != mat9.Ambient.a){
		device->SetMaterial(&mat9);
		d3dmaterial = mat9;
	}
}

void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];

	// View Matrix
	Matrix inv;
	// TODO: this can be simplified, or we use matrix flags....
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  =  -inv.at.x;
	view[9]  =   inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
        device->SetTransform(D3DTS_VIEW, (D3DMATRIX*)view);

	// Projection Matrix
	float32 invwx = 1.0f/this->viewWindow.x;
	float32 invwy = 1.0f/this->viewWindow.y;
	float32 invz = 1.0f/(this->farPlane-this->nearPlane);

	// is this all really correct? RW code looks a bit different...
	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	if(this->projection == PERSPECTIVE){
		proj[8] = this->viewOffset.x*invwx;
		proj[9] = this->viewOffset.y*invwy;
		proj[10] = this->farPlane*invz;
		proj[11] = 1.0f;

		proj[12] = 0.0f;
		proj[13] = 0.0f;
		proj[14] = -this->nearPlane*proj[10];
		proj[15] = 0.0f;
	}else{
		proj[8] = 0.0f;
		proj[9] = 0.0f;
		proj[10] = invz;
		proj[11] = 0.0f;

		proj[12] = this->viewOffset.x*invwx;
		proj[13] = this->viewOffset.y*invwy;
		proj[14] = -this->nearPlane*proj[10];
		proj[15] = 1.0f;
	}
        device->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)proj);
}

void
initializeRender(void)
{
	driver[PLATFORM_D3D8]->beginUpdate = beginUpdate;
	driver[PLATFORM_D3D9]->beginUpdate = beginUpdate;
}

#endif
}
}
