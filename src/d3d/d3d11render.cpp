#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#ifdef RW_D3D11
#include <d3dcompiler.h>

#include "rwd3dimpl.h"
#include "default_VS_d11.h"
#include "default_PS_d11.h"
#endif

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifndef RW_D3D11
void defaultRenderCB(Atomic*, InstanceDataHeader*) {}
void defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header) {}
#else

struct StandardConstants
{
	float combinedMat[16];
	float worldMat[16];
	float normalMat[16];
	float matCol[4];
	float surfProps[4];
	float fogData[4];
	float ambientLight[4];
};

struct LightVS
{
	V3d color; float param0;
	V3d position; float param1;
	V3d direction; float param2;
};

#define MAX_LIGHTS 8
#define MAX_SHADER_LIGHTS (MAX_LIGHTS*2)

struct LightConstants
{
	int32 numLights[4];
	int32 firstLight[4];
	LightVS lights[MAX_SHADER_LIGHTS];
};

static ID3D11VertexShader *default_amb_VS;
static ID3D11VertexShader *default_amb_dir_VS;
static ID3D11VertexShader *default_all_VS;
static ID3D11PixelShader *default_PS;
static ID3D11InputLayout *defaultLayout;
static ID3D11Buffer *standardConstantBuffer;
static ID3D11Buffer *lightConstantBuffer;
static StandardConstants standardConstants;
static LightConstants lightConstants;

template<class T> static void
safeRelease(T *&p)
{
	if(p){
		p->Release();
		p = nil;
	}
}

static ID3DBlob*
compileShader(const char *src, const char *entry, const char *target,
	const D3D_SHADER_MACRO *defines = nil)
{
	ID3DBlob *shader = nil;
	ID3DBlob *errors = nil;
	HRESULT hr = D3DCompile(src, strlen(src), nil, defines, nil,
		entry, target, 0, 0, &shader, &errors);
	if(errors){
		fprintf(stderr, "%s\n", (const char*)errors->GetBufferPointer());
		errors->Release();
	}
	if(FAILED(hr))
		return nil;
	return shader;
}

bool32
openDefaultRenderPipeline(void)
{
	static const D3D_SHADER_MACRO ambDirDefines[] = {
		{ "DIRECTIONALS", "1" },
		{ nil, nil }
	};
	static const D3D_SHADER_MACRO allDefines[] = {
		{ "DIRECTIONALS", "1" },
		{ "POINTLIGHTS", "1" },
		{ "SPOTLIGHTS", "1" },
		{ nil, nil }
	};
	D3D11_INPUT_ELEMENT_DESC elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DefaultVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DefaultVertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(DefaultVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, offsetof(DefaultVertex, texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D11_BUFFER_DESC desc;
	ID3DBlob *ambBlob = compileShader(default_VS_d11_source, "main", "vs_4_0");
	ID3DBlob *ambDirBlob = compileShader(default_VS_d11_source, "main", "vs_4_0",
		ambDirDefines);
	ID3DBlob *allBlob = compileShader(default_VS_d11_source, "main", "vs_4_0",
		allDefines);
	ID3DBlob *psBlob = compileShader(default_PS_d11_source, "main", "ps_4_0");
	if(ambBlob == nil || ambDirBlob == nil || allBlob == nil || psBlob == nil)
		goto fail;

	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(ambBlob->GetBufferPointer(),
		ambBlob->GetBufferSize(), nil, &default_amb_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(ambDirBlob->GetBufferPointer(),
		ambDirBlob->GetBufferSize(), nil, &default_amb_dir_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(allBlob->GetBufferPointer(),
		allBlob->GetBufferSize(), nil, &default_all_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreatePixelShader(psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(), nil, &default_PS)))
		goto fail;
	d3d11Globals.numPixelShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateInputLayout(elements, nelem(elements),
		allBlob->GetBufferPointer(), allBlob->GetBufferSize(), &defaultLayout)))
		goto fail;
	d3d11Globals.numInputLayouts++;

	memset(&desc, 0, sizeof(desc));
	memset(&standardConstants, 0, sizeof(standardConstants));
	standardConstants.matCol[0] = 1.0f;
	standardConstants.matCol[1] = 1.0f;
	standardConstants.matCol[2] = 1.0f;
	standardConstants.matCol[3] = 1.0f;
	standardConstants.fogData[3] = 1.0f;
	memset(&lightConstants, 0, sizeof(lightConstants));

	desc.ByteWidth = sizeof(StandardConstants);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(d3d11Globals.d3ddevice->CreateBuffer(&desc, nil,
		&standardConstantBuffer)))
		goto fail;

	desc.ByteWidth = sizeof(LightConstants);
	if(FAILED(d3d11Globals.d3ddevice->CreateBuffer(&desc, nil,
		&lightConstantBuffer)))
		goto fail;
	d3d11Globals.context->UpdateSubresource(lightConstantBuffer, 0,
		nil, &lightConstants, 0, 0);

	ambBlob->Release();
	ambDirBlob->Release();
	allBlob->Release();
	psBlob->Release();
	return 1;

fail:
	if(ambBlob) ambBlob->Release();
	if(ambDirBlob) ambDirBlob->Release();
	if(allBlob) allBlob->Release();
	if(psBlob) psBlob->Release();
	closeDefaultRenderPipeline();
	return 0;
}

void
closeDefaultRenderPipeline(void)
{
	safeRelease(lightConstantBuffer);
	safeRelease(standardConstantBuffer);
	if(defaultLayout){
		clearVertexDeclaration(defaultLayout);
		defaultLayout->Release();
		defaultLayout = nil;
		d3d11Globals.numInputLayouts--;
	}
	if(default_PS){
		clearPixelShader(default_PS);
		default_PS->Release();
		default_PS = nil;
		d3d11Globals.numPixelShaders--;
	}
	if(default_all_VS){
		clearVertexShader(default_all_VS);
		default_all_VS->Release();
		default_all_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
	if(default_amb_dir_VS){
		clearVertexShader(default_amb_dir_VS);
		default_amb_dir_VS->Release();
		default_amb_dir_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
	if(default_amb_VS){
		clearVertexShader(default_amb_VS);
		default_amb_VS->Release();
		default_amb_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
}

static void
setAmbient(const RGBAf &color)
{
	standardConstants.ambientLight[0] = color.red;
	standardConstants.ambientLight[1] = color.green;
	standardConstants.ambientLight[2] = color.blue;
	standardConstants.ambientLight[3] = color.alpha;
}

static int32
uploadLights(WorldLights *lightData)
{
	int32 i;
	int32 bits = 0;
	int32 firstLight[4] = { 0, 0, 0, 0 };

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	LightVS directionals[MAX_LIGHTS] = {};
	LightVS points[MAX_LIGHTS] = {};
	LightVS spots[MAX_LIGHTS] = {};
	for(i = 0; i < lightData->numDirectionals; i++){
		Light *l = lightData->directionals[i];
		directionals[i].color.x = l->color.red;
		directionals[i].color.y = l->color.green;
		directionals[i].color.z = l->color.blue;
		directionals[i].direction = l->getFrame()->getLTM()->at;
		bits |= VSLIGHT_DIRECT;
	}

	int32 np = 0;
	int32 ns = 0;
	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			points[np].color.x = l->color.red;
			points[np].color.y = l->color.green;
			points[np].color.z = l->color.blue;
			points[np].param0 = l->radius;
			points[np].position = l->getFrame()->getLTM()->pos;
			np++;
			bits |= VSLIGHT_POINT;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			spots[ns].color.x = l->color.red;
			spots[ns].color.y = l->color.green;
			spots[ns].color.z = l->color.blue;
			spots[ns].param0 = l->radius;
			spots[ns].position = l->getFrame()->getLTM()->pos;
			spots[ns].direction = l->getFrame()->getLTM()->at;
			spots[ns].param1 = l->minusCosAngle;
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				spots[ns].param2 = 0.0f;
			else
				spots[ns].param2 = 1.0f;
			bits |= VSLIGHT_SPOT;
			ns++;
			break;
		}
	}

	firstLight[0] = 0;
	int32 numDir = lightData->numDirectionals;
	firstLight[1] = numDir + firstLight[0];
	int32 numPoint = np;
	firstLight[2] = numPoint + firstLight[1];
	int32 numSpot = ns;

	memset(&lightConstants, 0, sizeof(lightConstants));
	lightConstants.numLights[0] = numDir;
	lightConstants.numLights[1] = numPoint;
	lightConstants.numLights[2] = numSpot;
	memcpy(lightConstants.firstLight, firstLight, sizeof(firstLight));

	int32 off = 0;
	if(numDir)
		memcpy(&lightConstants.lights[off], directionals,
			numDir*sizeof(LightVS));
	off += numDir;
	if(numPoint)
		memcpy(&lightConstants.lights[off], points,
			numPoint*sizeof(LightVS));
	off += numPoint;
	if(numSpot)
		memcpy(&lightConstants.lights[off], spots,
			numSpot*sizeof(LightVS));

	d3d11Globals.context->UpdateSubresource(lightConstantBuffer, 0,
		nil, &lightConstants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(VSlotLights, 1,
		&lightConstantBuffer);
	return bits;
}

int32
lightingCB_Shader(Atomic *atomic)
{
	WorldLights lightData;
	Light *directionals[MAX_LIGHTS];
	Light *locals[MAX_LIGHTS];
	lightData.directionals = directionals;
	lightData.numDirectionals = MAX_LIGHTS;
	lightData.locals = locals;
	lightData.numLocals = MAX_LIGHTS;

	if(atomic->geometry->flags & Geometry::LIGHT && engine->currentWorld){
		((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
		setAmbient(lightData.ambient);
		return uploadLights(&lightData);
	}

	static const RGBAf black = { 0.0f, 0.0f, 0.0f, 0.0f };
	setAmbient(black);
	lightData.numAmbients = 0;
	lightData.numDirectionals = 0;
	lightData.numLocals = 0;
	return uploadLights(&lightData);
}

bool32
uploadDefaultMatrices(Matrix *world)
{
	if(world == nil || engine->currentCamera == nil)
		return 0;

	RawMatrix combined, worldMatrix, worldView;
	Camera *cam = engine->currentCamera;
	convMatrix(&worldMatrix, world);
	RawMatrix::mult(&worldView, &worldMatrix, &cam->devView);
	RawMatrix::mult(&combined, &worldView, &cam->devProj);

	memcpy(standardConstants.combinedMat, &combined, sizeof(combined));
	memcpy(standardConstants.worldMat, &worldMatrix, sizeof(worldMatrix));
	// D3D9 currently uploads world as the normal matrix too.
	memcpy(standardConstants.normalMat, &worldMatrix, sizeof(worldMatrix));
	return 1;
}

bool32
uploadDefaultMaterial(const RGBA &color, const SurfaceProperties &surfaceProps)
{
	if(standardConstantBuffer == nil)
		return 0;

	standardConstants.matCol[0] = color.red/255.0f;
	standardConstants.matCol[1] = color.green/255.0f;
	standardConstants.matCol[2] = color.blue/255.0f;
	standardConstants.matCol[3] = color.alpha/255.0f;
	standardConstants.surfProps[0] = surfaceProps.ambient;
	standardConstants.surfProps[1] = surfaceProps.specular;
	standardConstants.surfProps[2] = surfaceProps.diffuse;
	standardConstants.surfProps[3] = 0.0f;
	d3d11Globals.context->UpdateSubresource(standardConstantBuffer, 0,
		nil, &standardConstants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(VSlotObjects, 1,
		&standardConstantBuffer);
	return 1;
}

bool32
setDefaultVertexDeclaration(void)
{
	return setVertexDeclaration(defaultLayout);
}

bool32
setDefaultVertexShader(int32 lightBits)
{
	if((lightBits & VSLIGHT_MASK) == 0)
		return setVertexShader(default_amb_VS);
	if((lightBits & VSLIGHT_MASK) == VSLIGHT_DIRECT)
		return setVertexShader(default_amb_dir_VS);
	return setVertexShader(default_all_VS);
}

bool32
setDefaultPixelShader(void)
{
	return setPixelShader(default_PS);
}

void
drawInst_simple(d3d11::InstanceDataHeader *header, d3d11::InstanceData *inst)
{
	applyDrawState();
	d3d11Globals.context->DrawIndexed(inst->numIndex, inst->startIndex,
		inst->baseIndex);
}

// Emulate PS2 GS alpha test FB_ONLY case: failed alpha writes to frame- but not to depth buffer
void
drawInst_GSemu(d3d11::InstanceDataHeader *header, InstanceData *inst)
{
	bool32 hasAlpha = GetRenderState(VERTEXALPHA);
	Raster *raster = (Raster*)GetRenderStatePtr(TEXTURERASTER);
	if(raster && GETD3DRASTEREXT(raster)->hasAlpha)
		hasAlpha = 1;
	if(!hasAlpha){
		drawInst_simple(header, inst);
		return;
	}

	uint32 zwrite = GetRenderState(ZWRITEENABLE);
	uint32 alphafunc = GetRenderState(ALPHATESTFUNC);
	uint32 alpharef = GetRenderState(ALPHATESTREF);
	if(zwrite){
		uint32 gsalpharef = GetRenderState(GSALPHATESTREF);
		SetRenderState(ALPHATESTFUNC, ALPHAGREATEREQUAL);
		SetRenderState(ALPHATESTREF, gsalpharef);
		drawInst_simple(header, inst);
		SetRenderState(ALPHATESTFUNC, ALPHALESS);
		SetRenderState(ZWRITEENABLE, 0);
		drawInst_simple(header, inst);
		SetRenderState(ZWRITEENABLE, 1);
		SetRenderState(ALPHATESTFUNC, alphafunc);
		SetRenderState(ALPHATESTREF, alpharef);
	}else{
		SetRenderState(ALPHATESTFUNC, ALPHAALWAYS);
		drawInst_simple(header, inst);
		SetRenderState(ALPHATESTFUNC, alphafunc);
	}
}

void
drawInst(d3d11::InstanceDataHeader *header, d3d11::InstanceData *inst)
{
	if(GetRenderState(GSALPHATEST))
		drawInst_GSemu(header, inst);
	else
		drawInst_simple(header, inst);
}

void
defaultRenderCB_Shader(Atomic *atomic, InstanceDataHeader *header)
{
	if(atomic == nil || atomic->geometry == nil ||
	   atomic->getFrame() == nil || header == nil)
		return;

	if(!setStreamSource(0, header->vertexStream[0].vertexBuffer, 0,
	   header->vertexStream[0].stride))
		return;

	if(!setIndices(header->indexBuffer))
		return;
	if(!setDefaultVertexDeclaration())
		return;

	d3d11Globals.context->IASetPrimitiveTopology(
		(D3D11_PRIMITIVE_TOPOLOGY)header->primType);

	int32 vsBits = lightingCB_Shader(atomic);
	if(!uploadDefaultMatrices(atomic->getFrame()->getLTM()))
		return;

	if(!setDefaultVertexShader(vsBits) || !setDefaultPixelShader())
		return;

	uint32 flags = atomic->geometry->flags;
	static const RGBA white = { 255, 255, 255, 255 };
	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++, inst++){
		Material *material = inst->material;
		if(material == nil)
			continue;

		SetRenderState(VERTEXALPHA,
			inst->vertexAlpha || material->color.alpha != 255);
		setTexture(0, material->texture);
		if(!uploadDefaultMaterial((flags & Geometry::MODULATE) ?
		   material->color : white, material->surfaceProps))
			return;
		drawInst(header, inst);
	}
}

#endif
}
}
