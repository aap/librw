#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwanim.h"
#include "../rwengine.h"
#include "../rwplugins.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#ifdef RW_D3D11
#include <d3dcompiler.h>

#include "rwd3dimpl.h"
#include "skin_VS_d11.h"
#endif

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifndef RW_D3D11
void skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance) {}
void skinRenderCB(Atomic *atomic, InstanceDataHeader *header) {}
#else

static ID3D11VertexShader *skin_amb_VS;
static ID3D11VertexShader *skin_amb_dir_VS;
static ID3D11VertexShader *skin_all_VS;
static ID3D11InputLayout *skinLayout;
static ID3D11Buffer *skinMatrixBuffer;
void destroySkinShaders(void);

void
skinInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	VertexStream *s = &header->vertexStream[0];
	bool32 isPrelit = (geo->flags & Geometry::PRELIT) != 0;
	bool32 hasNormals = (geo->flags & Geometry::NORMALS) != 0;
	bool32 hasTexCoords = geo->numTexCoordSets > 0;

	if(!reinstance){
		assert(s->vertexBuffer == nil);
		s->offset = 0;
		s->stride = sizeof(SkinVertex);
		s->managed = 1;
		s->geometryFlags = 0;
		s->dynamicLock = 0;
		s->vertexBuffer = createVertexBuffer(
			header->totalNumVertex*s->stride, 0, false);
	}

	Skin *skin = Skin::get(geo);
	SkinVertex *vertices = (SkinVertex*)lockVertices(
		s->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
	static const V3d defaultNormal = { 0.0f, 0.0f, 1.0f };
	static const RGBA black = { 0, 0, 0, 255 };
	static const TexCoords zeroTexCoord = { 0.0f, 0.0f };

	// A fixed D3D11 layout uses neutral values for missing geometry attributes.
	if(!reinstance || (geo->lockedSinceInst & Geometry::LOCKVERTICES))
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			vertices[i].position = geo->morphTargets[0].vertices[i];

	if(!reinstance || (geo->lockedSinceInst & Geometry::LOCKPRELIGHT))
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			vertices[i].color = isPrelit ? geo->colors[i] : black;

	if(!reinstance || (geo->lockedSinceInst & Geometry::LOCKTEXCOORDS))
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			vertices[i].texcoord = hasTexCoords ?
				geo->texCoords[0][i] : zeroTexCoord;

	if(!reinstance || (geo->lockedSinceInst & Geometry::LOCKNORMALS))
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			vertices[i].normal = hasNormals ?
				geo->morphTargets[0].normals[i] : defaultNormal;

	if(!reinstance){
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			vertices[i].weights = ((V4d*)skin->weights)[i];
		for(uint32 i = 0; i < header->totalNumVertex; i++)
			memcpy(vertices[i].indices, &skin->indices[i*4],
				sizeof(vertices[i].indices));
	}
	unlockVertices(s->vertexBuffer);

	if(!reinstance || (geo->lockedSinceInst & Geometry::LOCKPRELIGHT)){
		InstanceData *inst = header->inst;
		for(uint32 i = 0; i < header->numMeshes; i++, inst++){
			inst->vertexAlpha = 0;
			if(!isPrelit)
				continue;
			for(uint32 j = 0; j < inst->numVertices; j++)
				if(geo->colors[inst->minVert + j].alpha != 255){
					inst->vertexAlpha = 1;
					break;
				}
		}
	}
}

static float skinMatrices[64*16];

void
uploadSkinMatrices(Atomic *a)
{
	Skin *skin = Skin::get(a->geometry);
	assert(skin->numBones <= 64);
	int i;
	float *m = skinMatrices;
	HAnimHierarchy *hier = Skin::getHierarchy(a);

	if(hier){
		Matrix *invMats = (Matrix*)skin->inverseMatrices;
		Matrix tmp, tmp2;

		assert(skin->numBones == hier->numNodes);
		if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &invMats[i], &hier->matrices[i]);
				RawMatrix::transpose((RawMatrix*)m, (RawMatrix*)&tmp);
				m += 12;
			}
		}else{
			Matrix invAtmMat;
			Matrix::invert(&invAtmMat, a->getFrame()->getLTM());
			for(i = 0; i < hier->numNodes; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
				Matrix::mult(&tmp2, &invMats[i], &tmp);
				RawMatrix::transpose((RawMatrix*)m, (RawMatrix*)&tmp2);
				m += 12;
			}
		}
	}else{
		for(i = 0; i < skin->numBones; i++){
			m[0] = 1.0f;
			m[1] = 0.0f;
			m[2] = 0.0f;
			m[3] = 0.0f;

			m[4] = 0.0f;
			m[5] = 1.0f;
			m[6] = 0.0f;
			m[7] = 0.0f;

			m[8] = 0.0f;
			m[9] = 0.0f;
			m[10] = 1.0f;
			m[11] = 0.0f;

			m += 12;
		}
	}

	d3d11Globals.context->UpdateSubresource(skinMatrixBuffer, 0,
		nil, skinMatrices, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(VSlotSkin, 1,
		&skinMatrixBuffer);
}

void
skinRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setStreamSource(0, header->vertexStream[0].vertexBuffer, 0,
		header->vertexStream[0].stride);
	setIndices(header->indexBuffer);
	setVertexDeclaration(skinLayout);

	d3d11Globals.context->IASetPrimitiveTopology(
		(D3D11_PRIMITIVE_TOPOLOGY)header->primType);

	int32 vsBits = lightingCB_Shader(atomic);
	uploadDefaultMatrices(atomic->getFrame()->getLTM());
	uploadSkinMatrices(atomic);

	if((vsBits & VSLIGHT_MASK) == 0)
		setVertexShader(skin_amb_VS);
	else if((vsBits & VSLIGHT_MASK) == VSLIGHT_DIRECT)
		setVertexShader(skin_amb_dir_VS);
	else
		setVertexShader(skin_all_VS);
	setDefaultPixelShader();

	uint32 flags = atomic->geometry->flags;
	static const RGBA white = { 255, 255, 255, 255 };
	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++, inst++){
		Material *material = inst->material;

		SetRenderState(VERTEXALPHA,
			inst->vertexAlpha || material->color.alpha != 255);
		setTexture(0, material->texture);
		uploadDefaultMaterial((flags & Geometry::MODULATE) ?
			material->color : white, material->surfaceProps);
		drawInst(header, inst);
	}
}

static ID3DBlob*
compileSkinShader(const D3D_SHADER_MACRO *defines = nil)
{
	ID3DBlob *shader = nil;
	ID3DBlob *errors = nil;
	HRESULT hr = D3DCompile(skin_VS_d11_source,
		strlen(skin_VS_d11_source), nil, defines, nil,
		"main", "vs_4_0", 0, 0, &shader, &errors);
	if(errors){
		fprintf(stderr, "%s\n", (const char*)errors->GetBufferPointer());
		errors->Release();
	}
	if(FAILED(hr))
		return nil;
	return shader;
}

void
createSkinShaders(void)
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SkinVertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(SkinVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(SkinVertex, texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(SkinVertex, weights), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, offsetof(SkinVertex, indices), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D11_BUFFER_DESC desc;
	ID3DBlob *ambBlob = compileSkinShader();
	ID3DBlob *ambDirBlob = compileSkinShader(ambDirDefines);
	ID3DBlob *allBlob = compileSkinShader(allDefines);
	if(ambBlob == nil || ambDirBlob == nil || allBlob == nil)
		goto fail;

	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(
	   ambBlob->GetBufferPointer(), ambBlob->GetBufferSize(), nil,
	   &skin_amb_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(
	   ambDirBlob->GetBufferPointer(), ambDirBlob->GetBufferSize(), nil,
	   &skin_amb_dir_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;
	if(FAILED(d3d11Globals.d3ddevice->CreateVertexShader(
	   allBlob->GetBufferPointer(), allBlob->GetBufferSize(), nil,
	   &skin_all_VS)))
		goto fail;
	d3d11Globals.numVertexShaders++;

	if(FAILED(d3d11Globals.d3ddevice->CreateInputLayout(
	   elements, nelem(elements), allBlob->GetBufferPointer(),
	   allBlob->GetBufferSize(), &skinLayout)))
		goto fail;
	d3d11Globals.numInputLayouts++;

	memset(&desc, 0, sizeof(desc));
	// float4x3 occupies three float4 registers, as in the D3D9 shader.
	desc.ByteWidth = 64*12*sizeof(float);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(d3d11Globals.d3ddevice->CreateBuffer(
	   &desc, nil, &skinMatrixBuffer)))
		goto fail;

	ambBlob->Release();
	ambDirBlob->Release();
	allBlob->Release();
	return;

fail:
	if(ambBlob) ambBlob->Release();
	if(ambDirBlob) ambDirBlob->Release();
	if(allBlob) allBlob->Release();
	destroySkinShaders();
	assert(!"Failed to create D3D11 skin shaders");
}

void
destroySkinShaders(void)
{
	if(skinMatrixBuffer){
		ID3D11Buffer *buffer = nil;
		d3d11Globals.context->VSSetConstantBuffers(VSlotSkin, 1, &buffer);
		skinMatrixBuffer->Release();
		skinMatrixBuffer = nil;
	}
	if(skinLayout){
		clearVertexDeclaration(skinLayout);
		skinLayout->Release();
		skinLayout = nil;
		d3d11Globals.numInputLayouts--;
	}
	if(skin_all_VS){
		clearVertexShader(skin_all_VS);
		skin_all_VS->Release();
		skin_all_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
	if(skin_amb_dir_VS){
		clearVertexShader(skin_amb_dir_VS);
		skin_amb_dir_VS->Release();
		skin_amb_dir_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
	if(skin_amb_VS){
		clearVertexShader(skin_amb_VS);
		skin_amb_VS->Release();
		skin_amb_VS = nil;
		d3d11Globals.numVertexShaders--;
	}
}

#endif

static void*
skinOpen(void *o, int32, int32)
{
#ifdef RW_D3D11
	createSkinShaders();
#endif

	skinGlobals.pipelines[PLATFORM_D3D11] = makeSkinPipeline();
	return o;
}

static void*
skinClose(void *o, int32, int32)
{
#ifdef RW_D3D11
	destroySkinShaders();
#endif

	((ObjPipeline*)skinGlobals.pipelines[PLATFORM_D3D11])->destroy();
	skinGlobals.pipelines[PLATFORM_D3D11] = nil;
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, ID_SKIN,
	                       skinOpen, skinClose);
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = nil;
	pipe->renderCB = skinRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

}
}
