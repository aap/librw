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

struct LightConstants
{
	int32 numLights[4];
	int32 firstLight[4];
	LightVS lights[8];
};

static ID3D11VertexShader *defaultVS;
static ID3D11PixelShader *defaultPS;
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
compileShader(const char *src, const char *entry, const char *target)
{
	ID3DBlob *shader = nil;
	ID3DBlob *errors = nil;
	HRESULT hr = D3DCompile(src, strlen(src), nil, nil, nil,
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
	D3D11_INPUT_ELEMENT_DESC elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DefaultVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DefaultVertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(DefaultVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, offsetof(DefaultVertex, texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D11_BUFFER_DESC desc;
	ID3DBlob *vsBlob = compileShader(default_VS_d11_source, "main", "vs_4_0");
	ID3DBlob *psBlob = compileShader(default_PS_d11_source, "main", "ps_4_0");
	if(vsBlob == nil || psBlob == nil)
		goto fail;

	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(), nil, &defaultVS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreatePixelShader(psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(), nil, &defaultPS)))
		goto fail;
	d3d11Globals.numPixelShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateInputLayout(elements, nelem(elements),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &defaultLayout)))
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

	vsBlob->Release();
	psBlob->Release();
	return 1;

fail:
	if(vsBlob) vsBlob->Release();
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
	if(defaultPS){
		clearPixelShader(defaultPS);
		defaultPS->Release();
		defaultPS = nil;
		d3d11Globals.numPixelShaders--;
	}
	if(defaultVS){
		clearVertexShader(defaultVS);
		defaultVS->Release();
		defaultVS = nil;
		d3d11Globals.numVertexShaders--;
	}
}

static bool32
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

static bool32
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
	if(header == nil)
		return;

	if(!setStreamSource(0, header->vertexStream[0].vertexBuffer, 0,
	   header->vertexStream[0].stride))
		return;

	if(!setIndices(header->indexBuffer))
		return;
	if(!setVertexDeclaration(defaultLayout))
		return;

	d3d11Globals.context->IASetPrimitiveTopology(
		(D3D11_PRIMITIVE_TOPOLOGY)header->primType);

	if(!setVertexShader(defaultVS) || !setPixelShader(defaultPS))
		return;

	if(atomic == nil || atomic->getFrame() == nil)
		return;
	if(!uploadDefaultMatrices(atomic->getFrame()->getLTM()))
		return;
	if(lightConstantBuffer == nil)
		return;
	d3d11Globals.context->VSSetConstantBuffers(VSlotLights, 1,
		&lightConstantBuffer);

	uint32 flags = atomic->geometry->flags;
	static const RGBA white = { 255, 255, 255, 255 };
	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++, inst++){
		Material *material = inst->material;
		if(material == nil)
			continue;

		SetRenderState(VERTEXALPHA,
			inst->vertexAlpha || material->color.alpha != 255);
		if(material->texture && material->texture->raster){
			SetRenderState(TEXTUREFILTER, material->texture->getFilter());
			SetRenderState(TEXTUREADDRESSU, material->texture->getAddressU());
			SetRenderState(TEXTUREADDRESSV, material->texture->getAddressV());
		}
		SetRenderStatePtr(TEXTURERASTER,
			material->texture ? material->texture->raster : nil);
		if(!uploadDefaultMaterial((flags & Geometry::MODULATE) ?
		   material->color : white, material->surfaceProps))
			return;
		drawInst(header, inst);
	}
}

#endif
}
}
