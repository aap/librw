#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwerror.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "rwd3d.h"
#include "rwd3dimpl.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d {

#ifdef RW_D3D9

D3d9Globals d3d9Globals;

// Keep track of rasters exclusively in video memory
// as they need special treatment sometimes
struct VidmemRaster
{
	Raster *raster;
	VidmemRaster *next;
};
static VidmemRaster *vidmemRasters;
void addVidmemRaster(Raster *raster);
void removeVidmemRaster(Raster *raster);

struct RwRasterStateCache {
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

#define MAXNUMSTAGES 8

// cached RW render states
struct RwStateCache {
	bool32 vertexAlpha;
	bool32 textureAlpha;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 fogenable;
	RGBA fogcolor;
	uint32 cullmode;
	uint32 alphafunc;
	uint32 alpharef;
	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

#define MAXNUMSTATES (D3DRS_BLENDOPALPHA+1)
#define MAXNUMTEXSTATES (D3DTSS_CONSTANT+1)
#define MAXNUMSAMPLERSTATES (D3DSAMP_DMAPOFFSET+1)

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
} textureStageStateCache[MAXNUMTEXSTATES][MAXNUMSTAGES];
static uint32 d3dTextureStageStates[MAXNUMTEXSTATES][MAXNUMSTAGES];

static uint32 d3dSamplerStates[MAXNUMSAMPLERSTATES][MAXNUMSTAGES];


static bool validStates[MAXNUMSTATES];
static bool validTexStates[MAXNUMTEXSTATES];

static D3DMATERIAL9 d3dmaterial;


static uint32 blendMap[] = {
	D3DBLEND_ZERO,	// actually invalid
	D3DBLEND_ZERO,
	D3DBLEND_ONE,
	D3DBLEND_SRCCOLOR,
	D3DBLEND_INVSRCCOLOR,
	D3DBLEND_SRCALPHA,
	D3DBLEND_INVSRCALPHA,
	D3DBLEND_DESTALPHA,
	D3DBLEND_INVDESTALPHA,
	D3DBLEND_DESTCOLOR,
	D3DBLEND_INVDESTCOLOR,
	D3DBLEND_SRCALPHASAT
};

static uint32 alphafuncMap[] = {
	D3DCMP_ALWAYS,
	D3DCMP_GREATEREQUAL,
	D3DCMP_LESS
};

static uint32 cullmodeMap[] = {
	D3DCULL_NONE,	// actually invalid
	D3DCULL_NONE,
	D3DCULL_CW,
	D3DCULL_CCW
};

// TODO: support mipmaps
static uint32 filterConvMap_NoMIP[] = {
	0, D3DTEXF_POINT, D3DTEXF_LINEAR,
	   D3DTEXF_POINT, D3DTEXF_LINEAR,
	   D3DTEXF_POINT, D3DTEXF_LINEAR
};
static uint32 addressConvMap[] = {
	0, D3DTADDRESS_WRAP, D3DTADDRESS_MIRROR,
	D3DTADDRESS_CLAMP, D3DTADDRESS_BORDER
};

// D3D render state

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
getRenderState(uint32 state, uint32 *value)
{
	*value = stateCache[state].value;
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
getTextureStageState(uint32 stage, uint32 type, uint32 *value)
{
	*value = textureStageStateCache[type][stage].value;
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
			d3ddevice->SetRenderState((D3DRENDERSTATETYPE)s, v);
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
			d3ddevice->SetTextureStageState(s, (D3DTEXTURESTAGESTATETYPE)t, v);
			d3dTextureStageStates[t][s] = v;
		}
	}
	numDirtyTextureStageStates = 0;
}

void
setSamplerState(uint32 stage, uint32 type, uint32 value)
{
	if(d3dSamplerStates[type][stage] != value){
		d3ddevice->SetSamplerState(stage, (D3DSAMPLERSTATETYPE)type, value);
		d3dSamplerStates[type][stage] = value;
	}
}

void
getSamplerState(uint32 stage, uint32 type, uint32 *value)
{
	*value = d3dSamplerStates[type][stage];
}

// Bring D3D device in accordance with saved render states (after a reset)
static void
resetD3d9Device(void)
{
	int32 i;
	uint32 s, t;
	for(i = 0; i < MAXNUMSTAGES; i++){
		Raster *raster = rwStateCache.texstage[i].raster;
		if(raster){
			D3dRaster *d3draster = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
			d3ddevice->SetTexture(i, (IDirect3DTexture9*)d3draster->texture);
		}else
			d3ddevice->SetTexture(i, nil);
		setSamplerState(i, D3DSAMP_ADDRESSU, addressConvMap[rwStateCache.texstage[i].addressingU]);
		setSamplerState(i, D3DSAMP_ADDRESSV, addressConvMap[rwStateCache.texstage[i].addressingV]);
		setSamplerState(i, D3DSAMP_MAGFILTER, filterConvMap_NoMIP[rwStateCache.texstage[i].filter]);
		setSamplerState(i, D3DSAMP_MINFILTER, filterConvMap_NoMIP[rwStateCache.texstage[i].filter]);
	}
	for(s = 0; s < MAXNUMSTATES; s++)
		if(validStates[s])
			d3ddevice->SetRenderState((D3DRENDERSTATETYPE)s, d3dStates[s]);
	for(t = 0; t < MAXNUMTEXSTATES; t++)
		if(validTexStates[t])
			for(s = 0; s < MAXNUMSTAGES; s++)
				d3ddevice->SetTextureStageState(s, (D3DTEXTURESTAGESTATETYPE)t, d3dTextureStageStates[t][s]);
	for(t = 1; t < MAXNUMSAMPLERSTATES; t++)
		for(s = 0; s < MAXNUMSTAGES; s++)
			d3ddevice->SetSamplerState(s, (D3DSAMPLERSTATETYPE)t, d3dSamplerStates[t][s]);
	d3ddevice->SetMaterial(&d3dmaterial);
}

// RW render state

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		if(rwStateCache.zwrite && !enable){
			// If we still want to write, enable but set mode to always
			setRenderState(D3DRS_ZENABLE, TRUE);
			setRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		}else{
			setRenderState(D3DRS_ZENABLE, rwStateCache.ztest);
			setRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			// Have to switch on ztest so writing can work
			setRenderState(D3DRS_ZENABLE, TRUE);
			setRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
		}
		setRenderState(D3DRS_ZWRITEENABLE, rwStateCache.zwrite);
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			setRenderState(D3DRS_ALPHABLENDENABLE, enable);
			setRenderState(D3DRS_ALPHATESTENABLE, enable);
		}
		rwStateCache.vertexAlpha = enable;
	}
}

void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	D3dRaster *d3draster = nil;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		if(raster){
			assert(raster->platform == PLATFORM_D3D8 ||
				raster->platform == PLATFORM_D3D9);
			d3draster = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
			d3ddevice->SetTexture(stage, (IDirect3DTexture9*)d3draster->texture);
			alpha = d3draster->hasAlpha;
		}else{
			d3ddevice->SetTexture(stage, nil);
			alpha = 0;
		}
		if(stage == 0){
			if(rwStateCache.textureAlpha != alpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					setRenderState(D3DRS_ALPHABLENDENABLE, alpha);
					setRenderState(D3DRS_ALPHATESTENABLE, alpha);
				}
			}
		}
	}
}

static void
setFilterMode(uint32 stage, int32 filter)
{
	// TODO: mip mapping
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter){
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		setSamplerState(stage, D3DSAMP_MAGFILTER, filterConvMap_NoMIP[filter]);
		setSamplerState(stage, D3DSAMP_MINFILTER, filterConvMap_NoMIP[filter]);
	}
}

static void
setAddressU(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		setSamplerState(stage, D3DSAMP_ADDRESSU, addressConvMap[addressing]);
	}
}

static void
setAddressV(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		setSamplerState(stage, D3DSAMP_ADDRESSV, addressConvMap[addressing]);
	}
}

void
setTexture(uint32 stage, Texture *tex)
{
	if(tex == nil){
		setRasterStage(stage, nil);
		return;
	}
	if(tex->raster){
		setFilterMode(stage, tex->getFilter());
		setAddressU(stage, tex->getAddressU());
		setAddressV(stage, tex->getAddressV());
	}
	setRasterStage(stage, tex->raster);
}

void
setD3dMaterial(D3DMATERIAL9 *mat9)
{
	if(d3dmaterial.Diffuse.r != mat9->Diffuse.r ||
	   d3dmaterial.Diffuse.g != mat9->Diffuse.g ||
	   d3dmaterial.Diffuse.b != mat9->Diffuse.b ||
	   d3dmaterial.Diffuse.a != mat9->Diffuse.a ||
	   d3dmaterial.Ambient.r != mat9->Ambient.r ||
	   d3dmaterial.Ambient.g != mat9->Ambient.g ||
	   d3dmaterial.Ambient.b != mat9->Ambient.b ||
	   d3dmaterial.Ambient.a != mat9->Ambient.a ||
	   d3dmaterial.Specular.r != mat9->Specular.r ||
	   d3dmaterial.Specular.g != mat9->Specular.g ||
	   d3dmaterial.Specular.b != mat9->Specular.b ||
	   d3dmaterial.Specular.a != mat9->Specular.a ||
	   d3dmaterial.Emissive.r != mat9->Emissive.r ||
	   d3dmaterial.Emissive.g != mat9->Emissive.g ||
	   d3dmaterial.Emissive.b != mat9->Emissive.b ||
	   d3dmaterial.Emissive.a != mat9->Emissive.a ||
	   d3dmaterial.Power != mat9->Power){
		d3ddevice->SetMaterial(mat9);
		d3dmaterial = *mat9;
	}
}

void
setMaterial(SurfaceProperties surfProps, rw::RGBA color)
{
	D3DMATERIAL9 mat9;
	D3DCOLORVALUE black = { 0, 0, 0, 0 };
	float ambmult = surfProps.ambient/255.0f;
	float diffmult = surfProps.diffuse/255.0f;
	mat9.Ambient.r = color.red*ambmult;
	mat9.Ambient.g = color.green*ambmult;
	mat9.Ambient.b = color.blue*ambmult;
	mat9.Ambient.a = color.alpha*ambmult;
	mat9.Diffuse.r = color.red*diffmult;
	mat9.Diffuse.g = color.green*diffmult;
	mat9.Diffuse.b = color.blue*diffmult;
	mat9.Diffuse.a = color.alpha*diffmult;
	mat9.Power = 0.0f;
	mat9.Emissive = black;
	mat9.Specular = black;
	setD3dMaterial(&mat9);
}

static void
setRwRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	uint32 bval = value ? TRUE : FALSE;
	switch(state){
	case TEXTURERASTER:
		setRasterStage(0, (Raster*)pvalue);
		break;
	case TEXTUREADDRESS:
		setAddressU(0, value);
		setAddressV(0, value);
		break;
	case TEXTUREADDRESSU:
		setAddressU(0, value);
		break;
	case TEXTUREADDRESSV:
		setAddressV(0, value);
		break;
	case TEXTUREFILTER:
		setFilterMode(0, value);
		break;
	case VERTEXALPHA:
		setVertexAlpha(bval);
		break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			setRenderState(D3DRS_SRCBLEND, blendMap[value]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			setRenderState(D3DRS_DESTBLEND, blendMap[value]);
		}
		break;
	case ZTESTENABLE:
		setDepthTest(bval);
		break;
	case ZWRITEENABLE:
		setDepthWrite(bval);
		break;
	case FOGENABLE:
		if(rwStateCache.fogenable != bval){
			rwStateCache.fogenable = bval;
			setRenderState(D3DRS_FOGENABLE, rwStateCache.fogenable);
		};
		break;
	case FOGCOLOR:{
		RGBA c;
		c.red = value;
		c.green = value>>8;
		c.blue = value>>16;
		c.alpha = value>>24;
		if(!equal(rwStateCache.fogcolor, c)){
			rwStateCache.fogcolor = c;
			setRenderState(D3DRS_FOGCOLOR, D3DCOLOR_RGBA(c.red, c.green, c.blue, c.alpha));
		}} break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			setRenderState(D3DRS_CULLMODE, cullmodeMap[value]);
		}
		break;
	case ALPHATESTFUNC:
		if(rwStateCache.alphafunc != value){
			rwStateCache.alphafunc = value;
			setRenderState(D3DRS_ALPHAFUNC, alphafuncMap[rwStateCache.alphafunc]);
		}
		break;
	case ALPHATESTREF:
		if(rwStateCache.alpharef != value){
			rwStateCache.alpharef = value;
			setRenderState(D3DRS_ALPHAREF, rwStateCache.alpharef);
		}
		break;
	}
}

static void*
getRwRenderState(int32 state)
{
	uint32 val;
	switch(state){
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSV:
		val = rwStateCache.texstage[0].addressingV;
		break;
	case TEXTUREFILTER:
		val = rwStateCache.texstage[0].filter;
		break;

	case VERTEXALPHA:
		val = rwStateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = rwStateCache.srcblend;
		break;
	case DESTBLEND:
		val = rwStateCache.destblend;
		break;
	case ZTESTENABLE:
		val = rwStateCache.ztest;
		break;
	case ZWRITEENABLE:
		val = rwStateCache.zwrite;
		break;
	case FOGENABLE:
		val = rwStateCache.fogenable;
		break;
	case FOGCOLOR:
		val = RWRGBAINT(rwStateCache.fogcolor.red, rwStateCache.fogcolor.green,
			rwStateCache.fogcolor.blue, rwStateCache.fogcolor.alpha);
		break;
	case CULLMODE:
		val = rwStateCache.cullmode;
		break;
	case ALPHATESTFUNC:
		val = rwStateCache.alphafunc;
		break;
	case ALPHATESTREF:
		val = rwStateCache.alpharef;
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}

// Shaders

void
setVertexShader(void *vs)
{
	d3ddevice->SetVertexShader((IDirect3DVertexShader9*)vs);
}

void
setPixelShader(void *ps)
{
	d3ddevice->SetPixelShader((IDirect3DPixelShader9*)ps);
}

void*
createVertexShader(void *csosrc)
{
	void *shdr;
	if(d3ddevice->CreateVertexShader((DWORD*)csosrc, (IDirect3DVertexShader9**)&shdr) == D3D_OK)
		return shdr;
	return nil;
}

void*
createPixelShader(void *csosrc)
{
	void *shdr;
	if(d3ddevice->CreatePixelShader((DWORD*)csosrc, (IDirect3DPixelShader9**)&shdr) == D3D_OK)
		return shdr;
	return nil;
}

// Camera

static void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];

	// View Matrix
	Matrix inv;
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
	memcpy(&cam->devView, view, sizeof(RawMatrix));
	d3ddevice->SetTransform(D3DTS_VIEW, (D3DMATRIX*)view);

	// Projection Matrix
	float32 invwx = 1.0f/cam->viewWindow.x;
	float32 invwy = 1.0f/cam->viewWindow.y;
	float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = cam->farPlane*invz;
		proj[11] = 1.0f;

		proj[15] = 0.0f;
	}else{
		proj[10] = invz;
		proj[11] = 0.0f;

		proj[15] = 1.0f;
	}
	proj[14] = -cam->nearPlane*proj[10];
	memcpy(&cam->devProj, proj, sizeof(RawMatrix));
	d3ddevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)proj);

	// TODO: figure out where this is really done
	setRenderState(D3DRS_FOGSTART, *(uint32*)&cam->fogPlane);
	setRenderState(D3DRS_FOGEND, *(uint32*)&cam->farPlane);

	D3DVIEWPORT9 vp;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	vp.X = cam->frameBuffer->offsetX;
	vp.Y = cam->frameBuffer->offsetY;
	vp.Width = cam->frameBuffer->width;
	vp.Height = cam->frameBuffer->height;
	d3ddevice->SetViewport(&vp);

	// TODO: figure out when to call this
	d3ddevice->BeginScene();
}

static void
endUpdate(Camera *cam)
{
	// TODO: figure out when to call this
	d3ddevice->EndScene();
}

void
addVidmemRaster(Raster *raster)
{
	VidmemRaster *vmr = rwNewT(VidmemRaster, 1, ID_DRIVER | MEMDUR_EVENT);
	vmr->raster = raster;
	vmr->next = vidmemRasters;
	vidmemRasters = vmr;
}

void
removeVidmemRaster(Raster *raster)
{
	VidmemRaster **p, *vmr;
	for(p = &vidmemRasters; *p; p = &(*p)->next)
		if((*p)->raster == raster)
			goto found;
	return;
found:
	vmr = *p;
	*p = vmr->next;
	rwFree(vmr);
}

static void
releaseVidmemRasters(void)
{
	VidmemRaster *vmr;
	Raster *raster;
	D3dRaster *natras;
	for(vmr = vidmemRasters; vmr; vmr = vmr->next){
		raster = vmr->raster;
		natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
		if(raster->type == Raster::CAMERATEXTURE){
			deleteObject(natras->texture);
			natras->texture = nil;
		}
	}
}

static void
recreateVidmemRasters(void)
{
	VidmemRaster *vmr;
	Raster *raster;
	D3dRaster *natras;
	for(vmr = vidmemRasters; vmr; vmr = vmr->next){
		raster = vmr->raster;
		natras = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
		if(raster->type == Raster::CAMERATEXTURE){
			int32 levels = Raster::calculateNumLevels(raster->width, raster->height);
			IDirect3DTexture9 *tex;
			d3ddevice->CreateTexture(raster->width, raster->height,
						raster->format & Raster::MIPMAP ? levels : 1,
						D3DUSAGE_RENDERTARGET,
						(D3DFORMAT)natras->format, D3DPOOL_DEFAULT, &tex, nil);
			natras->texture = tex;
		}
	}
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	int flags = 0;
	if(mode & Camera::CLEARIMAGE)
		mode |= D3DCLEAR_TARGET;
	if(mode & Camera::CLEARZ)
		mode |= D3DCLEAR_ZBUFFER;
	D3DCOLOR c = D3DCOLOR_RGBA(col->red, col->green, col->blue, col->alpha);

	RECT r;
	GetClientRect(d3d9Globals.window, &r);
	BOOL icon = IsIconic(d3d9Globals.window);
	Raster *ras = cam->frameBuffer;
	if(!icon &&
	   (r.right != d3d9Globals.present.BackBufferWidth || r.bottom != d3d9Globals.present.BackBufferHeight)){
		releaseVidmemRasters();

		d3d9Globals.present.BackBufferWidth  = r.right;
		d3d9Globals.present.BackBufferHeight = r.bottom;
		// TODO: check result
		d3d::d3ddevice->Reset(&d3d9Globals.present);
		resetD3d9Device();
	}

	d3ddevice->Clear(0, 0, mode, c, 1.0f, 0);
}

static void
showRaster(Raster *raster)
{
	// TODO: do this properly!

	// not used but we want cameras to have rasters
	assert(raster);
	d3ddevice->Present(nil, nil, 0, nil);
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	IDirect3DTexture9 *dsttex;
	IDirect3DSurface9 *srcsurf, *dstsurf;
	D3DSURFACE_DESC srcdesc, dstdesc;
	RECT rect = { x, y, x, y };

	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	D3dRaster *natdst = PLUGINOFFSET(D3dRaster, dst, nativeRasterOffset);
	D3dRaster *natsrc = PLUGINOFFSET(D3dRaster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			dsttex = (IDirect3DTexture9*)natdst->texture;
			dsttex->GetSurfaceLevel(0, &dstsurf);
			assert(dstsurf);
			dstsurf->GetDesc(&dstdesc);

			d3ddevice->GetRenderTarget(0, &srcsurf);
			assert(srcsurf);
			srcsurf->GetDesc(&srcdesc);

			rect.right += srcdesc.Width;
			rect.bottom += srcdesc.Height;

			d3ddevice->StretchRect(srcsurf, &rect, dstsurf, &rect, D3DTEXF_NONE);
			dstsurf->Release();
			srcsurf->Release();
			return 1;
		}
		break;
	}

	return 0;
}


//
// Device
//

int
findFormatDepth(uint32 format)
{
	// not all formats actually
	switch(format){
	case D3DFMT_R8G8B8:	return 24;
	case D3DFMT_A8R8G8B8:	return 32;
	case D3DFMT_X8R8G8B8:	return 32;
	case D3DFMT_R5G6B5:	return 16;
	case D3DFMT_X1R5G5B5:	return 16;
	case D3DFMT_A1R5G5B5:	return 16;
	case D3DFMT_A4R4G4B4:	return 16;
	case D3DFMT_R3G3B2:	return 8;
	case D3DFMT_A8:	return 8;
	case D3DFMT_A8R3G3B2:	return 16;
	case D3DFMT_X4R4G4B4:	return 16;
	case D3DFMT_A2B10G10R10:	return 32;
	case D3DFMT_A8B8G8R8:	return 32;
	case D3DFMT_X8B8G8R8:	return 32;
	case D3DFMT_G16R16:	return 32;
	case D3DFMT_A2R10G10B10:	return 32;
	case D3DFMT_A16B16G16R16:	return 64;

	case D3DFMT_L8:	return 8;
	case D3DFMT_D16:	return 16;
	case D3DFMT_D24X8:	return 32;
	case D3DFMT_D32:	return 32;

	default:	return 0;
	}
}

// the commented ones don't "work"
static D3DFORMAT fbFormats[] = {
//	D3DFMT_A1R5G5B5,
///	D3DFMT_A2R10G10B10,	// works but let's not use it...
//	D3DFMT_A8R8G8B8,
	D3DFMT_R5G6B5,
//	D3DFMT_X1R5G5B5,
	D3DFMT_X8R8G8B8
};

static void
addVideoMode(D3DDISPLAYMODE *mode)
{
	int i;

	for(i = 1; i < d3d9Globals.numModes; i++){
		if(d3d9Globals.modes[i].mode.Width == mode->Width &&
		   d3d9Globals.modes[i].mode.Height == mode->Height &&
		   d3d9Globals.modes[i].mode.Format == mode->Format){
			// had this format already, remember highest refresh rate
			if(mode->RefreshRate > d3d9Globals.modes[i].mode.RefreshRate)
				d3d9Globals.modes[i].mode.RefreshRate = mode->RefreshRate;
			return;
		}
	}

	// none found, add
	d3d9Globals.modes[d3d9Globals.numModes].mode = *mode;
	d3d9Globals.modes[d3d9Globals.numModes].flags = VIDEOMODEEXCLUSIVE;
	d3d9Globals.numModes++;
}

static void
makeVideoModeList(void)
{
	int i, j;
	D3DDISPLAYMODE mode;

	d3d9Globals.numModes = 1;
	for(i = 0; i < nelem(fbFormats); i++)
		d3d9Globals.numModes += d3d9Globals.d3d9->GetAdapterModeCount(d3d9Globals.adapter, fbFormats[i]);

	rwFree(d3d9Globals.modes);
	d3d9Globals.modes = rwNewT(DisplayMode, d3d9Globals.numModes, ID_DRIVER | MEMDUR_EVENT);

	// first mode is current mode as windowed
	d3d9Globals.d3d9->GetAdapterDisplayMode(d3d9Globals.adapter, &d3d9Globals.modes[0].mode);
	d3d9Globals.modes[0].flags = 0;
	d3d9Globals.numModes = 1;

	for(i = 0; i < nelem(fbFormats); i++){
		int n = d3d9Globals.d3d9->GetAdapterModeCount(d3d9Globals.adapter, fbFormats[i]);
		for(j = 0; j < n; j++){
			d3d9Globals.d3d9->EnumAdapterModes(d3d9Globals.adapter, fbFormats[i], j, &mode);
			addVideoMode(&mode);
		}
	}
}

static int
openD3D(EngineOpenParams *params)
{
	HWND win = params->window;

	d3d9Globals.window = win;
	d3d9Globals.numAdapters = 0;
	d3d9Globals.modes = nil;
	d3d9Globals.numModes = 0;
	d3d9Globals.currentMode = 0;

	d3d9Globals.d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if(d3d9Globals.d3d9 == nil){
		RWERROR((ERR_GENERAL, "Direct3DCreate9() failed"));
		return 0;
	}

	d3d9Globals.numAdapters = d3d9Globals.d3d9->GetAdapterCount();
	d3d9Globals.adapter = 0;

	for(d3d9Globals.adapter = 0; d3d9Globals.adapter < d3d9Globals.numAdapters; d3d9Globals.adapter++)
		if(d3d9Globals.d3d9->GetDeviceCaps(d3d9Globals.adapter, D3DDEVTYPE_HAL, &d3d9Globals.caps) == D3D_OK)
			goto found;
	// no adapter
	d3d9Globals.d3d9->Release();
	d3d9Globals.d3d9 = nil;
	RWERROR((ERR_GENERAL, "Direct3DCreate9() failed"));
	return 0;

found:
	makeVideoModeList();
	return 1;
}

static int
closeD3D(void)
{
	d3d9Globals.d3d9->Release();
	d3d9Globals.d3d9 = nil;
	return 1;
}

static int
startD3D(void)
{
	HRESULT hr;
	int vp;
	if(d3d9Globals.caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	uint32 width, height, depth;
	D3DFORMAT format, zformat;
	format = d3d9Globals.modes[d3d9Globals.currentMode].mode.Format;

	// Use window size in windowed mode, otherwise get size from video mode
	if(d3d9Globals.modes[d3d9Globals.currentMode].flags & VIDEOMODEEXCLUSIVE){
		width = d3d9Globals.modes[d3d9Globals.currentMode].mode.Width;
		height = d3d9Globals.modes[d3d9Globals.currentMode].mode.Height;
	}else{
		RECT rect;
		GetClientRect(d3d9Globals.window, &rect);
		width = rect.right - rect.left;
		height = rect.bottom - rect.top;
	}

	bool windowed = !(d3d9Globals.modes[d3d9Globals.currentMode].flags & VIDEOMODEEXCLUSIVE);

	// See if we can get an alpha channel
	if(format == D3DFMT_X8R8G8B8){
		if(d3d9Globals.d3d9->CheckDeviceType(d3d9Globals.adapter, D3DDEVTYPE_HAL, format, D3DFMT_A8R8G8B8, windowed) == D3D_OK)
			format = D3DFMT_A8R8G8B8;
	}

	depth = findFormatDepth(format);

	// TOOD: use something more sophisticated maybe?
	if(depth == 32)
		zformat = D3DFMT_D24S8;
	else
		zformat = D3DFMT_D16;

	d3d9Globals.present.BackBufferWidth            = width;
	d3d9Globals.present.BackBufferHeight           = height;
	d3d9Globals.present.BackBufferFormat           = format;
	d3d9Globals.present.BackBufferCount            = 1;
	d3d9Globals.present.MultiSampleType            = D3DMULTISAMPLE_NONE;
	d3d9Globals.present.MultiSampleQuality         = 0;
	d3d9Globals.present.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
	d3d9Globals.present.hDeviceWindow              = d3d9Globals.window;
	d3d9Globals.present.Windowed                   = windowed;
	d3d9Globals.present.EnableAutoDepthStencil     = true;
	d3d9Globals.present.AutoDepthStencilFormat     = zformat;
	d3d9Globals.present.Flags                      = 0;
	d3d9Globals.present.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	d3d9Globals.present.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;
//	d3d9Globals.present.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;

	IDirect3DDevice9 *dev;
	hr = d3d9Globals.d3d9->CreateDevice(d3d9Globals.adapter, D3DDEVTYPE_HAL,
			d3d9Globals.window, vp, &d3d9Globals.present, &dev);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "CreateDevice() failed"));
		return 0;
	}
	d3d::d3ddevice = dev;
	return 1;
}

static int
initD3D(void)
{
	int32 s, t;

	// TODO: do some real stuff here

	d3ddevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	rwStateCache.alphafunc = ALPHAGREATEREQUAL;
	d3ddevice->SetRenderState(D3DRS_ALPHAREF, 10);
	rwStateCache.alpharef = 10;

	d3ddevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	rwStateCache.fogenable = 0;
	d3ddevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	// TODO: more fog stuff

	d3ddevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	rwStateCache.cullmode = CULLNONE;

	d3ddevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3ddevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	rwStateCache.srcblend = BLENDSRCALPHA;
	d3ddevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	rwStateCache.destblend = BLENDINVSRCALPHA;
	d3ddevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;

	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
//	setTextureStageState(0, D3DTSS_CONSTANT, 0xFFFFFFFF);
//	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
//	setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CONSTANT);
//	setTextureStageState(0, D3DTSS_COLOROP, D3DTA_CONSTANT);

	// These states exist, not all do
	validStates[D3DRS_ZENABLE] = 1;
	validStates[D3DRS_FILLMODE] = 1;
	validStates[D3DRS_SHADEMODE] = 1;
	validStates[D3DRS_ZWRITEENABLE] = 1;
	validStates[D3DRS_ALPHATESTENABLE] = 1;
	validStates[D3DRS_LASTPIXEL] = 1;
	validStates[D3DRS_SRCBLEND] = 1;
	validStates[D3DRS_DESTBLEND] = 1;
	validStates[D3DRS_CULLMODE] = 1;
	validStates[D3DRS_ZFUNC] = 1;
	validStates[D3DRS_ALPHAREF] = 1;
	validStates[D3DRS_ALPHAFUNC] = 1;
	validStates[D3DRS_DITHERENABLE] = 1;
	validStates[D3DRS_ALPHABLENDENABLE] = 1;
	validStates[D3DRS_FOGENABLE] = 1;
	validStates[D3DRS_SPECULARENABLE] = 1;
	validStates[D3DRS_FOGCOLOR] = 1;
	validStates[D3DRS_FOGTABLEMODE] = 1;
	validStates[D3DRS_FOGSTART] = 1;
	validStates[D3DRS_FOGEND] = 1;
	validStates[D3DRS_FOGDENSITY] = 1;
	validStates[D3DRS_RANGEFOGENABLE] = 1;
	validStates[D3DRS_STENCILENABLE] = 1;
	validStates[D3DRS_STENCILFAIL] = 1;
	validStates[D3DRS_STENCILZFAIL] = 1;
	validStates[D3DRS_STENCILPASS] = 1;
	validStates[D3DRS_STENCILFUNC] = 1;
	validStates[D3DRS_STENCILREF] = 1;
	validStates[D3DRS_STENCILMASK] = 1;
	validStates[D3DRS_STENCILWRITEMASK] = 1;
	validStates[D3DRS_TEXTUREFACTOR] = 1;
	validStates[D3DRS_WRAP0] = 1;
	validStates[D3DRS_WRAP1] = 1;
	validStates[D3DRS_WRAP2] = 1;
	validStates[D3DRS_WRAP3] = 1;
	validStates[D3DRS_WRAP4] = 1;
	validStates[D3DRS_WRAP5] = 1;
	validStates[D3DRS_WRAP6] = 1;
	validStates[D3DRS_WRAP7] = 1;
	validStates[D3DRS_CLIPPING] = 1;
	validStates[D3DRS_LIGHTING] = 1;
	validStates[D3DRS_AMBIENT] = 1;
	validStates[D3DRS_FOGVERTEXMODE] = 1;
	validStates[D3DRS_COLORVERTEX] = 1;
	validStates[D3DRS_LOCALVIEWER] = 1;
	validStates[D3DRS_NORMALIZENORMALS] = 1;
	validStates[D3DRS_DIFFUSEMATERIALSOURCE] = 1;
	validStates[D3DRS_SPECULARMATERIALSOURCE] = 1;
	validStates[D3DRS_AMBIENTMATERIALSOURCE] = 1;
	validStates[D3DRS_EMISSIVEMATERIALSOURCE] = 1;
	validStates[D3DRS_VERTEXBLEND] = 1;
	validStates[D3DRS_CLIPPLANEENABLE] = 1;
	validStates[D3DRS_POINTSIZE] = 1;
	validStates[D3DRS_POINTSIZE_MIN] = 1;
	validStates[D3DRS_POINTSPRITEENABLE] = 1;
	validStates[D3DRS_POINTSCALEENABLE] = 1;
	validStates[D3DRS_POINTSCALE_A] = 1;
	validStates[D3DRS_POINTSCALE_B] = 1;
	validStates[D3DRS_POINTSCALE_C] = 1;
	validStates[D3DRS_MULTISAMPLEANTIALIAS] = 1;
	validStates[D3DRS_MULTISAMPLEMASK] = 1;
	validStates[D3DRS_PATCHEDGESTYLE] = 1;
	validStates[D3DRS_DEBUGMONITORTOKEN] = 1;
	validStates[D3DRS_POINTSIZE_MAX] = 1;
	validStates[D3DRS_INDEXEDVERTEXBLENDENABLE] = 1;
	validStates[D3DRS_COLORWRITEENABLE] = 1;
	validStates[D3DRS_TWEENFACTOR] = 1;
	validStates[D3DRS_BLENDOP] = 1;
	validStates[D3DRS_POSITIONDEGREE] = 1;
	validStates[D3DRS_NORMALDEGREE] = 1;
	validStates[D3DRS_SCISSORTESTENABLE] = 1;
	validStates[D3DRS_SLOPESCALEDEPTHBIAS] = 1;
	validStates[D3DRS_ANTIALIASEDLINEENABLE] = 1;
	validStates[D3DRS_MINTESSELLATIONLEVEL] = 1;
	validStates[D3DRS_MAXTESSELLATIONLEVEL] = 1;
	validStates[D3DRS_ADAPTIVETESS_X] = 1;
	validStates[D3DRS_ADAPTIVETESS_Y] = 1;
	validStates[D3DRS_ADAPTIVETESS_Z] = 1;
	validStates[D3DRS_ADAPTIVETESS_W] = 1;
	validStates[D3DRS_ENABLEADAPTIVETESSELLATION] = 1;
	validStates[D3DRS_TWOSIDEDSTENCILMODE] = 1;
	validStates[D3DRS_CCW_STENCILFAIL] = 1;
	validStates[D3DRS_CCW_STENCILZFAIL] = 1;
	validStates[D3DRS_CCW_STENCILPASS] = 1;
	validStates[D3DRS_CCW_STENCILFUNC] = 1;
	validStates[D3DRS_COLORWRITEENABLE1] = 1;
	validStates[D3DRS_COLORWRITEENABLE2] = 1;
	validStates[D3DRS_COLORWRITEENABLE3] = 1;
	validStates[D3DRS_BLENDFACTOR] = 1;
	validStates[D3DRS_SRGBWRITEENABLE] = 1;
	validStates[D3DRS_DEPTHBIAS] = 1;
	validStates[D3DRS_WRAP8] = 1;
	validStates[D3DRS_WRAP9] = 1;
	validStates[D3DRS_WRAP10] = 1;
	validStates[D3DRS_WRAP11] = 1;
	validStates[D3DRS_WRAP12] = 1;
	validStates[D3DRS_WRAP13] = 1;
	validStates[D3DRS_WRAP14] = 1;
	validStates[D3DRS_WRAP15] = 1;
	validStates[D3DRS_SEPARATEALPHABLENDENABLE] = 1;
	validStates[D3DRS_SRCBLENDALPHA] = 1;
	validStates[D3DRS_DESTBLENDALPHA] = 1;
	validStates[D3DRS_BLENDOPALPHA] = 1;

	validTexStates[D3DTSS_COLOROP] = 1;
	validTexStates[D3DTSS_COLORARG1] = 1;
	validTexStates[D3DTSS_COLORARG2] = 1;
	validTexStates[D3DTSS_ALPHAOP] = 1;
	validTexStates[D3DTSS_ALPHAARG1] = 1;
	validTexStates[D3DTSS_ALPHAARG2] = 1;
	validTexStates[D3DTSS_BUMPENVMAT00] = 1;
	validTexStates[D3DTSS_BUMPENVMAT01] = 1;
	validTexStates[D3DTSS_BUMPENVMAT10] = 1;
	validTexStates[D3DTSS_BUMPENVMAT11] = 1;
	validTexStates[D3DTSS_TEXCOORDINDEX] = 1;
	validTexStates[D3DTSS_BUMPENVLSCALE] = 1;
	validTexStates[D3DTSS_BUMPENVLOFFSET] = 1;
	validTexStates[D3DTSS_TEXTURETRANSFORMFLAGS] = 1;
	validTexStates[D3DTSS_COLORARG0] = 1;
	validTexStates[D3DTSS_ALPHAARG0] = 1;
	validTexStates[D3DTSS_RESULTARG] = 1;
	validTexStates[D3DTSS_CONSTANT] = 1;

	// Save the current states
	for(s = 0; s < MAXNUMSTATES; s++)
		if(validStates[s]){
			d3ddevice->GetRenderState((D3DRENDERSTATETYPE)s, (DWORD*)&d3dStates[s]);
			stateCache[s].value = d3dStates[s];
		}
	for(t = 0; t < MAXNUMTEXSTATES; t++)
		if(validTexStates[t])
			for(s = 0; s < MAXNUMSTAGES; s++){
				d3ddevice->GetTextureStageState(s, (D3DTEXTURESTAGESTATETYPE)t, (DWORD*)&d3dTextureStageStates[t][s]);
				textureStageStateCache[t][s].value = d3dTextureStageStates[t][s];
			}
	for(t = 1; t < MAXNUMSAMPLERSTATES; t++)
		for(s = 0; s < MAXNUMSTAGES; s++){
			d3ddevice->GetSamplerState(s, (D3DSAMPLERSTATETYPE)t, (DWORD*)&d3dSamplerStates[t][s]);
			d3dSamplerStates[t][s] = d3dSamplerStates[t][s];
		}

	openIm2D();
	openIm3D();

	return 1;
}

static int
termD3D(void)
{
	closeIm3D();
	closeIm2D();

	d3d::d3ddevice->Release();
	d3d::d3ddevice = nil;
	return 1;
}

static int
finalizeD3D(void)
{
	return 1;
}

static int
deviceSystem(DeviceReq req, void *arg, int32 n)
{
	D3DADAPTER_IDENTIFIER9 adapter;
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openD3D((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeD3D();

	case DEVICEINIT:
		return startD3D() && initD3D();
	case DEVICETERM:
		return termD3D();

	case DEVICEFINALIZE:
		return finalizeD3D();


	case DEVICEGETNUMSUBSYSTEMS:
		return d3d9Globals.numAdapters;

	case DEVICEGETCURRENTSUBSYSTEM:
		return d3d9Globals.adapter;

	case DEVICESETSUBSYSTEM:
		if(n >= d3d9Globals.numAdapters)
			return 0;
		d3d9Globals.adapter = n;
		if(d3d9Globals.d3d9->GetDeviceCaps(d3d9Globals.adapter, D3DDEVTYPE_HAL, &d3d9Globals.caps) != D3D_OK)
			return 0;
		makeVideoModeList();
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		if(n >= d3d9Globals.numAdapters)
			return 0;
		if(d3d9Globals.d3d9->GetAdapterIdentifier(d3d9Globals.adapter, 0, &adapter) != D3D_OK)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, adapter.Description, sizeof(SubSystemInfo::name));
		return 1;


	case DEVICEGETNUMVIDEOMODES:
		return d3d9Globals.numModes;

	case DEVICEGETCURRENTVIDEOMODE:
		return d3d9Globals.currentMode;

	case DEVICESETVIDEOMODE:
		if(n >= d3d9Globals.numModes)
			return 0;
		d3d9Globals.currentMode = n;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = d3d9Globals.modes[n].mode.Width;
		rwmode->height = d3d9Globals.modes[n].mode.Height;
		rwmode->depth = findFormatDepth(d3d9Globals.modes[n].mode.Format);
		rwmode->flags = d3d9Globals.modes[n].flags;
		return 1;
	}
	return 1;
}

Device renderdevice = {
	0.0f, 1.0f,
	d3d::beginUpdate,
	d3d::endUpdate,
	d3d::clearCamera,
	d3d::showRaster,
	d3d::rasterRenderFast,
	d3d::setRwRenderState,
	d3d::getRwRenderState,
	d3d::im2DRenderLine,
	d3d::im2DRenderTriangle,
	d3d::im2DRenderPrimitive,
	d3d::im2DRenderIndexedPrimitive,
	d3d::im3DTransform,
	d3d::im3DRenderIndexed,
	d3d::im3DEnd,
	d3d::deviceSystem,
};

#endif
}
}
