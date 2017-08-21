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

namespace rw {
namespace d3d {

#ifdef RW_D3D9
IDirect3DDevice9 *d3ddevice = nil;

#define MAX_LIGHTS 8

void
lightingCB(void)
{
	World *world;
	RGBAf ambLight = { 0.0, 0.0, 0.0, 1.0 };
	RGBA amb;
	D3DLIGHT9 light;
	light.Type = D3DLIGHT_DIRECTIONAL;
	//light.Diffuse =  { 0.8f, 0.8f, 0.8f, 1.0f };
	light.Specular = { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Ambient =  { 0.0f, 0.0f, 0.0f, 0.0f };
	light.Position = { 0.0f, 0.0f, 0.0f };
	//light.Direction = { 0.0f, 0.0f, -1.0f };
	light.Range = 0.0f;
	light.Falloff = 0.0f;
	light.Attenuation0 = 0.0f;
	light.Attenuation1 = 0.0f;
	light.Attenuation2 = 0.0f;
	light.Theta = 0.0f;
	light.Phi = 0.0f;
	int n = 0;

	world = (World*)engine->currentWorld;
	// only unpositioned lights right now
	FORLIST(lnk, world->directionalLights){
		Light *l = Light::fromWorld(lnk);
		if(l->getType() == Light::DIRECTIONAL){
			if(n >= MAX_LIGHTS)
				continue;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
		}else if(l->getType() == Light::AMBIENT){
			ambLight.red   += l->color.red;
			ambLight.green += l->color.green;
			ambLight.blue  += l->color.blue;
		}
	}
	for(; n < MAX_LIGHTS; n++)
		d3ddevice->LightEnable(n, FALSE);
	convColor(&amb, &ambLight);
	d3d::setRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(amb.red, amb.green, amb.blue, amb.alpha));
}

#endif

}
}
