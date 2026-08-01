#ifdef RW_D3D11

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <d3d11_1.h>
#include <d3dcompiler.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwrender.h"
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwd3d11.h"

#ifdef LIBRW_SDL3
#include <SDL3/SDL.h>
#elif defined(LIBRW_SDL2)
#include <SDL.h>
#include <SDL_syswm.h>
#elif defined(LIBRW_GLFW)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#define PLUGIN_ID ID_DRIVER

namespace rw {
namespace d3d11 {

struct Constants
{
	float mvp[16];
	float matColor[4];
	float xform[4];
	float alphaRef[4];
	float world[16];
	float normal[16];
	float surface[4];
	float ambient[4];
	float fog[4];
	float fogColor[4];
	float flags[4];
	float lightParams[8][4];
	float lightColor[8][4];
	float lightPosition[8][4];
	float lightDirection[8][4];
};

struct D3D11Globals
{
	HWND window;
	ID3D11Device *device;
	ID3D11DeviceContext *context;
	IDXGISwapChain *swapChain;
	ID3D11RenderTargetView *backBufferRTV;
	ID3D11Texture2D *depthTexture;
	ID3D11DepthStencilView *depthDSV;
	ID3D11VertexShader *vs2D;
	ID3D11PixelShader *ps2D;
	ID3D11InputLayout *layout2D;
	ID3D11VertexShader *vs3D;
	ID3D11PixelShader *ps3D;
	ID3D11InputLayout *layout3D;
	ID3D11Buffer *constantBuffer;
	ID3D11Buffer *dynamicVertexBuffer;
	ID3D11Buffer *dynamicIndexBuffer;
	uint32 dynamicVertexCapacity;
	uint32 dynamicIndexCapacity;
	ID3D11BlendState *blendOpaque;
	ID3D11BlendState *blendAlpha;
	ID3D11DepthStencilState *depthStates[2][2];
	ID3D11BlendState *blendState;
	bool32 blendStateAlpha;
	uint32 blendStateSrc;
	uint32 blendStateDst;
	ID3D11DepthStencilState *depthStencilState;
	uint32 depthStateZTest;
	uint32 depthStateZWrite;
	uint32 depthStateStencilEnable;
	uint32 depthStateStencilFail;
	uint32 depthStateStencilZFail;
	uint32 depthStateStencilPass;
	uint32 depthStateStencilFunc;
	uint32 depthStateStencilMask;
	uint32 depthStateStencilWriteMask;
	ID3D11RasterizerState *rastNone;
	ID3D11RasterizerState *rastBack;
	ID3D11RasterizerState *rastFront;
	ID3D11SamplerState *sampler;
	uint32 samplerFilter;
	uint32 samplerAddressU;
	uint32 samplerAddressV;
	ID3D11Texture2D *whiteTexture;
	ID3D11ShaderResourceView *whiteSRV;
	void *renderStates[GSALPHATESTREF + 1];
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
	VideoMode *modes;
	int32 numModes;
#endif
	VideoMode mode;
	int32 currentMode;
	bool32 inFrame;
	uint32 presentWidth;
	uint32 presentHeight;
	Im3DVertex *tempVertices;
	uint32 tempVertexCapacity;
	Im3DVertex *im3dVertices;
	int32 im3dNumVertices;
	Matrix im3dWorld;
	uint32 im3dFlags;
	float32 fogStart;
	float32 fogEnd;
	float32 fogRange;
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
	SDL_Window **pWindow;
#elif defined(LIBRW_GLFW)
	GLFWwindow **pWindow;
#endif
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
	int winWidth, winHeight;
	const char *winTitle;
#endif
};

static D3D11Globals d3d11Globals;

static void
releaseIUnknown(IUnknown *object)
{
	if(object)
		object->Release();
}

ID3D11Device*
getDevice(void)
{
	return d3d11Globals.device;
}

ID3D11DeviceContext*
getContext(void)
{
	return d3d11Globals.context;
}

static uint32
getRenderStateUInt(int32 state, uint32 fallback)
{
	if(state < 0 || state > GSALPHATESTREF)
		return fallback;
	return (uint32)(uintptr)d3d11Globals.renderStates[state];
}

static void
setRenderState(int32 state, void *value)
{
	if(state < 0 || state > GSALPHATESTREF)
		return;
	d3d11Globals.renderStates[state] = value;
	if(state == TEXTUREADDRESS){
		d3d11Globals.renderStates[TEXTUREADDRESSU] = value;
		d3d11Globals.renderStates[TEXTUREADDRESSV] = value;
	}
}

static void*
getRenderState(int32 state)
{
	if(state < 0 || state > GSALPHATESTREF)
		return nil;
	if(state == TEXTUREADDRESS){
		if(d3d11Globals.renderStates[TEXTUREADDRESSU] == d3d11Globals.renderStates[TEXTUREADDRESSV])
			return d3d11Globals.renderStates[TEXTUREADDRESSU];
		return nil;
	}
	if(state >= 0 && state <= GSALPHATESTREF)
		return d3d11Globals.renderStates[state];
	return nil;
}

static void
resetRenderState(void)
{
	memset(d3d11Globals.renderStates, 0, sizeof(d3d11Globals.renderStates));
	setRenderState(SRCBLEND, (void*)(uintptr)BLENDSRCALPHA);
	setRenderState(DESTBLEND, (void*)(uintptr)BLENDINVSRCALPHA);
	setRenderState(ZTESTENABLE, (void*)(uintptr)1);
	setRenderState(ZWRITEENABLE, (void*)(uintptr)1);
	setRenderState(CULLMODE, (void*)(uintptr)CULLNONE);
	setRenderState(ALPHATESTFUNC, (void*)(uintptr)ALPHAGREATEREQUAL);
	setRenderState(ALPHATESTREF, (void*)(uintptr)10);
	setRenderState(GSALPHATESTREF, (void*)(uintptr)128);
	setRenderState(TEXTUREFILTER, (void*)(uintptr)Texture::LINEAR);
	setRenderState(TEXTUREADDRESS, (void*)(uintptr)Texture::WRAP);
	setRenderState(TEXTUREADDRESSU, (void*)(uintptr)Texture::WRAP);
	setRenderState(TEXTUREADDRESSV, (void*)(uintptr)Texture::WRAP);
	setRenderState(FOGENABLE, (void*)(uintptr)0);
	setRenderState(FOGCOLOR, (void*)(uintptr)RWRGBAINT(255, 255, 255, 255));
	setRenderState(STENCILENABLE, (void*)(uintptr)0);
	setRenderState(STENCILFAIL, (void*)(uintptr)STENCILKEEP);
	setRenderState(STENCILZFAIL, (void*)(uintptr)STENCILKEEP);
	setRenderState(STENCILPASS, (void*)(uintptr)STENCILKEEP);
	setRenderState(STENCILFUNCTION, (void*)(uintptr)STENCILALWAYS);
	setRenderState(STENCILFUNCTIONREF, (void*)(uintptr)0);
	setRenderState(STENCILFUNCTIONMASK, (void*)(uintptr)0xFFFFFFFF);
	setRenderState(STENCILFUNCTIONWRITEMASK, (void*)(uintptr)0xFFFFFFFF);
}

static void
colorToFloat(float *dst, const RGBA &c)
{
	dst[0] = c.red / 255.0f;
	dst[1] = c.green / 255.0f;
	dst[2] = c.blue / 255.0f;
	dst[3] = c.alpha / 255.0f;
}

static void
makeAlphaRef(float *dst)
{
	uint32 func = getRenderStateUInt(ALPHATESTFUNC, ALPHAGREATEREQUAL);
	float ref = getRenderStateUInt(ALPHATESTREF, 10) / 255.0f;
	switch(func){
	case ALPHAALWAYS:
		dst[0] = -1.0f;
		dst[1] = 2.0f;
		break;
	case ALPHALESS:
		dst[0] = -1.0f;
		dst[1] = ref;
		break;
	case ALPHAGREATEREQUAL:
	default:
		dst[0] = ref;
		dst[1] = 2.0f;
		break;
	}
	dst[2] = 0.0f;
	dst[3] = 0.0f;
}

static D3D11_PRIMITIVE_TOPOLOGY
topologyFromPrim(PrimitiveType primType)
{
	switch(primType){
	case PRIMTYPELINELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case PRIMTYPEPOLYLINE:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case PRIMTYPETRILIST:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PRIMTYPETRISTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PRIMTYPEPOINTLIST:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	default:
		return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}

static bool32
validPrim(PrimitiveType primType)
{
	return topologyFromPrim(primType) != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

static uint16*
makeTriangleFanIndices(void *indices, int32 numIndices, int32 numVertices, int32 *outNumIndices)
{
	int32 count = indices && numIndices > 0 ? numIndices : numVertices;
	if(count < 3)
		return nil;
	int32 n = (count - 2)*3;
	uint16 *out = rwNewT(uint16, n, MEMDUR_FUNCTION | ID_DRIVER);
	if(out == nil)
		return nil;

	if(indices){
		uint16 *src = (uint16*)indices;
		for(int32 i = 0; i < count-2; i++){
			out[i*3 + 0] = src[0];
			out[i*3 + 1] = src[i+1];
			out[i*3 + 2] = src[i+2];
		}
	}else{
		for(int32 i = 0; i < count-2; i++){
			out[i*3 + 0] = 0;
			out[i*3 + 1] = i+1;
			out[i*3 + 2] = i+2;
		}
	}

	*outNumIndices = n;
	return out;
}

static HRESULT
compileShader(const char *source, const char *entry, const char *profile, ID3DBlob **blob)
{
	ID3DBlob *errors = nil;
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	HRESULT hr = D3DCompile(source, strlen(source), nil, nil, nil,
		entry, profile, flags, 0, blob, &errors);
	if(FAILED(hr) && errors){
		printf("D3D11 shader compile error: %s\n", (const char*)errors->GetBufferPointer());
	}
	releaseIUnknown(errors);
	return hr;
}

static bool32
createShaders(void)
{
	static const char *shader2D =
		"Texture2D u_tex : register(t0);\n"
		"SamplerState u_samp : register(s0);\n"
		"cbuffer C : register(b0) {\n"
		"	row_major float4x4 u_mvp;\n"
		"	float4 u_matColor;\n"
		"	float4 u_xform;\n"
		"	float4 u_alphaRef;\n"
		"};\n"
		"struct VSIn { float4 pos : POSITION; float4 color : COLOR0; float2 uv : TEXCOORD0; };\n"
		"struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR0; float2 uv : TEXCOORD0; };\n"
		"VSOut vs_main(VSIn input) {\n"
		"	VSOut o;\n"
		"	float w = max(input.pos.w, 0.000001f);\n"
		"	o.pos = float4(input.pos.x*u_xform.x + u_xform.z,\n"
		"	               input.pos.y*u_xform.y + u_xform.w,\n"
		"	               input.pos.z, w);\n"
		"	o.pos.xyz *= o.pos.w;\n"
		"	o.color = input.color;\n"
		"	o.uv = input.uv;\n"
		"	return o;\n"
		"}\n"
		"float4 ps_main(VSOut input) : SV_TARGET {\n"
		"	float4 c = u_tex.Sample(u_samp, input.uv) * input.color;\n"
		"	if(c.a < u_alphaRef.x || c.a >= u_alphaRef.y) discard;\n"
		"	return c;\n"
		"}\n";

	static const char *shader3D =
		"Texture2D u_tex : register(t0);\n"
		"SamplerState u_samp : register(s0);\n"
		"cbuffer C : register(b0) {\n"
		"	row_major float4x4 u_mvp;\n"
		"	float4 u_matColor;\n"
		"	float4 u_xform;\n"
		"	float4 u_alphaRef;\n"
		"	row_major float4x4 u_world;\n"
		"	row_major float4x4 u_normal;\n"
		"	float4 u_surface;\n"
		"	float4 u_ambient;\n"
		"	float4 u_fog;\n"
		"	float4 u_fogColor;\n"
		"	float4 u_flags;\n"
		"	float4 u_lightParams[8];\n"
		"	float4 u_lightColor[8];\n"
		"	float4 u_lightPosition[8];\n"
		"	float4 u_lightDirection[8];\n"
		"};\n"
		"struct VSIn { float3 pos : POSITION; float3 normal : NORMAL; float4 color : COLOR0; float2 uv : TEXCOORD0; };\n"
		"struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR0; float2 uv : TEXCOORD0; float fog : TEXCOORD1; };\n"
		"float3 doDirLight(int i, float3 n) {\n"
		"	return max(0.0f, dot(n, -u_lightDirection[i].xyz)) * u_lightColor[i].rgb;\n"
		"}\n"
		"float3 doPointLight(int i, float3 worldPos, float3 n) {\n"
		"	float3 dir = worldPos - u_lightPosition[i].xyz;\n"
		"	float dist = length(dir);\n"
		"	float radius = max(u_lightParams[i].y, 0.0001f);\n"
		"	float atten = max(0.0f, 1.0f - dist/radius);\n"
		"	float l = max(0.0f, dot(n, -normalize(dir)));\n"
		"	return l * u_lightColor[i].rgb * atten;\n"
		"}\n"
		"float3 doSpotLight(int i, float3 worldPos, float3 n) {\n"
		"	float3 dir = worldPos - u_lightPosition[i].xyz;\n"
		"	float dist = length(dir);\n"
		"	float radius = max(u_lightParams[i].y, 0.0001f);\n"
		"	float atten = max(0.0f, 1.0f - dist/radius);\n"
		"	dir /= max(dist, 0.0001f);\n"
		"	float l = max(0.0f, dot(n, -dir));\n"
		"	float ccos = -u_lightParams[i].z;\n"
		"	float falloff = (dot(dir, u_lightDirection[i].xyz) - ccos) / max(1.0f - ccos, 0.0001f);\n"
		"	if(falloff < 0.0f) l = 0.0f;\n"
		"	l *= max(falloff, u_lightParams[i].w);\n"
		"	return l * u_lightColor[i].rgb * atten;\n"
		"}\n"
		"VSOut vs_main(VSIn input) {\n"
		"	VSOut o;\n"
		"	o.pos = mul(float4(input.pos, 1.0f), u_mvp);\n"
		"	float3 worldPos = mul(float4(input.pos, 1.0f), u_world).xyz;\n"
		"	float3 n = normalize(mul(float4(input.normal, 0.0f), u_normal).xyz);\n"
		"	float4 color = input.color;\n"
		"	if(u_flags.x > 0.5f) {\n"
		"		color.rgb += u_ambient.rgb * u_surface.x;\n"
		"		[unroll] for(int i = 0; i < 8; i++) {\n"
		"			if(u_lightParams[i].x == 1.0f) color.rgb += doDirLight(i, n) * u_surface.z;\n"
		"			else if(u_lightParams[i].x == 2.0f) color.rgb += doPointLight(i, worldPos, n) * u_surface.z;\n"
		"			else if(u_lightParams[i].x == 3.0f) color.rgb += doSpotLight(i, worldPos, n) * u_surface.z;\n"
		"		}\n"
		"		color.rgb = saturate(color.rgb);\n"
		"	}\n"
		"	o.color = color * u_matColor;\n"
		"	o.uv = input.uv;\n"
		"	o.fog = clamp((o.pos.w - u_fog.y) * u_fog.z, u_fog.w, 1.0f);\n"
		"	return o;\n"
		"}\n"
		"float4 ps_main(VSOut input) : SV_TARGET {\n"
		"	float4 c = u_tex.Sample(u_samp, input.uv) * input.color;\n"
		"	c.rgb = lerp(u_fogColor.rgb, c.rgb, input.fog);\n"
		"	if(c.a < u_alphaRef.x || c.a >= u_alphaRef.y) discard;\n"
		"	return c;\n"
		"}\n";

	ID3DBlob *vsBlob = nil;
	ID3DBlob *psBlob = nil;
	if(FAILED(compileShader(shader2D, "vs_main", "vs_4_0", &vsBlob)) ||
	   FAILED(compileShader(shader2D, "ps_main", "ps_4_0", &psBlob)))
		goto fail;
	if(FAILED(d3d11Globals.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nil, &d3d11Globals.vs2D)) ||
	   FAILED(d3d11Globals.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nil, &d3d11Globals.ps2D)))
		goto fail;
	{
		D3D11_INPUT_ELEMENT_DESC elems[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if(FAILED(d3d11Globals.device->CreateInputLayout(elems, 3,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &d3d11Globals.layout2D)))
			goto fail;
	}
	releaseIUnknown(vsBlob);
	releaseIUnknown(psBlob);
	vsBlob = psBlob = nil;

	if(FAILED(compileShader(shader3D, "vs_main", "vs_4_0", &vsBlob)) ||
	   FAILED(compileShader(shader3D, "ps_main", "ps_4_0", &psBlob)))
		goto fail;
	if(FAILED(d3d11Globals.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nil, &d3d11Globals.vs3D)) ||
	   FAILED(d3d11Globals.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nil, &d3d11Globals.ps3D)))
		goto fail;
	{
		D3D11_INPUT_ELEMENT_DESC elems[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		if(FAILED(d3d11Globals.device->CreateInputLayout(elems, 4,
			vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &d3d11Globals.layout3D)))
			goto fail;
	}
	releaseIUnknown(vsBlob);
	releaseIUnknown(psBlob);
	return 1;

fail:
	releaseIUnknown(vsBlob);
	releaseIUnknown(psBlob);
	return 0;
}

static bool32
createStates(void)
{
	D3D11_BUFFER_DESC cbdesc;
	memset(&cbdesc, 0, sizeof(cbdesc));
	cbdesc.ByteWidth = sizeof(Constants);
	cbdesc.Usage = D3D11_USAGE_DEFAULT;
	cbdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(d3d11Globals.device->CreateBuffer(&cbdesc, nil, &d3d11Globals.constantBuffer)))
		return 0;

	D3D11_BLEND_DESC bdesc;
	memset(&bdesc, 0, sizeof(bdesc));
	bdesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if(FAILED(d3d11Globals.device->CreateBlendState(&bdesc, &d3d11Globals.blendOpaque)))
		return 0;
	bdesc.RenderTarget[0].BlendEnable = TRUE;
	bdesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bdesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bdesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bdesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	bdesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	if(FAILED(d3d11Globals.device->CreateBlendState(&bdesc, &d3d11Globals.blendAlpha)))
		return 0;

	for(int ztest = 0; ztest < 2; ztest++){
		for(int zwrite = 0; zwrite < 2; zwrite++){
			D3D11_DEPTH_STENCIL_DESC ddesc;
			memset(&ddesc, 0, sizeof(ddesc));
			ddesc.DepthEnable = ztest;
			ddesc.DepthWriteMask = zwrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			ddesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			ddesc.StencilEnable = FALSE;
			if(FAILED(d3d11Globals.device->CreateDepthStencilState(&ddesc, &d3d11Globals.depthStates[ztest][zwrite])))
				return 0;
		}
	}

	D3D11_RASTERIZER_DESC rdesc;
	memset(&rdesc, 0, sizeof(rdesc));
	rdesc.FillMode = D3D11_FILL_SOLID;
	rdesc.DepthClipEnable = TRUE;
	rdesc.FrontCounterClockwise = TRUE;
	rdesc.CullMode = D3D11_CULL_NONE;
	if(FAILED(d3d11Globals.device->CreateRasterizerState(&rdesc, &d3d11Globals.rastNone)))
		return 0;
	rdesc.CullMode = D3D11_CULL_BACK;
	if(FAILED(d3d11Globals.device->CreateRasterizerState(&rdesc, &d3d11Globals.rastBack)))
		return 0;
	rdesc.CullMode = D3D11_CULL_FRONT;
	if(FAILED(d3d11Globals.device->CreateRasterizerState(&rdesc, &d3d11Globals.rastFront)))
		return 0;

	uint32 white = 0xFFFFFFFF;
	D3D11_TEXTURE2D_DESC tdesc;
	memset(&tdesc, 0, sizeof(tdesc));
	tdesc.Width = 1;
	tdesc.Height = 1;
	tdesc.MipLevels = 1;
	tdesc.ArraySize = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tdesc.SampleDesc.Count = 1;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA sub;
	memset(&sub, 0, sizeof(sub));
	sub.pSysMem = &white;
	sub.SysMemPitch = 4;
	sub.SysMemSlicePitch = 4;
	if(FAILED(d3d11Globals.device->CreateTexture2D(&tdesc, &sub, &d3d11Globals.whiteTexture)))
		return 0;
	if(FAILED(d3d11Globals.device->CreateShaderResourceView(d3d11Globals.whiteTexture, nil, &d3d11Globals.whiteSRV)))
		return 0;

	return 1;
}

static D3D11_FILTER
filterFromRW(uint32 filter)
{
	switch(filter){
	case Texture::NEAREST:
		return D3D11_FILTER_MIN_MAG_MIP_POINT;
	case Texture::LINEAR:
		return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case Texture::MIPNEAREST:
		return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case Texture::MIPLINEAR:
	case Texture::LINEARMIPNEAREST:
	case Texture::LINEARMIPLINEAR:
	default:
		return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

static D3D11_TEXTURE_ADDRESS_MODE
addressFromRW(uint32 address)
{
	switch(address){
	case Texture::MIRROR:
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	case Texture::CLAMP:
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	case Texture::BORDER:
		return D3D11_TEXTURE_ADDRESS_BORDER;
	case Texture::WRAP:
	default:
		return D3D11_TEXTURE_ADDRESS_WRAP;
	}
}

static void
bindSampler(void)
{
	uint32 filter = getRenderStateUInt(TEXTUREFILTER, Texture::LINEAR);
	uint32 baseAddress = getRenderStateUInt(TEXTUREADDRESS, Texture::WRAP);
	uint32 addressU = getRenderStateUInt(TEXTUREADDRESSU, baseAddress);
	uint32 addressV = getRenderStateUInt(TEXTUREADDRESSV, baseAddress);

	if(d3d11Globals.sampler &&
	   d3d11Globals.samplerFilter == filter &&
	   d3d11Globals.samplerAddressU == addressU &&
	   d3d11Globals.samplerAddressV == addressV){
		d3d11Globals.context->PSSetSamplers(0, 1, &d3d11Globals.sampler);
		return;
	}

	releaseIUnknown(d3d11Globals.sampler);
	d3d11Globals.sampler = nil;

	D3D11_SAMPLER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Filter = filterFromRW(filter);
	desc.AddressU = addressFromRW(addressU);
	desc.AddressV = addressFromRW(addressV);
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 0.0f;
	if(SUCCEEDED(d3d11Globals.device->CreateSamplerState(&desc, &d3d11Globals.sampler))){
		d3d11Globals.samplerFilter = filter;
		d3d11Globals.samplerAddressU = addressU;
		d3d11Globals.samplerAddressV = addressV;
		d3d11Globals.context->PSSetSamplers(0, 1, &d3d11Globals.sampler);
	}
}

static bool32
ensureDynamicBuffer(ID3D11Buffer **buffer, uint32 *capacity, uint32 size, uint32 bindFlags)
{
	if(*buffer && *capacity >= size)
		return 1;
	releaseIUnknown(*buffer);
	*buffer = nil;
	*capacity = size < 4096 ? 4096 : size;

	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = *capacity;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = bindFlags;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return SUCCEEDED(d3d11Globals.device->CreateBuffer(&desc, nil, buffer));
}

static bool32
uploadDynamic(ID3D11Buffer *buffer, const void *data, uint32 size)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(d3d11Globals.context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return 0;
	memcpy(mapped.pData, data, size);
	d3d11Globals.context->Unmap(buffer, 0);
	return 1;
}

static bool32
uploadVertices(const void *data, uint32 size)
{
	return ensureDynamicBuffer(&d3d11Globals.dynamicVertexBuffer,
		&d3d11Globals.dynamicVertexCapacity, size, D3D11_BIND_VERTEX_BUFFER) &&
		uploadDynamic(d3d11Globals.dynamicVertexBuffer, data, size);
}

static bool32
uploadIndices(const void *data, uint32 size)
{
	return ensureDynamicBuffer(&d3d11Globals.dynamicIndexBuffer,
		&d3d11Globals.dynamicIndexCapacity, size, D3D11_BIND_INDEX_BUFFER) &&
		uploadDynamic(d3d11Globals.dynamicIndexBuffer, data, size);
}

static bool32
rasterHasAlpha(Raster *raster)
{
	if(raster == nil || raster->platform != PLATFORM_D3D11 || nativeRasterOffset == 0)
		return 0;
	return GETD3D11RASTEREXT(raster)->hasAlpha;
}

static ID3D11ShaderResourceView*
getTextureSRV(Raster *raster)
{
	if(raster && raster->platform == PLATFORM_D3D11 && ensureTextureUploaded(raster)){
		D3D11Raster *natras = GETD3D11RASTEREXT(raster);
		if(natras->srv)
			return natras->srv;
	}
	return d3d11Globals.whiteSRV;
}

static D3D11_BLEND
blendFromRW(uint32 blend)
{
	switch(blend){
	case BLENDZERO: return D3D11_BLEND_ZERO;
	case BLENDONE: return D3D11_BLEND_ONE;
	case BLENDSRCCOLOR: return D3D11_BLEND_SRC_COLOR;
	case BLENDINVSRCCOLOR: return D3D11_BLEND_INV_SRC_COLOR;
	case BLENDSRCALPHA: return D3D11_BLEND_SRC_ALPHA;
	case BLENDINVSRCALPHA: return D3D11_BLEND_INV_SRC_ALPHA;
	case BLENDDESTALPHA: return D3D11_BLEND_DEST_ALPHA;
	case BLENDINVDESTALPHA: return D3D11_BLEND_INV_DEST_ALPHA;
	case BLENDDESTCOLOR: return D3D11_BLEND_DEST_COLOR;
	case BLENDINVDESTCOLOR: return D3D11_BLEND_INV_DEST_COLOR;
	case BLENDSRCALPHASAT: return D3D11_BLEND_SRC_ALPHA_SAT;
	default: return D3D11_BLEND_ONE;
	}
}

static D3D11_STENCIL_OP
stencilOpFromRW(uint32 op)
{
	switch(op){
	case STENCILZERO: return D3D11_STENCIL_OP_ZERO;
	case STENCILREPLACE: return D3D11_STENCIL_OP_REPLACE;
	case STENCILINCSAT: return D3D11_STENCIL_OP_INCR_SAT;
	case STENCILDECSAT: return D3D11_STENCIL_OP_DECR_SAT;
	case STENCILINVERT: return D3D11_STENCIL_OP_INVERT;
	case STENCILINC: return D3D11_STENCIL_OP_INCR;
	case STENCILDEC: return D3D11_STENCIL_OP_DECR;
	case STENCILKEEP:
	default: return D3D11_STENCIL_OP_KEEP;
	}
}

static D3D11_COMPARISON_FUNC
stencilFuncFromRW(uint32 func)
{
	switch(func){
	case STENCILNEVER: return D3D11_COMPARISON_NEVER;
	case STENCILLESS: return D3D11_COMPARISON_LESS;
	case STENCILEQUAL: return D3D11_COMPARISON_EQUAL;
	case STENCILLESSEQUAL: return D3D11_COMPARISON_LESS_EQUAL;
	case STENCILGREATER: return D3D11_COMPARISON_GREATER;
	case STENCILNOTEQUAL: return D3D11_COMPARISON_NOT_EQUAL;
	case STENCILGREATEREQUAL: return D3D11_COMPARISON_GREATER_EQUAL;
	case STENCILALWAYS:
	default: return D3D11_COMPARISON_ALWAYS;
	}
}

static ID3D11BlendState*
getBlendState(bool32 alpha)
{
	uint32 src = getRenderStateUInt(SRCBLEND, BLENDSRCALPHA);
	uint32 dst = getRenderStateUInt(DESTBLEND, BLENDINVSRCALPHA);
	if(d3d11Globals.blendState &&
	   d3d11Globals.blendStateAlpha == alpha &&
	   d3d11Globals.blendStateSrc == src &&
	   d3d11Globals.blendStateDst == dst)
		return d3d11Globals.blendState;

	releaseIUnknown(d3d11Globals.blendState);
	d3d11Globals.blendState = nil;

	D3D11_BLEND_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.RenderTarget[0].BlendEnable = alpha;
	desc.RenderTarget[0].SrcBlend = blendFromRW(src);
	desc.RenderTarget[0].DestBlend = blendFromRW(dst);
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if(FAILED(d3d11Globals.device->CreateBlendState(&desc, &d3d11Globals.blendState)))
		return alpha ? d3d11Globals.blendAlpha : d3d11Globals.blendOpaque;

	d3d11Globals.blendStateAlpha = alpha;
	d3d11Globals.blendStateSrc = src;
	d3d11Globals.blendStateDst = dst;
	return d3d11Globals.blendState;
}

static ID3D11DepthStencilState*
getDepthStencilState(void)
{
	uint32 ztest = getRenderStateUInt(ZTESTENABLE, 1) ? 1 : 0;
	uint32 zwrite = getRenderStateUInt(ZWRITEENABLE, 1) ? 1 : 0;
	uint32 stencilEnable = getRenderStateUInt(STENCILENABLE, 0) ? 1 : 0;
	uint32 stencilFail = getRenderStateUInt(STENCILFAIL, STENCILKEEP);
	uint32 stencilZFail = getRenderStateUInt(STENCILZFAIL, STENCILKEEP);
	uint32 stencilPass = getRenderStateUInt(STENCILPASS, STENCILKEEP);
	uint32 stencilFunc = getRenderStateUInt(STENCILFUNCTION, STENCILALWAYS);
	uint32 stencilMask = getRenderStateUInt(STENCILFUNCTIONMASK, 0xFFFFFFFF);
	uint32 stencilWriteMask = getRenderStateUInt(STENCILFUNCTIONWRITEMASK, 0xFFFFFFFF);

	if(d3d11Globals.depthStencilState &&
	   d3d11Globals.depthStateZTest == ztest &&
	   d3d11Globals.depthStateZWrite == zwrite &&
	   d3d11Globals.depthStateStencilEnable == stencilEnable &&
	   d3d11Globals.depthStateStencilFail == stencilFail &&
	   d3d11Globals.depthStateStencilZFail == stencilZFail &&
	   d3d11Globals.depthStateStencilPass == stencilPass &&
	   d3d11Globals.depthStateStencilFunc == stencilFunc &&
	   d3d11Globals.depthStateStencilMask == stencilMask &&
	   d3d11Globals.depthStateStencilWriteMask == stencilWriteMask)
		return d3d11Globals.depthStencilState;

	releaseIUnknown(d3d11Globals.depthStencilState);
	d3d11Globals.depthStencilState = nil;

	D3D11_DEPTH_STENCIL_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.DepthEnable = ztest || zwrite;
	desc.DepthWriteMask = zwrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = ztest ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = stencilEnable;
	desc.StencilReadMask = (UINT8)stencilMask;
	desc.StencilWriteMask = (UINT8)stencilWriteMask;
	desc.FrontFace.StencilFailOp = stencilOpFromRW(stencilFail);
	desc.FrontFace.StencilDepthFailOp = stencilOpFromRW(stencilZFail);
	desc.FrontFace.StencilPassOp = stencilOpFromRW(stencilPass);
	desc.FrontFace.StencilFunc = stencilFuncFromRW(stencilFunc);
	desc.BackFace = desc.FrontFace;
	if(FAILED(d3d11Globals.device->CreateDepthStencilState(&desc, &d3d11Globals.depthStencilState)))
		return d3d11Globals.depthStates[ztest][zwrite];

	d3d11Globals.depthStateZTest = ztest;
	d3d11Globals.depthStateZWrite = zwrite;
	d3d11Globals.depthStateStencilEnable = stencilEnable;
	d3d11Globals.depthStateStencilFail = stencilFail;
	d3d11Globals.depthStateStencilZFail = stencilZFail;
	d3d11Globals.depthStateStencilPass = stencilPass;
	d3d11Globals.depthStateStencilFunc = stencilFunc;
	d3d11Globals.depthStateStencilMask = stencilMask;
	d3d11Globals.depthStateStencilWriteMask = stencilWriteMask;
	return d3d11Globals.depthStencilState;
}

static void
applyDrawState(bool32 alpha)
{
	float blendFactor[4] = { 0, 0, 0, 0 };
	d3d11Globals.context->OMSetBlendState(getBlendState(alpha), blendFactor, 0xFFFFFFFF);

	d3d11Globals.context->OMSetDepthStencilState(getDepthStencilState(),
		getRenderStateUInt(STENCILFUNCTIONREF, 0));

	switch(getRenderStateUInt(CULLMODE, CULLNONE)){
	case CULLBACK:
		d3d11Globals.context->RSSetState(d3d11Globals.rastBack);
		break;
	case CULLFRONT:
		d3d11Globals.context->RSSetState(d3d11Globals.rastFront);
		break;
	case CULLNONE:
	default:
		d3d11Globals.context->RSSetState(d3d11Globals.rastNone);
		break;
	}
}

static void
makeViewProj(RawMatrix *out)
{
	Camera *cam = (Camera*)engine->currentCamera;
	if(cam)
		RawMatrix::mult(out, &cam->devView, &cam->devProj);
	else
		RawMatrix::setIdentity(out);
}

static void
updateConstants(const Constants *constants)
{
	d3d11Globals.context->UpdateSubresource(d3d11Globals.constantBuffer, 0, nil, constants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(0, 1, &d3d11Globals.constantBuffer);
	d3d11Globals.context->PSSetConstantBuffers(0, 1, &d3d11Globals.constantBuffer);
}

static void
setFogConstants(Constants *c)
{
	c->fog[0] = d3d11Globals.fogStart;
	c->fog[1] = d3d11Globals.fogEnd;
	c->fog[2] = d3d11Globals.fogRange;
	c->fog[3] = getRenderStateUInt(FOGENABLE, 0) ? 0.0f : 1.0f;

	uint32 fog = getRenderStateUInt(FOGCOLOR, RWRGBAINT(255, 255, 255, 255));
	RGBA fogColor;
	fogColor.red = fog;
	fogColor.green = fog >> 8;
	fogColor.blue = fog >> 16;
	fogColor.alpha = fog >> 24;
	colorToFloat(c->fogColor, fogColor);
}

static void
setDefaultSurface(Constants *c, const SurfaceProperties *surface)
{
	if(surface){
		c->surface[0] = surface->ambient;
		c->surface[1] = surface->specular;
		c->surface[2] = surface->diffuse;
	}else{
		c->surface[0] = 1.0f;
		c->surface[1] = 0.0f;
		c->surface[2] = 1.0f;
	}
	c->surface[3] = 0.0f;
}

static void
draw2D(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices)
{
	uint16 *fanIndices = nil;
	if(primType == PRIMTYPETRIFAN){
		fanIndices = makeTriangleFanIndices(indices, numIndices, numVertices, &numIndices);
		if(fanIndices == nil)
			return;
		indices = fanIndices;
		primType = PRIMTYPETRILIST;
	}

	if(!validPrim(primType) || numVertices <= 0){
		rwFree(fanIndices);
		return;
	}
	uint32 vertexSize = (uint32)numVertices * sizeof(Im2DVertex);
	if(!uploadVertices(vertices, vertexSize)){
		rwFree(fanIndices);
		return;
	}
	if(indices && numIndices > 0 && !uploadIndices(indices, (uint32)numIndices*sizeof(uint16))){
		rwFree(fanIndices);
		return;
	}

	bool32 vertexAlpha = 0;
	Im2DVertex *verts = (Im2DVertex*)vertices;
	for(int32 i = 0; i < numVertices; i++)
		if(verts[i].a != 0xFF)
			vertexAlpha = 1;
	Raster *textureRaster = (Raster*)d3d11Globals.renderStates[TEXTURERASTER];
	bool32 alpha = vertexAlpha || rasterHasAlpha(textureRaster) || getRenderStateUInt(VERTEXALPHA, 0);

	applyDrawState(alpha);
	d3d11Globals.context->IASetPrimitiveTopology(topologyFromPrim(primType));
	d3d11Globals.context->IASetInputLayout(d3d11Globals.layout2D);
	UINT stride = sizeof(Im2DVertex);
	UINT offset = 0;
	d3d11Globals.context->IASetVertexBuffers(0, 1, &d3d11Globals.dynamicVertexBuffer, &stride, &offset);
	if(indices && numIndices > 0)
		d3d11Globals.context->IASetIndexBuffer(d3d11Globals.dynamicIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	d3d11Globals.context->VSSetShader(d3d11Globals.vs2D, nil, 0);
	d3d11Globals.context->PSSetShader(d3d11Globals.ps2D, nil, 0);

	Constants c;
	memset(&c, 0, sizeof(c));
	RawMatrix::setIdentity((RawMatrix*)c.mvp);
	RGBA white = { 255, 255, 255, 255 };
	colorToFloat(c.matColor, white);
	Camera *cam = (Camera*)engine->currentCamera;
	float w = cam && cam->frameBuffer ? (float)cam->frameBuffer->width : (float)d3d11Globals.presentWidth;
	float h = cam && cam->frameBuffer ? (float)cam->frameBuffer->height : (float)d3d11Globals.presentHeight;
	if(w <= 0.0f) w = 1.0f;
	if(h <= 0.0f) h = 1.0f;
	c.xform[0] = 2.0f/w;
	c.xform[1] = -2.0f/h;
	c.xform[2] = -1.0f;
	c.xform[3] = 1.0f;
	makeAlphaRef(c.alphaRef);
	updateConstants(&c);

	ID3D11ShaderResourceView *srv = getTextureSRV(textureRaster);
	d3d11Globals.context->PSSetShaderResources(0, 1, &srv);
	bindSampler();
	if(indices && numIndices > 0)
		d3d11Globals.context->DrawIndexed(numIndices, 0, 0);
	else
		d3d11Globals.context->Draw(numVertices, 0);
	rwFree(fanIndices);
}

static void
im2DRenderLine(void *vertices, int32, int32 vert1, int32 vert2)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex tmp[2] = { verts[vert1], verts[vert2] };
	draw2D(PRIMTYPELINELIST, tmp, 2, nil, 0);
}

static void
im2DRenderTriangle(void *vertices, int32, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex *verts = (Im2DVertex*)vertices;
	Im2DVertex tmp[3] = { verts[vert1], verts[vert2], verts[vert3] };
	draw2D(PRIMTYPETRILIST, tmp, 3, nil, 0);
}

static void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	draw2D(primType, vertices, numVertices, nil, 0);
}

static void
im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices)
{
	draw2D(primType, vertices, numVertices, indices, numIndices);
}

static void
draw3D(PrimitiveType primType, Im3DVertex *vertices, int32 numVertices,
	void *indices, int32 numIndices, const RawMatrix *mvp, const RGBA &matColor, Raster *textureRaster, bool32 alpha)
{
	uint16 *fanIndices = nil;
	if(primType == PRIMTYPETRIFAN){
		fanIndices = makeTriangleFanIndices(indices, numIndices, numVertices, &numIndices);
		if(fanIndices == nil)
			return;
		indices = fanIndices;
		primType = PRIMTYPETRILIST;
	}

	if(!validPrim(primType) || numVertices <= 0){
		rwFree(fanIndices);
		return;
	}
	uint32 vertexSize = (uint32)numVertices * sizeof(Im3DVertex);
	if(!uploadVertices(vertices, vertexSize)){
		rwFree(fanIndices);
		return;
	}
	if(indices && numIndices > 0 && !uploadIndices(indices, (uint32)numIndices*sizeof(uint16))){
		rwFree(fanIndices);
		return;
	}

	applyDrawState(alpha);
	d3d11Globals.context->IASetPrimitiveTopology(topologyFromPrim(primType));
	d3d11Globals.context->IASetInputLayout(d3d11Globals.layout3D);
	UINT stride = sizeof(Im3DVertex);
	UINT offset = 0;
	d3d11Globals.context->IASetVertexBuffers(0, 1, &d3d11Globals.dynamicVertexBuffer, &stride, &offset);
	if(indices && numIndices > 0)
		d3d11Globals.context->IASetIndexBuffer(d3d11Globals.dynamicIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	d3d11Globals.context->VSSetShader(d3d11Globals.vs3D, nil, 0);
	d3d11Globals.context->PSSetShader(d3d11Globals.ps3D, nil, 0);

	Constants c;
	memset(&c, 0, sizeof(c));
	memcpy(c.mvp, mvp, sizeof(RawMatrix));
	RawMatrix::setIdentity((RawMatrix*)c.world);
	RawMatrix::setIdentity((RawMatrix*)c.normal);
	colorToFloat(c.matColor, matColor);
	setDefaultSurface(&c, nil);
	setFogConstants(&c);
	makeAlphaRef(c.alphaRef);
	updateConstants(&c);

	ID3D11ShaderResourceView *srv = getTextureSRV(textureRaster);
	d3d11Globals.context->PSSetShaderResources(0, 1, &srv);
	bindSampler();
	if(indices && numIndices > 0)
		d3d11Globals.context->DrawIndexed(numIndices, 0, 0);
	else
		d3d11Globals.context->Draw(numVertices, 0);
	rwFree(fanIndices);
}

static void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	if(world == nil)
		d3d11Globals.im3dWorld.setIdentity();
	else
		d3d11Globals.im3dWorld = *world;
	d3d11Globals.im3dFlags = flags;
	d3d11Globals.im3dVertices = (Im3DVertex*)vertices;
	d3d11Globals.im3dNumVertices = numVertices;
}

static void
im3DRenderPrimitive(PrimitiveType primType)
{
	RGBA white = { 255, 255, 255, 255 };
	RawMatrix world, mvp;
	convMatrix(&world, &d3d11Globals.im3dWorld);
	RawMatrix viewproj;
	makeViewProj(&viewproj);
	RawMatrix::mult(&mvp, &world, &viewproj);
	Raster *tex = (d3d11Globals.im3dFlags & im3d::VERTEXUV) ?
		(Raster*)d3d11Globals.renderStates[TEXTURERASTER] : nil;
	bool32 alpha = rasterHasAlpha(tex) || getRenderStateUInt(VERTEXALPHA, 0) ||
		(d3d11Globals.im3dFlags & im3d::ALLOPAQUE) == 0;
	draw3D(primType, d3d11Globals.im3dVertices, d3d11Globals.im3dNumVertices,
		nil, 0, &mvp, white, tex, alpha);
}

static void
im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices)
{
	RGBA white = { 255, 255, 255, 255 };
	RawMatrix world, mvp;
	convMatrix(&world, &d3d11Globals.im3dWorld);
	RawMatrix viewproj;
	makeViewProj(&viewproj);
	RawMatrix::mult(&mvp, &world, &viewproj);
	Raster *tex = (d3d11Globals.im3dFlags & im3d::VERTEXUV) ?
		(Raster*)d3d11Globals.renderStates[TEXTURERASTER] : nil;
	bool32 alpha = rasterHasAlpha(tex) || getRenderStateUInt(VERTEXALPHA, 0) ||
		(d3d11Globals.im3dFlags & im3d::ALLOPAQUE) == 0;
	draw3D(primType, d3d11Globals.im3dVertices, d3d11Globals.im3dNumVertices,
		indices, numIndices, &mvp, white, tex, alpha);
}

static void
im3DEnd(void)
{
}

static V3d
transformVector(Matrix *m, const V3d &v)
{
	V3d out;
	out.x = m->right.x*v.x + m->up.x*v.y + m->at.x*v.z;
	out.y = m->right.y*v.x + m->up.y*v.y + m->at.y*v.z;
	out.z = m->right.z*v.x + m->up.z*v.y + m->at.z*v.z;
	return out;
}

static void
normalizeVector(V3d *v)
{
	float len = sqrtf(v->x*v->x + v->y*v->y + v->z*v->z);
	if(len > 0.0001f){
		v->x /= len;
		v->y /= len;
		v->z /= len;
	}
}

static float
dotVector(const V3d &a, const V3d &b)
{
	return a.x*b.x + a.y*b.y + a.z*b.z;
}

struct SimpleLightSet
{
	bool32 enabled;
	bool32 hasNormals;
	RGBAf ambient;
	Light *directionals[8];
	Light *locals[8];
	V3d directionalsNormalized[8];
	RGBAf directionalsColor[8];
	int32 numDirectionals;
	int32 numLocals;
};

static void
collectSimpleLights(Atomic *atomic, SimpleLightSet *lights)
{
	memset(lights, 0, sizeof(*lights));
	Geometry *geo = atomic->geometry;
	if(engine->currentWorld == nil)
		return;

	lights->hasNormals = (geo->flags & Geometry::NORMALS) != 0;
	if((geo->flags & Geometry::LIGHT) == 0){
		lights->enabled = 0;
		return;
	}

	WorldLights lightData;
	lightData.directionals = lights->directionals;
	lightData.numDirectionals = 8;
	lightData.locals = lights->locals;
	lightData.numLocals = 8;
	((World*)engine->currentWorld)->enumerateLights(atomic, &lightData);
	lights->ambient = lightData.ambient;
	lights->numDirectionals = lightData.numDirectionals;
	lights->numLocals = lightData.numLocals;
	lights->enabled = 1;

	for(int32 i = 0; i < lights->numDirectionals; i++){
		Light *l = lights->directionals[i];
		V3d dir = l->getFrame()->getLTM()->at;
		dir.x = -dir.x;
		dir.y = -dir.y;
		dir.z = -dir.z;
		normalizeVector(&dir);
		lights->directionalsNormalized[i] = dir;
		lights->directionalsColor[i] = l->color;
	}
}

static void
setLightConstants(Constants *c, Atomic *atomic)
{
	if(atomic == nil || atomic->geometry == nil ||
	   (atomic->geometry->flags & Geometry::LIGHT) == 0 ||
	   engine->currentWorld == nil)
		return;

	SimpleLightSet lights;
	collectSimpleLights(atomic, &lights);
	if(!lights.enabled)
		return;

	c->flags[0] = 1.0f;
	c->ambient[0] = lights.ambient.red;
	c->ambient[1] = lights.ambient.green;
	c->ambient[2] = lights.ambient.blue;
	c->ambient[3] = lights.ambient.alpha;

	int32 n = 0;
	for(int32 i = 0; i < lights.numDirectionals && n < 8; i++, n++){
		Light *l = lights.directionals[i];
		Matrix *m = l->getFrame()->getLTM();
		c->lightParams[n][0] = 1.0f;
		c->lightColor[n][0] = l->color.red;
		c->lightColor[n][1] = l->color.green;
		c->lightColor[n][2] = l->color.blue;
		c->lightColor[n][3] = l->color.alpha;
		c->lightDirection[n][0] = m->at.x;
		c->lightDirection[n][1] = m->at.y;
		c->lightDirection[n][2] = m->at.z;
	}

	for(int32 i = 0; i < lights.numLocals && n < 8; i++){
		Light *l = lights.locals[i];
		Matrix *m = l->getFrame()->getLTM();
		switch(l->getType()){
		case Light::POINT:
			c->lightParams[n][0] = 2.0f;
			c->lightParams[n][1] = l->radius;
			c->lightColor[n][0] = l->color.red;
			c->lightColor[n][1] = l->color.green;
			c->lightColor[n][2] = l->color.blue;
			c->lightColor[n][3] = l->color.alpha;
			c->lightPosition[n][0] = m->pos.x;
			c->lightPosition[n][1] = m->pos.y;
			c->lightPosition[n][2] = m->pos.z;
			n++;
			break;

		case Light::SPOT:
		case Light::SOFTSPOT:
			c->lightParams[n][0] = 3.0f;
			c->lightParams[n][1] = l->radius;
			c->lightParams[n][2] = l->minusCosAngle;
			c->lightParams[n][3] = l->getType() == Light::SOFTSPOT ? 0.0f : 1.0f;
			c->lightColor[n][0] = l->color.red;
			c->lightColor[n][1] = l->color.green;
			c->lightColor[n][2] = l->color.blue;
			c->lightColor[n][3] = l->color.alpha;
			c->lightPosition[n][0] = m->pos.x;
			c->lightPosition[n][1] = m->pos.y;
			c->lightPosition[n][2] = m->pos.z;
			c->lightDirection[n][0] = m->at.x;
			c->lightDirection[n][1] = m->at.y;
			c->lightDirection[n][2] = m->at.z;
			n++;
			break;
		}
	}
}

static RGBA
applySimpleLighting(SimpleLightSet *lights, Matrix *world, const V3d &normal, const RGBA &base)
{
	if(!lights->enabled)
		return base;

	if(!lights->hasNormals){
		float r = base.red / 255.0f + lights->ambient.red;
		float g = base.green / 255.0f + lights->ambient.green;
		float b = base.blue / 255.0f + lights->ambient.blue;
		if(r > 1.0f) r = 1.0f;
		if(g > 1.0f) g = 1.0f;
		if(b > 1.0f) b = 1.0f;
		RGBA out = base;
		out.red = (uint8)(r*255.0f);
		out.green = (uint8)(g*255.0f);
		out.blue = (uint8)(b*255.0f);
		return out;
	}

	V3d n = transformVector(world, normal);
	normalizeVector(&n);

	float r = base.red / 255.0f + lights->ambient.red;
	float g = base.green / 255.0f + lights->ambient.green;
	float b = base.blue / 255.0f + lights->ambient.blue;

	for(int32 i = 0; i < lights->numDirectionals; i++){
		float f = dotVector(n, lights->directionalsNormalized[i]);
		if(f > 0.0f){
			r += f*lights->directionalsColor[i].red;
			g += f*lights->directionalsColor[i].green;
			b += f*lights->directionalsColor[i].blue;
		}
	}

	if(r > 1.0f) r = 1.0f;
	if(g > 1.0f) g = 1.0f;
	if(b > 1.0f) b = 1.0f;
	RGBA out = base;
	out.red = (uint8)(r*255.0f);
	out.green = (uint8)(g*255.0f);
	out.blue = (uint8)(b*255.0f);
	return out;
}

static bool32
ensureTempGeometry(uint32 numVertices)
{
	if(d3d11Globals.tempVertexCapacity < numVertices){
		rwFree(d3d11Globals.tempVertices);
		d3d11Globals.tempVertices = rwNewT(Im3DVertex, numVertices, MEMDUR_EVENT | ID_DRIVER);
		d3d11Globals.tempVertexCapacity = numVertices;
	}
	return d3d11Globals.tempVertices != nil;
}

static bool32
geometryHasVertexAlpha(Geometry *geo)
{
	if((geo->flags & Geometry::PRELIT) == 0 || geo->colors == nil)
		return 0;
	for(int32 i = 0; i < geo->numVertices; i++)
		if(geo->colors[i].alpha != 0xFF)
			return 1;
	return 0;
}

static bool32
createStaticBuffer(ID3D11Buffer **buffer, uint32 bindFlags, const void *data, uint32 size)
{
	releaseIUnknown(*buffer);
	*buffer = nil;
	if(size == 0)
		return 1;
	if(d3d11Globals.device == nil)
		return 1;

	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = size;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bindFlags;

	D3D11_SUBRESOURCE_DATA sub;
	memset(&sub, 0, sizeof(sub));
	sub.pSysMem = data;
	return SUCCEEDED(d3d11Globals.device->CreateBuffer(&desc, &sub, buffer));
}

static bool32
ensureInstanceBuffers(InstanceDataHeader *header)
{
	if(header == nil)
		return 0;
	if(header->vertexBuffer && header->indexBuffer && !header->gpuDirty)
		return 1;
	return createStaticBuffer(&header->vertexBuffer, D3D11_BIND_VERTEX_BUFFER,
		header->vertices, header->totalNumVertex*sizeof(Im3DVertex)) &&
		createStaticBuffer(&header->indexBuffer, D3D11_BIND_INDEX_BUFFER,
		header->indices, header->totalNumIndex*sizeof(uint16)) &&
		(header->gpuDirty = 0, 1);
}

void
freeInstanceData(Geometry *geometry)
{
	if(geometry == nil || geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_D3D11)
		return;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	releaseIUnknown(header->vertexBuffer);
	releaseIUnknown(header->indexBuffer);
	rwFree(header->vertices);
	rwFree(header->indices);
	rwFree(header->inst);
	rwFree(header);
}

void*
destroyNativeData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

static uint32
numPrimitivesFor(PrimitiveType primType, uint32 numIndices)
{
	if(primType == PRIMTYPETRISTRIP)
		return numIndices >= 3 ? numIndices-2 : 0;
	return numIndices/3;
}

static InstanceDataHeader*
instanceMesh(Geometry *geo)
{
	MeshHeader *meshh = geo->meshHeader;
	if(meshh == nil)
		return nil;

	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY);
	if(header == nil)
		return nil;
	memset(header, 0, sizeof(*header));
	header->platform = PLATFORM_D3D11;
	header->serialNumber = meshh->serialNum;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == MeshHeader::TRISTRIP ? PRIMTYPETRISTRIP : PRIMTYPETRILIST;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->gpuDirty = 1;
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);
	header->indices = rwNewT(uint16, header->totalNumIndex, MEMDUR_EVENT | ID_GEOMETRY);
	if(header->inst == nil || header->indices == nil){
		rwFree(header->inst);
		rwFree(header->indices);
		rwFree(header);
		return nil;
	}

	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->getMeshes();
	uint32 startIndex = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		memset(inst, 0, sizeof(*inst));
		if(mesh->indices && mesh->numIndices)
			findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
				&inst->minVert, (int32*)&inst->numVertices);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->baseIndex = 0;
		inst->startIndex = startIndex;
		inst->numPrimitives = numPrimitivesFor((PrimitiveType)header->primType, inst->numIndex);
		if(mesh->indices && mesh->numIndices)
			memcpy(&header->indices[startIndex], mesh->indices, mesh->numIndices*sizeof(uint16));
		startIndex += mesh->numIndices;
		mesh++;
		inst++;
	}
	return header;
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32)
{
	if(geo == nil || header == nil || geo->numVertices <= 0 || geo->morphTargets == nil)
		return;
	if(header->vertices == nil)
		header->vertices = rwNewT(Im3DVertex, geo->numVertices, MEMDUR_EVENT | ID_GEOMETRY);
	if(header->vertices == nil)
		return;

	bool32 hasNormals = !!(geo->flags & Geometry::NORMALS);
	bool32 hasPrelit = !!(geo->flags & Geometry::PRELIT);
	bool32 hasTex = geo->numTexCoordSets > 0 && geo->texCoords[0] != nil;
	RGBA white = { 255, 255, 255, 255 };
	RGBA black = { 0, 0, 0, 255 };
	MorphTarget *morph = &geo->morphTargets[0];

	for(int32 i = 0; i < geo->numVertices; i++){
		Im3DVertex &v = header->vertices[i];
		v.position = morph->vertices[i];
		if(hasNormals)
			v.normal = morph->normals[i];
		else{
			v.normal.x = 0.0f;
			v.normal.y = 0.0f;
			v.normal.z = 1.0f;
		}
		RGBA color = hasPrelit ? geo->colors[i] :
			((geo->flags & Geometry::LIGHT) ? black : white);
		v.r = color.red;
		v.g = color.green;
		v.b = color.blue;
		v.a = color.alpha;
		if(hasTex){
			v.u = geo->texCoords[0][i].u;
			v.v = geo->texCoords[0][i].v;
		}else{
			v.u = 0.0f;
			v.v = 0.0f;
		}
	}

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		inst->vertexAlpha = 0;
		uint32 end = inst->minVert + inst->numVertices;
		if(end > header->totalNumVertex)
			end = header->totalNumVertex;
		for(uint32 j = inst->minVert; j < end; j++)
			if(header->vertices[j].a != 0xFF){
				inst->vertexAlpha = 1;
				break;
			}
		inst++;
	}

	header->gpuDirty = 1;
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	if(geo == nil || header == nil || header->vertices == nil ||
	   geo->morphTargets == nil || geo->numVertices <= 0)
		return;

	MorphTarget *morph = &geo->morphTargets[0];
	for(int32 i = 0; i < geo->numVertices; i++){
		Im3DVertex &v = header->vertices[i];
		morph->vertices[i] = v.position;
		if((geo->flags & Geometry::NORMALS) && morph->normals)
			morph->normals[i] = v.normal;
		if((geo->flags & Geometry::PRELIT) && geo->colors)
			geo->colors[i] = makeRGBA(v.r, v.g, v.b, v.a);
		if(geo->numTexCoordSets > 0 && geo->texCoords[0]){
			geo->texCoords[0][i].u = v.u;
			geo->texCoords[0][i].v = v.v;
		}
	}
}

Stream*
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	uint32 platform = stream->readU32();
	if(platform != PLATFORM_D3D11){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}
	uint32 version = stream->readU32();
	if(version != 1){
		RWERROR((ERR_GENERAL, "unsupported D3D11 native geometry"));
		return nil;
	}

	InstanceDataHeader *header = rwNewT(InstanceDataHeader, 1, MEMDUR_EVENT | ID_GEOMETRY);
	if(header == nil)
		return nil;
	memset(header, 0, sizeof(*header));
	header->platform = PLATFORM_D3D11;
	header->serialNumber = stream->readU32();
	header->numMeshes = stream->readU32();
	header->primType = stream->readU32();
	header->totalNumVertex = stream->readU32();
	header->totalNumIndex = stream->readU32();
	header->inst = rwNewT(InstanceData, header->numMeshes, MEMDUR_EVENT | ID_GEOMETRY);
	header->vertices = rwNewT(Im3DVertex, header->totalNumVertex, MEMDUR_EVENT | ID_GEOMETRY);
	header->indices = rwNewT(uint16, header->totalNumIndex, MEMDUR_EVENT | ID_GEOMETRY);
	if(header->inst == nil || header->vertices == nil || header->indices == nil){
		rwFree(header->inst);
		rwFree(header->vertices);
		rwFree(header->indices);
		rwFree(header);
		return nil;
	}

	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *inst = &header->inst[i];
		inst->numIndex = stream->readU32();
		inst->minVert = stream->readU32();
		uint32 matId = stream->readU32();
		inst->material = matId < (uint32)geometry->matList.numMaterials ?
			geometry->matList.materials[matId] : nil;
		inst->vertexAlpha = stream->readU32();
		inst->baseIndex = stream->readU32();
		inst->numVertices = stream->readU32();
		inst->startIndex = stream->readU32();
		inst->numPrimitives = stream->readU32();
	}
	stream->read8(header->vertices, header->totalNumVertex*sizeof(Im3DVertex));
	stream->read16(header->indices, header->totalNumIndex*sizeof(uint16));
	header->gpuDirty = 1;
	geometry->instData = header;
	ensureInstanceBuffers(header);
	return stream;
}

Stream*
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_STRUCT, len-12);
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_D3D11)
		return stream;

	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	stream->writeU32(PLATFORM_D3D11);
	stream->writeU32(1);
	stream->writeU32(header->serialNumber);
	stream->writeU32(header->numMeshes);
	stream->writeU32(header->primType);
	stream->writeU32(header->totalNumVertex);
	stream->writeU32(header->totalNumIndex);

	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *inst = &header->inst[i];
		stream->writeU32(inst->numIndex);
		stream->writeU32(inst->minVert);
		stream->writeU32(geometry->matList.findIndex(inst->material));
		stream->writeU32(inst->vertexAlpha);
		stream->writeU32(inst->baseIndex);
		stream->writeU32(inst->numVertices);
		stream->writeU32(inst->startIndex);
		stream->writeU32(inst->numPrimitives);
	}
	stream->write8(header->vertices, header->totalNumVertex*sizeof(Im3DVertex));
	stream->write16(header->indices, header->totalNumIndex*sizeof(uint16));
	return stream;
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_D3D11)
		return 0;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	return 12 + 4 + 4 + 5*4 + header->numMeshes*8*4 +
		header->totalNumVertex*sizeof(Im3DVertex) +
		header->totalNumIndex*sizeof(uint16);
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if(geo == nil || (geo->flags & Geometry::NATIVE))
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	if(header){
		assert(header->platform == PLATFORM_D3D11);
		if(geo->meshHeader && header->serialNumber != geo->meshHeader->serialNum)
			freeInstanceData(geo);
	}

	if(geo->instData == nil){
		geo->instData = instanceMesh(geo);
		header = (InstanceDataHeader*)geo->instData;
		if(header && pipe->instanceCB)
			pipe->instanceCB(geo, header, 0);
	}else if(geo->lockedSinceInst && pipe->instanceCB){
		header = (InstanceDataHeader*)geo->instData;
		pipe->instanceCB(geo, header, 1);
	}
	header = (InstanceDataHeader*)geo->instData;
	if(header)
		ensureInstanceBuffers(header);
	geo->lockedSinceInst = 0;
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if(geo == nil || (geo->flags & Geometry::NATIVE) == 0 ||
	   geo->instData == nil || geo->instData->platform != PLATFORM_D3D11)
		return;

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	geo->numTriangles = geo->meshHeader->guessNumTriangles();
	geo->allocateData();
	geo->allocateMeshes(geo->meshHeader->numMeshes, geo->meshHeader->totalIndices, 0);

	Mesh *mesh = geo->meshHeader->getMeshes();
	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *inst = &header->inst[i];
		memcpy(mesh[i].indices, &header->indices[inst->startIndex], inst->numIndex*sizeof(uint16));
	}
	if(pipe->uninstanceCB)
		pipe->uninstanceCB(geo, header);
	geo->generateTriangles();
	geo->flags &= ~Geometry::NATIVE;
	destroyNativeData(geo, 0, 0);
}

static void
setTextureStates(Texture *tex)
{
	if(tex && tex->raster){
		setRenderState(TEXTUREFILTER, (void*)(uintptr)tex->getFilter());
		setRenderState(TEXTUREADDRESSU, (void*)(uintptr)tex->getAddressU());
		setRenderState(TEXTUREADDRESSV, (void*)(uintptr)tex->getAddressV());
		setRenderState(TEXTURERASTER, tex->raster);
	}else
		setRenderState(TEXTURERASTER, nil);
}

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Geometry *geo = atomic->geometry;
	if(geo == nil || header == nil || header->numMeshes == 0 ||
	   !ensureInstanceBuffers(header))
		return;

	PrimitiveType primType = (PrimitiveType)header->primType;
	if(!validPrim(primType))
		return;

	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	RawMatrix worldRaw, viewproj, mvp;
	convMatrix(&worldRaw, world);
	makeViewProj(&viewproj);
	RawMatrix::mult(&mvp, &worldRaw, &viewproj);

	d3d11Globals.context->IASetPrimitiveTopology(topologyFromPrim(primType));
	d3d11Globals.context->IASetInputLayout(d3d11Globals.layout3D);
	UINT stride = sizeof(Im3DVertex);
	UINT offset = 0;
	d3d11Globals.context->IASetVertexBuffers(0, 1, &header->vertexBuffer, &stride, &offset);
	d3d11Globals.context->IASetIndexBuffer(header->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
	d3d11Globals.context->VSSetShader(d3d11Globals.vs3D, nil, 0);
	d3d11Globals.context->PSSetShader(d3d11Globals.ps3D, nil, 0);

	RGBA white = { 255, 255, 255, 255 };
	SurfaceProperties defaultSurface = { 1.0f, 0.0f, 1.0f };
	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *inst = &header->inst[i];
		Material *mat = inst->material;
		RGBA matColor = white;
		SurfaceProperties *surface = &defaultSurface;
		Texture *texture = nil;
		Raster *texRaster = nil;
		if(mat){
			if(geo->flags & Geometry::MODULATE)
				matColor = mat->color;
			surface = &mat->surfaceProps;
			texture = mat->texture;
			if(texture)
				texRaster = texture->raster;
		}

		setTextureStates(texture);
		SetRenderState(VERTEXALPHA, inst->vertexAlpha || matColor.alpha != 0xFF);
		bool32 alpha = inst->vertexAlpha || matColor.alpha != 0xFF ||
			rasterHasAlpha(texRaster) || getRenderStateUInt(VERTEXALPHA, 0);
		applyDrawState(alpha);

		Constants c;
		memset(&c, 0, sizeof(c));
		memcpy(c.mvp, &mvp, sizeof(RawMatrix));
		memcpy(c.world, &worldRaw, sizeof(RawMatrix));
		memcpy(c.normal, &worldRaw, sizeof(RawMatrix));
		colorToFloat(c.matColor, matColor);
		setDefaultSurface(&c, surface);
		setFogConstants(&c);
		makeAlphaRef(c.alphaRef);
		setLightConstants(&c, atomic);
		updateConstants(&c);

		ID3D11ShaderResourceView *srv = getTextureSRV(texRaster);
		d3d11Globals.context->PSSetShaderResources(0, 1, &srv);
		bindSampler();
		d3d11Globals.context->DrawIndexed(inst->numIndex, inst->startIndex, 0);
	}
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	pipe->instance(atomic);
	if(geo == nil || geo->instData == nil ||
	   geo->instData->platform != PLATFORM_D3D11)
		return;
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

void
ObjPipeline::init(void)
{
	this->rw::ObjPipeline::init(PLATFORM_D3D11);
	this->impl.instance = d3d11::instance;
	this->impl.uninstance = d3d11::uninstance;
	this->impl.render = d3d11::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}

ObjPipeline*
ObjPipeline::create(void)
{
	ObjPipeline *pipe = rwNewT(ObjPipeline, 1, MEMDUR_GLOBAL);
	pipe->init();
	return pipe;
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

static void
skinRenderCB(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo == nil || geo->numVertices <= 0 || geo->meshHeader == nil ||
	   geo->morphTargets == nil)
		return;
	MeshHeader *meshh = geo->meshHeader;
	if(!ensureTempGeometry(geo->numVertices))
		return;

	Skin *skin = Skin::get(geo);
	HAnimHierarchy *hier = skin ? Skin::getHierarchy(atomic) : nil;

	static Matrix skinMats[256];
	int32 numBones = skin ? skin->numBones : 0;
	if(numBones > 256)
		numBones = 256;

	if(skin && hier){
		Matrix *invMats = (Matrix*)skin->inverseMatrices;
		if(hier->flags & HAnimHierarchy::LOCALSPACEMATRICES){
			for(int32 i = 0; i < numBones; i++){
				invMats[i].flags = 0;
				Matrix::mult(&skinMats[i], &invMats[i], &hier->matrices[i]);
			}
		}else{
			Matrix ident;
			ident.setIdentity();
			Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
			Matrix invAtmMat;
			Matrix::invert(&invAtmMat, world);
			Matrix tmp;
			for(int32 i = 0; i < numBones; i++){
				invMats[i].flags = 0;
				Matrix::mult(&tmp, &hier->matrices[i], &invAtmMat);
				Matrix::mult(&skinMats[i], &invMats[i], &tmp);
			}
		}
	}else{
		for(int32 i = 0; i < numBones; i++)
			skinMats[i].setIdentity();
	}

	bool32 hasNormals = !!(geo->flags & Geometry::NORMALS);
	bool32 hasPrelit = !!(geo->flags & Geometry::PRELIT);
	bool32 hasTex = geo->numTexCoordSets > 0 && geo->texCoords[0] != nil;
	MorphTarget *morph = &geo->morphTargets[0];
	RGBA white = { 255, 255, 255, 255 };
	RGBA black = { 0, 0, 0, 255 };
	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	SimpleLightSet lights;
	collectSimpleLights(atomic, &lights);

	for(int32 i = 0; i < geo->numVertices; i++){
		Im3DVertex &v = d3d11Globals.tempVertices[i];
		V3d srcPos = morph->vertices[i];
		V3d srcNrm;
		if(hasNormals)
			srcNrm = morph->normals[i];
		else{
			srcNrm.x = 0.0f;
			srcNrm.y = 0.0f;
			srcNrm.z = 1.0f;
		}

		if(skin && skin->weights && skin->indices){
			V3d pos = { 0.0f, 0.0f, 0.0f };
			V3d nrm = { 0.0f, 0.0f, 0.0f };
			uint8 *idx = &skin->indices[i*4];
			float *wgt = &skin->weights[i*4];
			for(int32 j = 0; j < 4; j++){
				float w = wgt[j];
				if(w == 0.0f || idx[j] >= numBones)
					continue;
				Matrix *m = &skinMats[idx[j]];
				V3d tp;
				tp.x = m->right.x*srcPos.x + m->up.x*srcPos.y + m->at.x*srcPos.z + m->pos.x;
				tp.y = m->right.y*srcPos.x + m->up.y*srcPos.y + m->at.y*srcPos.z + m->pos.y;
				tp.z = m->right.z*srcPos.x + m->up.z*srcPos.y + m->at.z*srcPos.z + m->pos.z;
				pos.x += w*tp.x;
				pos.y += w*tp.y;
				pos.z += w*tp.z;
				V3d tn;
				tn.x = m->right.x*srcNrm.x + m->up.x*srcNrm.y + m->at.x*srcNrm.z;
				tn.y = m->right.y*srcNrm.x + m->up.y*srcNrm.y + m->at.y*srcNrm.z;
				tn.z = m->right.z*srcNrm.x + m->up.z*srcNrm.y + m->at.z*srcNrm.z;
				nrm.x += w*tn.x;
				nrm.y += w*tn.y;
				nrm.z += w*tn.z;
			}
			float len = sqrtf(nrm.x*nrm.x + nrm.y*nrm.y + nrm.z*nrm.z);
			if(len > 0.0001f){
				nrm.x /= len;
				nrm.y /= len;
				nrm.z /= len;
			}else
				nrm = srcNrm;
			v.position = pos;
			v.normal = nrm;
		}else{
			v.position = srcPos;
			v.normal = srcNrm;
		}

		RGBA c = hasPrelit ? geo->colors[i] :
			((geo->flags & Geometry::LIGHT) ? black : white);
		c = applySimpleLighting(&lights, world, v.normal, c);
		v.r = c.red;
		v.g = c.green;
		v.b = c.blue;
		v.a = c.alpha;
		if(hasTex){
			v.u = geo->texCoords[0][i].u;
			v.v = geo->texCoords[0][i].v;
		}else{
			v.u = 0.0f;
			v.v = 0.0f;
		}
	}

	PrimitiveType primType = meshh->flags == MeshHeader::TRISTRIP ? PRIMTYPETRISTRIP : PRIMTYPETRILIST;
	if(!validPrim(primType))
		return;

	RawMatrix worldRaw, viewproj, mvp;
	convMatrix(&worldRaw, world);
	makeViewProj(&viewproj);
	RawMatrix::mult(&mvp, &worldRaw, &viewproj);

	bool32 vertexAlpha = geometryHasVertexAlpha(geo);
	Mesh *mesh = meshh->getMeshes();
	for(uint32 i = 0; i < meshh->numMeshes; i++){
		if(mesh[i].numIndices <= 0)
			continue;
		Material *mat = mesh[i].material;
		RGBA matColor = white;
		Texture *texture = nil;
		Raster *texRaster = nil;
		if(mat){
			if(geo->flags & Geometry::MODULATE)
				matColor = mat->color;
			texture = mat->texture;
			if(texture)
				texRaster = texture->raster;
		}
		setTextureStates(texture);
		bool32 meshAlpha = vertexAlpha || matColor.alpha != 0xFF;
		SetRenderState(VERTEXALPHA, meshAlpha);
		bool32 alpha = meshAlpha || rasterHasAlpha(texRaster);
		draw3D(primType, d3d11Globals.tempVertices, geo->numVertices,
			mesh[i].indices, mesh[i].numIndices, &mvp, matColor, texRaster, alpha);
	}
}

static void
skinPipeInstance(rw::ObjPipeline*, Atomic*)
{
}

static void
skinPipeUninstance(rw::ObjPipeline*, Atomic*)
{
}

static void
skinPipeRender(rw::ObjPipeline*, Atomic *atomic)
{
	skinRenderCB(atomic);
}

static ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->impl.instance = skinPipeInstance;
	pipe->impl.uninstance = skinPipeUninstance;
	pipe->impl.render = skinPipeRender;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

static void*
d3d11SkinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_D3D11] = makeSkinPipeline();
	return o;
}

static void*
d3d11SkinClose(void *o, int32, int32)
{
	if(skinGlobals.pipelines[PLATFORM_D3D11]){
		((ObjPipeline*)skinGlobals.pipelines[PLATFORM_D3D11])->destroy();
		skinGlobals.pipelines[PLATFORM_D3D11] = nil;
	}
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, ID_SKIN,
	                       d3d11SkinOpen, d3d11SkinClose);
}

static void
buildEnvMapVertices(Atomic *atomic, InstanceDataHeader *header, MatFX::Env *env)
{
	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	Frame *envFrame = env->frame ? env->frame : engine->currentCamera->getFrame();
	Matrix invEnv;
	Matrix::invert(&invEnv, envFrame->getLTM());
	float32 uscale = MatFX::envMapFlipU ? -0.5f : 0.5f;

	for(uint32 i = 0; i < header->totalNumVertex; i++){
		d3d11Globals.tempVertices[i] = header->vertices[i];
		V3d n = transformVector(world, header->vertices[i].normal);
		normalizeVector(&n);
		n = transformVector(&invEnv, n);
		d3d11Globals.tempVertices[i].u = n.x*uscale + 0.5f;
		d3d11Globals.tempVertices[i].v = n.y*-0.5f + 0.5f;
	}
}

static RGBA
makeEnvColor(Material *mat, MatFX::Env *env)
{
	RGBA c = MatFX::envMapUseMatColor && mat ? mat->color : MatFX::envMapColor;
	float32 coef = env->coefficient;
	if(coef < 0.0f)
		coef = 0.0f;
	float32 r = c.red * coef;
	float32 g = c.green * coef;
	float32 b = c.blue * coef;
	if(r > 255.0f) r = 255.0f;
	if(g > 255.0f) g = 255.0f;
	if(b > 255.0f) b = 255.0f;
	c.red = (uint8)r;
	c.green = (uint8)g;
	c.blue = (uint8)b;
	c.alpha = 0;
	return c;
}

static void
matFXRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	defaultRenderCB(atomic, header);
	Geometry *geo = atomic->geometry;
	if(geo == nil || header == nil || header->vertices == nil ||
	   (geo->flags & Geometry::NORMALS) == 0)
		return;
	if(!ensureTempGeometry(header->totalNumVertex))
		return;

	PrimitiveType primType = (PrimitiveType)header->primType;
	if(!validPrim(primType))
		return;

	Matrix ident;
	ident.setIdentity();
	Matrix *world = atomic->getFrame() ? atomic->getFrame()->getLTM() : &ident;
	RawMatrix worldRaw, viewproj, mvp;
	convMatrix(&worldRaw, world);
	makeViewProj(&viewproj);
	RawMatrix::mult(&mvp, &worldRaw, &viewproj);

	uint32 oldSrc = getRenderStateUInt(SRCBLEND, BLENDSRCALPHA);
	uint32 oldVertexAlpha = getRenderStateUInt(VERTEXALPHA, 0);
	setRenderState(SRCBLEND, (void*)(uintptr)BLENDONE);
	setRenderState(VERTEXALPHA, (void*)(uintptr)1);

	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *inst = &header->inst[i];
		Material *mat = inst->material;
		MatFX *matfx = mat ? MatFX::get(mat) : nil;
		if(matfx == nil || matfx->type != MatFX::ENVMAP)
			continue;
		MatFX::Env *env = &matfx->fx[0].env;
		if(env->tex == nil || env->tex->raster == nil || env->coefficient == 0.0f)
			continue;

		setTextureStates(env->tex);
		buildEnvMapVertices(atomic, header, env);
		RGBA envColor = makeEnvColor(mat, env);
		draw3D(primType, d3d11Globals.tempVertices, header->totalNumVertex,
			&header->indices[inst->startIndex], inst->numIndex,
			&mvp, envColor, env->tex->raster, 1);
	}

	setRenderState(SRCBLEND, (void*)(uintptr)oldSrc);
	setRenderState(VERTEXALPHA, (void*)(uintptr)oldVertexAlpha);
}

static ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = makeDefaultPipeline();
	pipe->renderCB = matFXRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 1;
	return pipe;
}

static void*
d3d11MatFXOpen(void *o, int32, int32)
{
	matFXGlobals.pipelines[PLATFORM_D3D11] = makeMatFXPipeline();
	return o;
}

static void*
d3d11MatFXClose(void *o, int32, int32)
{
	if(matFXGlobals.pipelines[PLATFORM_D3D11]){
		((ObjPipeline*)matFXGlobals.pipelines[PLATFORM_D3D11])->destroy();
		matFXGlobals.pipelines[PLATFORM_D3D11] = nil;
	}
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, ID_MATFX,
	                       d3d11MatFXOpen, d3d11MatFXClose);
}

static void
releaseSwapChainResources(void)
{
	if(d3d11Globals.context)
		d3d11Globals.context->OMSetRenderTargets(0, nil, nil);
	releaseIUnknown(d3d11Globals.backBufferRTV);
	releaseIUnknown(d3d11Globals.depthDSV);
	releaseIUnknown(d3d11Globals.depthTexture);
	d3d11Globals.backBufferRTV = nil;
	d3d11Globals.depthDSV = nil;
	d3d11Globals.depthTexture = nil;
}

static bool32
createBackBuffer(uint32 width, uint32 height)
{
	ID3D11Texture2D *backBuffer = nil;
	if(FAILED(d3d11Globals.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
		return 0;
	HRESULT hr = d3d11Globals.device->CreateRenderTargetView(backBuffer, nil, &d3d11Globals.backBufferRTV);
	releaseIUnknown(backBuffer);
	if(FAILED(hr))
		return 0;

	D3D11_TEXTURE2D_DESC ddesc;
	memset(&ddesc, 0, sizeof(ddesc));
	ddesc.Width = width;
	ddesc.Height = height;
	ddesc.MipLevels = 1;
	ddesc.ArraySize = 1;
	ddesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	ddesc.SampleDesc.Count = 1;
	ddesc.Usage = D3D11_USAGE_DEFAULT;
	ddesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if(FAILED(d3d11Globals.device->CreateTexture2D(&ddesc, nil, &d3d11Globals.depthTexture)))
		return 0;
	if(FAILED(d3d11Globals.device->CreateDepthStencilView(d3d11Globals.depthTexture, nil, &d3d11Globals.depthDSV)))
		return 0;

	d3d11Globals.presentWidth = width;
	d3d11Globals.presentHeight = height;
	return 1;
}

static void
getClientSize(uint32 *width, uint32 *height)
{
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
	int w, h;
	SDL_GetWindowSize(*d3d11Globals.pWindow, &w, &h);
	*width = w;
	*height = h;
#elif defined(LIBRW_GLFW)
	int w, h;
	glfwGetWindowSize(*d3d11Globals.pWindow, &w, &h);
	*width = w;
	*height = h;
#else
	RECT r;
	GetClientRect(d3d11Globals.window, &r);
	*width = (uint32)(r.right - r.left);
	*height = (uint32)(r.bottom - r.top);
#endif
	if(*width == 0) *width = 1;
	if(*height == 0) *height = 1;
}

static bool32
resizeIfNeeded(void)
{
	uint32 width, height;
	getClientSize(&width, &height);
	if(width == d3d11Globals.presentWidth && height == d3d11Globals.presentHeight)
		return 1;
	releaseSwapChainResources();
	if(FAILED(d3d11Globals.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
		return 0;
	return createBackBuffer(width, height);
}

#ifdef LIBRW_SDL3
static void
makeVideoModeList(void)
{
	int numDisplays = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
	if (displays && numDisplays > 0) {
		SDL_DisplayID displayId = displays[0];
		const SDL_DisplayMode *currentMode = SDL_GetCurrentDisplayMode(displayId);
		int numModes = 0;
		SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(displayId, &numModes);
		
		int totalModes = numModes + (currentMode ? 1 : 0);
		if (totalModes > 0) {
			if (d3d11Globals.modes) {
				rwFree(d3d11Globals.modes);
			}
			d3d11Globals.modes = rwNewT(VideoMode, totalModes, ID_DRIVER | MEMDUR_EVENT);
			d3d11Globals.numModes = 0;
			
			if (currentMode) {
				d3d11Globals.modes[0].width = currentMode->w;
				d3d11Globals.modes[0].height = currentMode->h;
				d3d11Globals.modes[0].depth = 32;
				d3d11Globals.modes[0].flags = 0;
				d3d11Globals.numModes = 1;
			}
			
			if (modes) {
				for (int i = 0; i < numModes; i++) {
					bool duplicate = false;
					for (int j = 1; j < d3d11Globals.numModes; j++) {
						if (d3d11Globals.modes[j].width == modes[i]->w &&
							d3d11Globals.modes[j].height == modes[i]->h) {
							duplicate = true;
							break;
						}
					}
					if (!duplicate) {
						int idx = d3d11Globals.numModes;
						d3d11Globals.modes[idx].width = modes[i]->w;
						d3d11Globals.modes[idx].height = modes[i]->h;
						d3d11Globals.modes[idx].depth = 32;
						d3d11Globals.modes[idx].flags = VIDEOMODEEXCLUSIVE;
						d3d11Globals.numModes++;
					}
				}
				SDL_free(modes);
			}
		}
		SDL_free(displays);
	}
}
#endif

#ifdef LIBRW_SDL3
static int
startSDL3(void)
{
	printf("DEBUG: startSDL3 start\n");
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)){
		printf("DEBUG: startSDL3 SDL_InitSubSystem failed\n");
		return 0;
	}
	makeVideoModeList();

	// Synchronize width/height with the chosen mode if it is exclusive fullscreen
	if (d3d11Globals.mode.width > 0 && d3d11Globals.mode.height > 0 && (d3d11Globals.mode.flags & VIDEOMODEEXCLUSIVE)) {
		d3d11Globals.winWidth = d3d11Globals.mode.width;
		d3d11Globals.winHeight = d3d11Globals.mode.height;
	}

	// If there's already a valid window, reuse it (video mode reinit path)
	if(d3d11Globals.pWindow && *d3d11Globals.pWindow) {
		SDL_Window *existing = *d3d11Globals.pWindow;
		printf("DEBUG: startSDL3 reusing existing window: %p\n", existing);
		
		if (d3d11Globals.mode.flags & VIDEOMODEEXCLUSIVE) {
			int numDisplays = 0;
			SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
			if (displays && numDisplays > 0) {
				SDL_DisplayID displayId = displays[0];
				int numModes = 0;
				SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(displayId, &numModes);
				if (modes) {
					const SDL_DisplayMode *targetMode = NULL;
					for (int i = 0; i < numModes; i++) {
						if (modes[i]->w == d3d11Globals.winWidth &&
							modes[i]->h == d3d11Globals.winHeight) {
							targetMode = modes[i];
							break;
						}
					}
					if (targetMode) {
						SDL_SetWindowFullscreenMode(existing, targetMode);
					}
					SDL_free(modes);
				}
				SDL_free(displays);
			}
			SDL_SetWindowFullscreen(existing, true);
		} else {
			SDL_SetWindowFullscreen(existing, false);
			SDL_SetWindowSize(existing, d3d11Globals.winWidth, d3d11Globals.winHeight);
		}

		d3d11Globals.window = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(existing), "SDL.window.win32.hwnd", NULL);
		printf("DEBUG: startSDL3 native HWND retrieved: %p\n", d3d11Globals.window);
		return 1;
	}
	printf("DEBUG: startSDL3 SDL_CreateWindow width: %d, height: %d, title: %s\n", d3d11Globals.winWidth, d3d11Globals.winHeight, d3d11Globals.winTitle);
	SDL_Window *win = SDL_CreateWindow(d3d11Globals.winTitle, d3d11Globals.winWidth, d3d11Globals.winHeight, SDL_WINDOW_RESIZABLE);
	if(win == nil){
		printf("DEBUG: startSDL3 SDL_CreateWindow failed, error: %s\n", SDL_GetError());
		return 0;
	}

	if (d3d11Globals.mode.flags & VIDEOMODEEXCLUSIVE) {
		int numDisplays = 0;
		SDL_DisplayID *displays = SDL_GetDisplays(&numDisplays);
		if (displays && numDisplays > 0) {
			SDL_DisplayID displayId = displays[0];
			int numModes = 0;
			SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(displayId, &numModes);
			if (modes) {
				const SDL_DisplayMode *targetMode = NULL;
				for (int i = 0; i < numModes; i++) {
					if (modes[i]->w == d3d11Globals.winWidth &&
						modes[i]->h == d3d11Globals.winHeight) {
						targetMode = modes[i];
						break;
					}
				}
				if (targetMode) {
					SDL_SetWindowFullscreenMode(win, targetMode);
				}
				SDL_free(modes);
			}
			SDL_free(displays);
		}
		SDL_SetWindowFullscreen(win, true);
	} else {
		SDL_SetWindowFullscreen(win, false);
	}

	*d3d11Globals.pWindow = win;
	printf("DEBUG: startSDL3 window pointer set to: %p\n", win);
	d3d11Globals.window = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(win), "SDL.window.win32.hwnd", NULL);
	printf("DEBUG: startSDL3 native HWND retrieved: %p\n", d3d11Globals.window);
	return 1;
}
#endif

#ifdef LIBRW_SDL2
static int
startSDL2(void)
{
	if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0){
		return 0;
	}
	SDL_Window *win = SDL_CreateWindow(d3d11Globals.winTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, d3d11Globals.winWidth, d3d11Globals.winHeight, SDL_WINDOW_RESIZABLE);
	if(win == nil){
		return 0;
	}
	*d3d11Globals.pWindow = win;
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	if(SDL_GetWindowWMInfo(win, &wmInfo)){
		d3d11Globals.window = wmInfo.info.win.window;
	}else{
		SDL_DestroyWindow(win);
		*d3d11Globals.pWindow = nil;
		return 0;
	}
	return 1;
}
#endif

#ifdef LIBRW_GLFW
static int
startGLFW(void)
{
	if(!glfwInit()){
		return 0;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *win = glfwCreateWindow(d3d11Globals.winWidth, d3d11Globals.winHeight, d3d11Globals.winTitle, NULL, NULL);
	if(win == nil){
		return 0;
	}
	*d3d11Globals.pWindow = win;
	d3d11Globals.window = glfwGetWin32Window(win);
	return 1;
}
#endif

static bool32
initD3D11(void)
{
	printf("DEBUG: initD3D11 start\n");
#if defined(LIBRW_SDL3)
	if(!startSDL3()) {
		printf("DEBUG: initD3D11 startSDL3 failed\n");
		return 0;
	}
#elif defined(LIBRW_SDL2)
	if(!startSDL2()) {
		printf("DEBUG: initD3D11 startSDL2 failed\n");
		return 0;
	}
#elif defined(LIBRW_GLFW)
	if(!startGLFW()) {
		printf("DEBUG: initD3D11 startGLFW failed\n");
		return 0;
	}
#endif
	printf("DEBUG: initD3D11 windowing setup done\n");
	uint32 width, height;
	getClientSize(&width, &height);
	d3d11Globals.mode.width = width;
	d3d11Globals.mode.height = height;
	d3d11Globals.mode.depth = 32;
	d3d11Globals.mode.flags = 0;
	d3d11Globals.currentMode = 0;

	DXGI_SWAP_CHAIN_DESC scd;
	memset(&scd, 0, sizeof(scd));
	scd.BufferDesc.Width = width;
	scd.BufferDesc.Height = height;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.RefreshRate.Numerator = 0;
	scd.BufferDesc.RefreshRate.Denominator = 1;
	scd.SampleDesc.Count = 1;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.OutputWindow = d3d11Globals.window;
	scd.Windowed = TRUE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0
	};
	D3D_FEATURE_LEVEL featureLevel;
	UINT flags = 0;
#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nil, D3D_DRIVER_TYPE_HARDWARE, nil,
		flags, levels, nelem(levels), D3D11_SDK_VERSION, &scd,
		&d3d11Globals.swapChain, &d3d11Globals.device, &featureLevel, &d3d11Globals.context);
	if(FAILED(hr)){
		flags &= ~D3D11_CREATE_DEVICE_DEBUG;
		hr = D3D11CreateDeviceAndSwapChain(nil, D3D_DRIVER_TYPE_HARDWARE, nil,
			flags, &levels[1], nelem(levels)-1, D3D11_SDK_VERSION, &scd,
			&d3d11Globals.swapChain, &d3d11Globals.device, &featureLevel, &d3d11Globals.context);
	}
	if(FAILED(hr))
		return 0;

	if(!createBackBuffer(width, height) || !createShaders() || !createStates())
		return 0;

	resetRenderState();
	return 1;
}

static void
termD3D11(void)
{
	if(d3d11Globals.context)
		d3d11Globals.context->ClearState();
	releaseSwapChainResources();
	releaseIUnknown(d3d11Globals.sampler);
	releaseIUnknown(d3d11Globals.whiteSRV);
	releaseIUnknown(d3d11Globals.whiteTexture);
	releaseIUnknown(d3d11Globals.blendOpaque);
	releaseIUnknown(d3d11Globals.blendAlpha);
	releaseIUnknown(d3d11Globals.blendState);
	releaseIUnknown(d3d11Globals.depthStencilState);
	for(int ztest = 0; ztest < 2; ztest++)
		for(int zwrite = 0; zwrite < 2; zwrite++)
			releaseIUnknown(d3d11Globals.depthStates[ztest][zwrite]);
	releaseIUnknown(d3d11Globals.rastNone);
	releaseIUnknown(d3d11Globals.rastBack);
	releaseIUnknown(d3d11Globals.rastFront);
	releaseIUnknown(d3d11Globals.constantBuffer);
	releaseIUnknown(d3d11Globals.dynamicVertexBuffer);
	releaseIUnknown(d3d11Globals.dynamicIndexBuffer);
	releaseIUnknown(d3d11Globals.layout2D);
	releaseIUnknown(d3d11Globals.ps2D);
	releaseIUnknown(d3d11Globals.vs2D);
	releaseIUnknown(d3d11Globals.layout3D);
	releaseIUnknown(d3d11Globals.ps3D);
	releaseIUnknown(d3d11Globals.vs3D);
	releaseIUnknown(d3d11Globals.swapChain);
	releaseIUnknown(d3d11Globals.context);
	releaseIUnknown(d3d11Globals.device);
	rwFree(d3d11Globals.tempVertices);
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
	if (d3d11Globals.modes) {
		rwFree(d3d11Globals.modes);
		d3d11Globals.modes = nil;
		d3d11Globals.numModes = 0;
	}
#endif
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
	if(!bChangingVideoMode) {
		if(d3d11Globals.pWindow && *d3d11Globals.pWindow){
			SDL_DestroyWindow(*d3d11Globals.pWindow);
			*d3d11Globals.pWindow = nil;
		}
	}
#elif defined(LIBRW_GLFW)
	if(!bChangingVideoMode) {
		if(d3d11Globals.pWindow && *d3d11Globals.pWindow){
			glfwDestroyWindow(*d3d11Globals.pWindow);
			*d3d11Globals.pWindow = nil;
		}
	}
#endif
	memset(&d3d11Globals, 0, sizeof(d3d11Globals));
}

static void
setViewport(Camera *cam)
{
	Raster *fb = cam && cam->frameBuffer ? cam->frameBuffer : nil;
	float x = 0.0f;
	float y = 0.0f;
	float w = (float)d3d11Globals.presentWidth;
	float h = (float)d3d11Globals.presentHeight;
	if(fb){
		x = (float)fb->offsetX;
		y = (float)fb->offsetY;
		w = (float)fb->width;
		h = (float)fb->height;
	}
	D3D11_VIEWPORT vp;
	memset(&vp, 0, sizeof(vp));
	vp.TopLeftX = x;
	vp.TopLeftY = y;
	vp.Width = w;
	vp.Height = h;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	d3d11Globals.context->RSSetViewports(1, &vp);
}

static Raster*
topLevelRaster(Raster *raster)
{
	if(raster && raster->parent)
		return raster->parent;
	return raster;
}

static ID3D11RenderTargetView*
getCameraRTV(Camera *cam)
{
	Raster *fb = cam && cam->frameBuffer ? topLevelRaster(cam->frameBuffer) : nil;
	if(fb && fb->type == Raster::CAMERATEXTURE){
		ID3D11RenderTargetView *rtv = getRasterRTV(fb);
		if(rtv)
			return rtv;
	}
	return d3d11Globals.backBufferRTV;
}

static ID3D11DepthStencilView*
getCameraDSV(Camera *cam)
{
	if(cam == nil || cam->zBuffer == nil)
		return nil;

	Raster *fb = cam->frameBuffer ? topLevelRaster(cam->frameBuffer) : nil;
	Raster *zbuf = topLevelRaster(cam->zBuffer);
	if(fb && fb->type == Raster::CAMERATEXTURE)
		return getRasterDSV(zbuf);

	return d3d11Globals.depthDSV;
}

static void
setRenderTarget(Camera *cam)
{
	ID3D11ShaderResourceView *nullSRV = nil;
	d3d11Globals.context->PSSetShaderResources(0, 1, &nullSRV);

	ID3D11RenderTargetView *rtv = getCameraRTV(cam);
	ID3D11DepthStencilView *dsv = getCameraDSV(cam);
	d3d11Globals.context->OMSetRenderTargets(rtv ? 1 : 0, rtv ? &rtv : nil, dsv);
}

static void
beginUpdate(Camera *cam)
{
	engine->currentCamera = cam;
	if(d3d11Globals.swapChain)
		resizeIfNeeded();

	float view[16], proj[16];
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  = -inv.at.x;
	view[9]  =  inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	memcpy(&cam->devView, view, sizeof(RawMatrix));

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

	d3d11Globals.fogStart = cam->fogPlane;
	d3d11Globals.fogEnd = cam->farPlane;
	float32 denom = d3d11Globals.fogStart - d3d11Globals.fogEnd;
	d3d11Globals.fogRange = fabsf(denom) > 0.0001f ? 1.0f/denom : 0.0f;

	setRenderTarget(cam);
	setViewport(cam);
	d3d11Globals.inFrame = 1;
}

static void
endUpdate(Camera*)
{
	d3d11Globals.inFrame = 0;
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	if(d3d11Globals.swapChain)
		resizeIfNeeded();
	setRenderTarget(cam);
	setViewport(cam);
	if(mode & Camera::CLEARIMAGE){
		float c[4] = {
			col->red / 255.0f,
			col->green / 255.0f,
			col->blue / 255.0f,
			col->alpha / 255.0f
		};
		ID3D11RenderTargetView *rtv = getCameraRTV(cam);
		Raster *fb = cam && cam->frameBuffer ? cam->frameBuffer : nil;
		if(rtv && fb && fb->parent && fb != fb->parent){
			ID3D11DeviceContext1 *context1 = nil;
			if(SUCCEEDED(d3d11Globals.context->QueryInterface(__uuidof(ID3D11DeviceContext1), (void**)&context1))){
				D3D11_RECT rect;
				rect.left = fb->offsetX;
				rect.top = fb->offsetY;
				rect.right = fb->offsetX + fb->width;
				rect.bottom = fb->offsetY + fb->height;
				context1->ClearView(rtv, c, &rect, 1);
				context1->Release();
			}else
				d3d11Globals.context->ClearRenderTargetView(rtv, c);
		}else if(rtv)
			d3d11Globals.context->ClearRenderTargetView(rtv, c);
	}
	UINT clearFlags = 0;
	if(mode & Camera::CLEARZ)
		clearFlags |= D3D11_CLEAR_DEPTH;
	if(mode & Camera::CLEARSTENCIL)
		clearFlags |= D3D11_CLEAR_STENCIL;
	if(clearFlags){
		ID3D11DepthStencilView *dsv = getCameraDSV(cam);
		if(dsv)
			d3d11Globals.context->ClearDepthStencilView(dsv, clearFlags, 1.0f, 0);
	}
}

static void
showRaster(Raster*, uint32 flags)
{
	if(d3d11Globals.swapChain)
		d3d11Globals.swapChain->Present((flags & Raster::FLIPWAITVSYNCH) ? 1 : 0, 0);
}

static bool32
rasterRenderFast(Raster*, int32, int32)
{
	return 0;
}

static int
openD3D11(EngineOpenParams *params)
{
	printf("DEBUG: openD3D11 start, params: %p\n", params);
	if(params == nil || params->window == nil) {
		printf("DEBUG: openD3D11 failure, params or params->window is nil\n");
		return 0;
	}
	memset(&d3d11Globals, 0, sizeof(d3d11Globals));
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
	printf("DEBUG: openD3D11 using skeleton mode, window address: %p\n", params->window);
	d3d11Globals.pWindow = params->window;
	d3d11Globals.winWidth = params->width;
	d3d11Globals.winHeight = params->height;
	d3d11Globals.winTitle = params->windowtitle;
	d3d11Globals.mode.width = params->width;
	d3d11Globals.mode.height = params->height;
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
	d3d11Globals.mode.flags = params->fullscreen ? VIDEOMODEEXCLUSIVE : 0;
#else
	d3d11Globals.mode.flags = 0;
#endif
#else
	printf("DEBUG: openD3D11 using native Win32 mode, HWND: %p\n", params->window);
	d3d11Globals.window = params->window;
	getClientSize((uint32*)&d3d11Globals.mode.width, (uint32*)&d3d11Globals.mode.height);
#endif
	d3d11Globals.mode.depth = 32;
	d3d11Globals.currentMode = 0;

#if defined(LIBRW_SDL3)
	// Initialize video subsystem and make mode list early, so they are available for psSelectDevice()
	if (SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		makeVideoModeList();
	} else {
		printf("DEBUG: openD3D11 SDL_InitSubSystem failed: %s\n", SDL_GetError());
	}
#endif

	printf("DEBUG: openD3D11 end\n");
	return 1;
}

static int
closeD3D11(void)
{
#if defined(LIBRW_SDL3)
	// Destroy only the window, not the whole video subsystem.
	// The subsystem stays alive so that re-init (video mode change) can
	// reuse it without SDL_GetPrimaryDisplay / SDL_GetDesktopDisplayMode failing.
	if(!bChangingVideoMode) {
		if(d3d11Globals.pWindow && *d3d11Globals.pWindow) {
			SDL_DestroyWindow(*d3d11Globals.pWindow);
			*d3d11Globals.pWindow = nil;
		}
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
#elif defined(LIBRW_SDL2)
	if(!bChangingVideoMode) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
#elif defined(LIBRW_GLFW)
	if(!bChangingVideoMode) {
		glfwTerminate();
	}
#endif
	return 1;
}

static int
deviceSystem(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;
	SubSystemInfo *subsys;

	switch(req){
	case DEVICEOPEN:
		return openD3D11((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeD3D11();
	case DEVICEINIT:
		return initD3D11();
	case DEVICETERM:
		termD3D11();
		return 1;
	case DEVICEFINALIZE:
		return 1;

	case DEVICEGETNUMSUBSYSTEMS:
		return 1;
	case DEVICEGETCURRENTSUBSYSTEM:
		return 0;
	case DEVICESETSUBSYSTEM:
		return n == 0;
	case DEVICEGETSUBSSYSTEMINFO:
		subsys = (SubSystemInfo*)arg;
		strncpy(subsys->name, "Direct3D 11", sizeof(SubSystemInfo::name));
		subsys->name[sizeof(SubSystemInfo::name)-1] = '\0';
		return 1;

	case DEVICEGETNUMVIDEOMODES:
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
		if (d3d11Globals.modes && d3d11Globals.numModes > 0) {
			return d3d11Globals.numModes;
		}
#endif
		return 1;
	case DEVICEGETCURRENTVIDEOMODE:
		return d3d11Globals.currentMode;
	case DEVICESETVIDEOMODE:
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
		if (d3d11Globals.modes && n >= 0 && n < d3d11Globals.numModes) {
			d3d11Globals.currentMode = n;
			d3d11Globals.mode = d3d11Globals.modes[n];
			return 1;
		}
#endif
		return n == 0;
	case DEVICEGETVIDEOMODEINFO:
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2) || defined(LIBRW_GLFW)
		if (d3d11Globals.modes && n >= 0 && n < d3d11Globals.numModes) {
			rwmode = (VideoMode*)arg;
			*rwmode = d3d11Globals.modes[n];
			return 1;
		}
#endif
		if(n != 0)
			return 0;
		rwmode = (VideoMode*)arg;
		*rwmode = d3d11Globals.mode;
		return 1;

	case DEVICEGETMAXMULTISAMPLINGLEVELS:
	case DEVICEGETMULTISAMPLINGLEVELS:
		return 1;
	case DEVICESETMULTISAMPLINGLEVELS:
		return n == 1;
	default:
		assert(0 && "not implemented");
		return 0;
	}
}

Device renderdevice = {
	0.0f, 1.0f,
	d3d11::beginUpdate,
	d3d11::endUpdate,
	d3d11::clearCamera,
	d3d11::showRaster,
	d3d11::rasterRenderFast,
	d3d11::setRenderState,
	d3d11::getRenderState,
	d3d11::im2DRenderLine,
	d3d11::im2DRenderTriangle,
	d3d11::im2DRenderPrimitive,
	d3d11::im2DRenderIndexedPrimitive,
	d3d11::im3DTransform,
	d3d11::im3DRenderPrimitive,
	d3d11::im3DRenderIndexedPrimitive,
	d3d11::im3DEnd,
	d3d11::deviceSystem
};

}
}

#endif
