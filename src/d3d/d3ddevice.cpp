#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwerror.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d {

#ifdef RW_D3D9

// cached RW render states
static bool32 vertexAlpha;
static bool32 textureAlpha;
static uint32 srcblend, destblend;
static uint32 zwrite;
static uint32 ztest;
static uint32 fogenable;
static RGBA fogcolor;
static uint32 alphafunc;
static uint32 alpharef;


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

// TODO: not only rasters, make a struct
static Raster *d3dRaster[MAXNUMSTAGES];

static D3DMATERIAL9 d3dmaterial;

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

// RW render state

static void
setVertexAlpha(bool32 enable)
{
	if(vertexAlpha != enable){
		if(!textureAlpha){
			setRenderState(D3DRS_ALPHABLENDENABLE, enable);
			setRenderState(D3DRS_ALPHATESTENABLE, enable);
		}
		vertexAlpha = enable;
	}
}

static uint32 blendMap[] = {
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

uint32 alphafuncMap[] = {
	D3DCMP_ALWAYS,
	D3DCMP_GREATEREQUAL,
	D3DCMP_LESS
};

static void
setRwRenderState(int32 state, uint32 value)
{
	uint32 bval = value ? TRUE : FALSE;
	switch(state){
	case VERTEXALPHA:
		setVertexAlpha(bval);
		break;
	case SRCBLEND:
		if(srcblend != value){
			srcblend = value;
			setRenderState(D3DRS_SRCBLEND, blendMap[value]);
		}
		break;
	case DESTBLEND:
		if(destblend != value){
			destblend = value;
			setRenderState(D3DRS_DESTBLEND, blendMap[value]);
		}
		break;
	case ZTESTENABLE:
		if(ztest != bval){
			ztest = bval;
			setRenderState(D3DRS_ZENABLE, ztest);
		}
		break;
	case ZWRITEENABLE:
		if(zwrite != bval){
			zwrite = bval;
			setRenderState(D3DRS_ZWRITEENABLE, zwrite);
		}
		break;
	case FOGENABLE:
		if(fogenable != bval){
			fogenable = bval;
			setRenderState(D3DRS_FOGENABLE, fogenable);
		};
		break;
	case FOGCOLOR:{
		RGBA c = *(RGBA*)&value;
		if(!equal(fogcolor, c)){
			fogcolor = c;
			setRenderState(D3DRS_FOGCOLOR, D3DCOLOR_RGBA(c.red, c.green, c.blue, c.alpha));
		}} break;
	case ALPHATESTFUNC:
		if(alphafunc != value){
			alphafunc = value;
			setRenderState(D3DRS_ALPHAFUNC, alphafuncMap[alphafunc]);
		}
		break;
	case ALPHATESTREF:
		if(alpharef != value){
			alpharef = value;
			setRenderState(D3DRS_ALPHAREF, alpharef);
		}
		break;
	}
}

static uint32
getRwRenderState(int32 state)
{
	switch(state){
	case VERTEXALPHA:
		return vertexAlpha;
	case SRCBLEND:
		return srcblend;
	case DESTBLEND:
		return destblend;
	case ZTESTENABLE:
		return ztest;
	case ZWRITEENABLE:
		return zwrite;
	case FOGENABLE:
		return fogenable;
	case FOGCOLOR:
		return *(uint32*)&fogcolor;
	case ALPHATESTFUNC:
		return alphafunc;
	case ALPHATESTREF:
		return alpharef;
	}
	return 0;
}

void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	D3dRaster *d3draster = nil;
	if(raster != d3dRaster[stage]){
		d3dRaster[stage] = raster;
		if(raster){
			d3draster = PLUGINOFFSET(D3dRaster, raster, nativeRasterOffset);
			d3ddevice->SetTexture(stage, (IDirect3DTexture9*)d3draster->texture);
			alpha = d3draster->hasAlpha;
		}else{
			d3ddevice->SetTexture(stage, nil);
			alpha = 0;
		}
		if(stage == 0){
			if(textureAlpha != alpha){
				textureAlpha = alpha;
				if(!vertexAlpha){
					setRenderState(D3DRS_ALPHABLENDENABLE, alpha);
					setRenderState(D3DRS_ALPHATESTENABLE, alpha);
				}
			}
		}
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
		d3ddevice->SetMaterial(&mat9);
		d3dmaterial = mat9;
	}
}

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
	d3ddevice->SetTransform(D3DTS_PROJECTION, (D3DMATRIX*)proj);

	// TODO: figure out where this is really done
	setRenderState(D3DRS_FOGSTART, *(uint32*)&cam->fogPlane);
	setRenderState(D3DRS_FOGEND, *(uint32*)&cam->farPlane);

	// TODO: figure out when to call this
	d3ddevice->BeginScene();
}

static void
endUpdate(Camera *cam)
{
	// TODO: figure out when to call this
	d3ddevice->EndScene();
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
	d3ddevice->Clear(0, 0, mode, c, 1.0f, 0);
}

static void
showRaster(Raster *raster)
{
	// TODO: do this properly!
	d3ddevice->Present(nil, nil, 0, nil);
}

// taken from Frank Luna's d3d9 book
static int
startD3D(EngineStartParams *params)
{
	HWND win = params->window;
	bool windowed = true;

	HRESULT hr = 0;
	IDirect3D9 *d3d9 = 0;
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if(!d3d9){
		RWERROR((ERR_GENERAL, "Direct3DCreate9() failed"));
		return 0;
	}

	D3DCAPS9 caps;
	d3d9->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
	int vp = 0;
	if(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	RECT rect;
	GetClientRect(win, &rect);
	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;

	D3DPRESENT_PARAMETERS d3dpp;
	d3dpp.BackBufferWidth            = width;
	d3dpp.BackBufferHeight           = height;
	d3dpp.BackBufferFormat           = D3DFMT_A8R8G8B8;
	d3dpp.BackBufferCount            = 1;
	d3dpp.MultiSampleType            = D3DMULTISAMPLE_NONE;
	d3dpp.MultiSampleQuality         = 0;
	d3dpp.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow              = win;
	d3dpp.Windowed                   = windowed;
	d3dpp.EnableAutoDepthStencil     = true;
	d3dpp.AutoDepthStencilFormat     = D3DFMT_D24S8;
	d3dpp.Flags                      = 0;
	d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
	d3dpp.PresentationInterval       = D3DPRESENT_INTERVAL_IMMEDIATE;

	IDirect3DDevice9 *dev;
	hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, win,
	                        vp, &d3dpp, &dev);
	if(FAILED(hr)){
		// try again using a 16-bit depth buffer
		d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

		hr = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		                        win, vp, &d3dpp, &dev);

		if(FAILED(hr)){
			RWERROR((ERR_GENERAL, "CreateDevice() failed"));
			d3d9->Release();
			return 0;
		}
	}
	d3d9->Release();
	d3d::d3ddevice = dev;
	return 1;
}

static int
stopD3D(void)
{
	d3d::d3ddevice->Release();
	d3d::d3ddevice = nil;
	return 1;
}

static int
initD3D(void)
{
	// TODO: do some real stuff here

	d3ddevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	alphafunc = ALPHAGREATEREQUAL;
	d3ddevice->SetRenderState(D3DRS_ALPHAREF, 10);
	alpharef = 10;

	d3ddevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
	fogenable = 0;
	d3ddevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	// TODO: more fog stuff

	d3ddevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

	d3ddevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3ddevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	srcblend = BLENDSRCALPHA;
	d3ddevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	destblend = BLENDINVSRCALPHA;
	d3ddevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
	vertexAlpha = 0;
	textureAlpha = 0;

	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
//	setTextureStageState(0, D3DTSS_CONSTANT, 0xFFFFFFFF);
//	setTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
//	setTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_CONSTANT);
//	setTextureStageState(0, D3DTSS_COLOROP, D3DTA_CONSTANT);

	return 1;
}

static int
deviceSystem(DeviceReq req, void *arg0)
{
	switch(req){
	case DEVICESTART:
		return startD3D((EngineStartParams*)arg0);
	case DEVICEINIT:
		return initD3D();
	case DEVICESTOP:
		return stopD3D();
	}
	return 1;
}

Device renderdevice = {
	0.0f, 1.0f,
	d3d::beginUpdate,
	d3d::endUpdate,
	d3d::clearCamera,
	d3d::showRaster,
	d3d::setRwRenderState,
	d3d::getRwRenderState,
	null::im2DRenderIndexedPrimitive,
	d3d::deviceSystem,
};

#endif
}
}
