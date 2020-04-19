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
#include "rwd3d.h"

namespace rw {
namespace d3d {

#ifdef RW_D3D9
IDirect3DDevice9 *d3ddevice = nil;

#define MAX_LIGHTS 8

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

	((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);

	int i, n;
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

	convColor(&amb, &lightData.ambient);
	d3d::setRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(amb.red, amb.green, amb.blue, amb.alpha));

	n = 0;
	for(i = 0; i < lightData.numDirectionals; i++){
		if(n >= MAX_LIGHTS)
			return;
		Light *l = lightData.directionals[i];
		light.Type = D3DLIGHT_DIRECTIONAL;
		light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
		light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
		d3ddevice->SetLight(n, &light);
		d3ddevice->LightEnable(n, TRUE);
		n++;
	}

	for(i = 0; i < lightData.numLocals; i++){
		if(n >= MAX_LIGHTS)
			return;
		Light *l = lightData.locals[i];
		switch(l->getType()){
		case Light::POINT:
			light.Type = D3DLIGHT_POINT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction.x = 0.0f;
			light.Direction.y = 0.0f;
			light.Direction.z = 0.0f;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;

		case Light::SPOT:
			light.Type = D3DLIGHT_SPOT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			light.Theta = l->getAngle()*2.0f;
			light.Phi = light.Theta;
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;

		case Light::SOFTSPOT:
			light.Type = D3DLIGHT_SPOT;
			light.Diffuse =  *(D3DCOLORVALUE*)&l->color;
			light.Position = *(D3DVECTOR*)&l->getFrame()->getLTM()->pos;
			light.Direction = *(D3DVECTOR*)&l->getFrame()->getLTM()->at;
			light.Range = l->radius;
			light.Falloff = 1.0f;
			light.Attenuation0 = 1.0f;
			light.Attenuation1 = 0.0f/l->radius;
			light.Attenuation2 = 5.0f/(l->radius*l->radius);
			light.Theta = 0.0f;
			light.Phi = l->getAngle()*2.0f;
			d3ddevice->SetLight(n, &light);
			d3ddevice->LightEnable(n, TRUE);
			n++;
			break;
		}
	}

	for(; n < MAX_LIGHTS; n++)
		d3ddevice->LightEnable(n, FALSE);
}

#endif

}
}
