#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwd3d.h"

namespace rw {
namespace d3d {
#ifdef RW_D3D9

void
setTexture(Texture *tex)
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

	D3dRaster *raster = PLUGINOFFSET(D3dRaster, tex->raster, nativeRasterOffset);
	if(tex->raster && raster->texture){
		device->SetTexture(0, (IDirect3DTexture9*)raster->texture);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, filternomip[tex->filterAddressing & 0xFF]);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, filternomip[tex->filterAddressing & 0xFF]);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, wrap[(tex->filterAddressing >> 8) & 0xF]);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, wrap[(tex->filterAddressing >> 12) & 0xF]);
	}else
		device->SetTexture(0, NULL);
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
	device->SetMaterial(&mat9);
}

#endif
}
}