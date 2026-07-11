#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"

#ifdef RW_D3D11

#include <d3dcompiler.h>

#include "rwd3d.h"
#include "rwd3d11.h"
#include "rwd3dimpl.h"
#include "im3d_VS_d11.h"
#include "im3d_PS_d11.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d11 {
using namespace d3d;

template<class T> static void
safeRelease(T *&p)
{
	if(p){
		p->Release();
		p = nil;
	}
}

struct DynamicVB
{
	uint32 length;
	uint32 stride;
	ID3D11Buffer **buf;
	DynamicVB *next;
};

struct DynamicIB
{
	uint32 length;
	ID3D11Buffer **buf;
	DynamicIB *next;
};

struct RwRasterStateCache
{
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

struct RwStateCache
{
	bool32 vertexAlpha;
	bool32 textureAlpha;
	uint32 srcblend;
	uint32 destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 fogenable;
	RGBA fogcolor;
	RwRasterStateCache texstage[1];
};

struct Im2DConstants
{
	float xform[4];
};

struct Im3DConstants
{
	float combined[16];
	float world[16];
	float normal[16];
};

static DynamicVB *dynamicVBs;
static DynamicIB *dynamicIBs;
static RwStateCache rwStateCache;

static ID3D11BlendState *blendState;
static ID3D11DepthStencilState *depthStencilState;
static ID3D11RasterizerState *rasterizerState;
static ID3D11SamplerState *samplerState;

static ID3D11VertexShader *im2dVS;
static ID3D11PixelShader *im2dPS;
static ID3D11InputLayout *im2dLayout;
static ID3D11Buffer *im2dConstantBuffer;
static ID3D11Buffer *im2dVertexBuffer;
static ID3D11Buffer *im2dIndexBuffer;
static uint32 im2dVertexBufferSize;
static uint32 im2dIndexBufferSize;

static ID3D11VertexShader *im3dVS;
static ID3D11PixelShader *im3dPS;
static ID3D11InputLayout *im3dLayout;
static ID3D11Buffer *im3dConstantBuffer;
static ID3D11Buffer *im3dVertexBuffer;
static ID3D11Buffer *im3dIndexBuffer;
static const uint32 im3dMaxVertices = 10000;
static const uint32 im3dMaxIndices = 10000;
static int32 num3DVertices;
static void *default_amb_VS;
static void *default_amb_dir_VS;
static void *default_all_VS;
static void *default_PS;
static void *default_tex_PS;

static ID3D11Texture2D *whiteTexture;
static ID3D11ShaderResourceView *whiteSRV;

static bool32 blendDirty;
static bool32 depthDirty;
static bool32 rasterizerDirty;
static bool32 samplerDirty;

static int findFormatDepth11(DXGI_FORMAT format);
static void initDefaultMode(void);
static void updateDefaultMode(void);
static bool32 createFactory(void);
static void releaseDefaultViews(void);
static bool32 createDefaultViews(void);
static bool32 resizeSwapChain(uint32 width, uint32 height);
static void ensureSwapChainSize(void);
static bool32 openD3D11(EngineOpenParams *params);
static int closeD3D11(void);
static bool32 startD3D11(void);
static bool32 initD3D11(void);
static int termD3D11(void);
static int finalizeD3D11(void);
static void resetRenderState(void);
static void setRenderSurfaces(Camera *cam);
static void setViewport(Raster *fb);
static void beginUpdate(Camera *cam);
static void endUpdate(Camera *cam);
static void clearCamera(Camera *cam, RGBA *col, uint32 mode);
static void showRaster(Raster *raster, uint32 flags);
static bool32 rasterRenderFast(Raster *raster, int32 x, int32 y);
static void setRwRenderState(int32 state, void *value);
static void *getRwRenderState(int32 state);
static bool32 openIm2D(void);
static void closeIm2D(void);
static bool32 openIm3D(void);
static void closeIm3D(void);
static void uploadMatrices(void);
static void uploadMatrices(Matrix *world);
static void im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags);
static void im3DRenderPrimitive(PrimitiveType primType);
static void im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2);
static void im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3);
static void im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices);
static void im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices);

D3d11Globals d3d11Globals;

void
addDynamicVB(uint32 length, uint32 stride, ID3D11Buffer **buf)
{
	DynamicVB *dvb = rwNewT(DynamicVB, 1, ID_DRIVER | MEMDUR_EVENT);
	dvb->length = length;
	dvb->stride = stride;
	dvb->buf = buf;
	dvb->next = dynamicVBs;
	dynamicVBs = dvb;
}

void
removeDynamicVB(ID3D11Buffer **buf)
{
	DynamicVB **p, *dvb;
	for(p = &dynamicVBs; *p; p = &(*p)->next)
		if((*p)->buf == buf)
			goto found;
	return;
found:
	dvb = *p;
	*p = dvb->next;
	rwFree(dvb);
}

void
addDynamicIB(uint32 length, ID3D11Buffer **buf)
{
	DynamicIB *dib = rwNewT(DynamicIB, 1, ID_DRIVER | MEMDUR_EVENT);
	dib->length = length;
	dib->buf = buf;
	dib->next = dynamicIBs;
	dynamicIBs = dib;
}

void
removeDynamicIB(ID3D11Buffer **buf)
{
	DynamicIB **p, *dib;
	for(p = &dynamicIBs; *p; p = &(*p)->next)
		if((*p)->buf == buf)
			goto found;
	return;
found:
	dib = *p;
	*p = dib->next;
	rwFree(dib);
}

static int
getClientWidth(void)
{
	RECT rect = { 0, 0, 0, 0 };
	if(d3d11Globals.window)
		GetClientRect(d3d11Globals.window, &rect);
	return rect.right - rect.left;
}

static int
getClientHeight(void)
{
	RECT rect = { 0, 0, 0, 0 };
	if(d3d11Globals.window)
		GetClientRect(d3d11Globals.window, &rect);
	return rect.bottom - rect.top;
}

static int
findFormatDepth11(DXGI_FORMAT format)
{
	switch(format){
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return 32;
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_D16_UNORM:
		return 16;
	default:
		return 32;
	}
}

static DXGI_FORMAT
getColorFormat(void)
{
	return DXGI_FORMAT_R8G8B8A8_UNORM;
}

static DXGI_FORMAT
getDepthFormat(void)
{
	return DXGI_FORMAT_D24_UNORM_S8_UINT;
}

static void
initDefaultMode(void)
{
	memset(&d3d11Globals.startMode, 0, sizeof(d3d11Globals.startMode));
	d3d11Globals.startMode.mode.Width = getClientWidth();
	d3d11Globals.startMode.mode.Height = getClientHeight();
	d3d11Globals.startMode.mode.Format = getColorFormat();
	d3d11Globals.startMode.mode.RefreshRate.Numerator = 60;
	d3d11Globals.startMode.mode.RefreshRate.Denominator = 1;
	d3d11Globals.startMode.flags = 0;
}

static void
updateDefaultMode(void)
{
	if(d3d11Globals.modes == nil)
		return;
	d3d11Globals.modes[0] = d3d11Globals.startMode;
}

static bool32
createFactory(void)
{
	if(d3d11Globals.factory)
		return 1;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&d3d11Globals.factory);
	return SUCCEEDED(hr);
}

static int
closeD3D11(void)
{
	termD3D11();
	rwFree(d3d11Globals.modes);
	d3d11Globals.modes = nil;
	d3d11Globals.numModes = 0;
	d3d11Globals.currentMode = 0;
	safeRelease(d3d11Globals.output);
	for(int i = 0; i < d3d11Globals.numAdapters; i++)
		safeRelease(d3d11Globals.adapters[i]);
	d3d11Globals.numAdapters = 0;
	safeRelease(d3d11Globals.factory);
	d3d11Globals.window = nil;
	return 1;
}

static void
releaseDefaultViews(void)
{
	safeRelease(d3d11Globals.defaultRenderTarget);
	safeRelease(d3d11Globals.defaultDepthStencilView);
}

static bool32
createDefaultViews(void)
{
	ID3D11Texture2D *backBuffer = nil;
	ID3D11Texture2D *depthTex = nil;
	HRESULT hr;

	hr = d3d11Globals.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "IDXGISwapChain::GetBuffer() failed"));
		return 0;
	}
	hr = d3d11Globals.device->CreateRenderTargetView(backBuffer, nil, &d3d11Globals.defaultRenderTarget);
	safeRelease(backBuffer);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "ID3D11Device::CreateRenderTargetView() failed"));
		return 0;
	}

	D3D11_TEXTURE2D_DESC depthDesc;
	memset(&depthDesc, 0, sizeof(depthDesc));
	depthDesc.Width = d3d11Globals.present.BufferDesc.Width;
	depthDesc.Height = d3d11Globals.present.BufferDesc.Height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = getDepthFormat();
	depthDesc.SampleDesc = d3d11Globals.present.SampleDesc;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	hr = d3d11Globals.device->CreateTexture2D(&depthDesc, nil, &depthTex);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "ID3D11Device::CreateTexture2D() failed for depth"));
		return 0;
	}
	hr = d3d11Globals.device->CreateDepthStencilView(depthTex, nil, &d3d11Globals.defaultDepthStencilView);
	safeRelease(depthTex);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "ID3D11Device::CreateDepthStencilView() failed"));
		return 0;
	}

	d3d11Globals.context->OMSetRenderTargets(1, &d3d11Globals.defaultRenderTarget, d3d11Globals.defaultDepthStencilView);
	return 1;
}

static bool32
resizeSwapChain(uint32 width, uint32 height)
{
	if(d3d11Globals.swapChain == nil)
		return 0;
	if(width == 0 || height == 0)
		return 1;

	d3d11Globals.context->OMSetRenderTargets(0, nil, nil);
	releaseDefaultViews();

	HRESULT hr = d3d11Globals.swapChain->ResizeBuffers(1, width, height, d3d11Globals.present.BufferDesc.Format, d3d11Globals.present.Flags);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "IDXGISwapChain::ResizeBuffers() failed"));
		return 0;
	}
	d3d11Globals.present.BufferDesc.Width = width;
	d3d11Globals.present.BufferDesc.Height = height;
	initDefaultMode();
	updateDefaultMode();
	return createDefaultViews();
}

static void
ensureSwapChainSize(void)
{
	if(d3d11Globals.swapChain == nil)
		return;
	uint32 width = getClientWidth();
	uint32 height = getClientHeight();
	if(width == 0 || height == 0)
		return;
	if(width != d3d11Globals.present.BufferDesc.Width ||
	   height != d3d11Globals.present.BufferDesc.Height)
		resizeSwapChain(width, height);
}

static bool32
openD3D11(EngineOpenParams *params)
{
	memset(&d3d11Globals, 0, sizeof(d3d11Globals));
	d3d11Globals.window = params->window;
	d3d11Globals.msLevel = 1;

	if(!createFactory()){
		RWERROR((ERR_GENERAL, "CreateDXGIFactory() failed"));
		return 0;
	}

	for(int i = 0; i < MAX_D3D11_ADAPTERS; i++){
		IDXGIAdapter *adapter = nil;
		if(d3d11Globals.factory->EnumAdapters(i, &adapter) == DXGI_ERROR_NOT_FOUND)
			break;
		if(adapter == nil)
			break;
		d3d11Globals.adapters[d3d11Globals.numAdapters] = adapter;
		adapter->GetDesc(&d3d11Globals.adapterDescs[d3d11Globals.numAdapters]);
		d3d11Globals.numAdapters++;
	}
	if(d3d11Globals.numAdapters == 0){
		RWERROR((ERR_GENERAL, "No DXGI adapters found"));
		return 0;
	}

	d3d11Globals.adapter = 0;
	d3d11Globals.adapters[d3d11Globals.adapter]->EnumOutputs(0, &d3d11Globals.output);

	d3d11Globals.modes = rwNewT(DisplayMode, 1, ID_DRIVER | MEMDUR_EVENT);
	d3d11Globals.numModes = 1;
	d3d11Globals.currentMode = 0;
	initDefaultMode();
	updateDefaultMode();

	memset(&d3d11Globals.present, 0, sizeof(d3d11Globals.present));
	d3d11Globals.present.BufferDesc.Width = d3d11Globals.startMode.mode.Width;
	d3d11Globals.present.BufferDesc.Height = d3d11Globals.startMode.mode.Height;
	d3d11Globals.present.BufferDesc.RefreshRate = d3d11Globals.startMode.mode.RefreshRate;
	d3d11Globals.present.BufferDesc.Format = getColorFormat();
	d3d11Globals.present.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	d3d11Globals.present.BufferCount = 1;
	d3d11Globals.present.OutputWindow = d3d11Globals.window;
	d3d11Globals.present.Windowed = TRUE;
	d3d11Globals.present.SampleDesc.Count = 1;
	d3d11Globals.present.SampleDesc.Quality = 0;
	d3d11Globals.present.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	d3d11Globals.present.Flags = 0;

	return 1;
}

static bool32
startD3D11(void)
{
	HRESULT hr;
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	DXGI_MODE_DESC mode = d3d11Globals.modes[d3d11Globals.currentMode].mode;
	bool32 windowed = !(d3d11Globals.modes[d3d11Globals.currentMode].flags & VIDEOMODEEXCLUSIVE);
	uint32 width = windowed ? getClientWidth() : mode.Width;
	uint32 height = windowed ? getClientHeight() : mode.Height;
	if(width == 0) width = 1;
	if(height == 0) height = 1;

	hr = D3D11CreateDevice(d3d11Globals.adapters[d3d11Globals.adapter],
		D3D_DRIVER_TYPE_UNKNOWN, nil, flags,
		levels, nelem(levels), D3D11_SDK_VERSION,
		&d3d11Globals.device, &d3d11Globals.featureLevel, &d3d11Globals.context);
	if(FAILED(hr)){
		hr = D3D11CreateDevice(nil, D3D_DRIVER_TYPE_HARDWARE, nil, flags,
			levels, nelem(levels), D3D11_SDK_VERSION,
			&d3d11Globals.device, &d3d11Globals.featureLevel, &d3d11Globals.context);
	}
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "D3D11CreateDevice() failed"));
		return 0;
	}

	d3d11Globals.present.BufferDesc.Width = width;
	d3d11Globals.present.BufferDesc.Height = height;
	d3d11Globals.present.BufferDesc.Format = getColorFormat();
	d3d11Globals.present.BufferDesc.RefreshRate = mode.RefreshRate;
	d3d11Globals.present.OutputWindow = d3d11Globals.window;
	d3d11Globals.present.Windowed = windowed;

	uint32 requestedSamples = d3d11Globals.msLevel > 1 ? d3d11Globals.msLevel : 1;
	uint32 quality = 0;
	if(requestedSamples > 1 &&
	   FAILED(d3d11Globals.device->CheckMultisampleQualityLevels(getColorFormat(), requestedSamples, &quality)))
		requestedSamples = 1;
	if(requestedSamples > 1 && quality == 0)
		requestedSamples = 1;
	d3d11Globals.present.SampleDesc.Count = requestedSamples;
	d3d11Globals.present.SampleDesc.Quality = requestedSamples > 1 ? quality-1 : 0;
	d3d11Globals.msLevel = requestedSamples;

	hr = d3d11Globals.factory->CreateSwapChain(d3d11Globals.device, &d3d11Globals.present, &d3d11Globals.swapChain);
	if(FAILED(hr)){
		RWERROR((ERR_GENERAL, "IDXGIFactory::CreateSwapChain() failed"));
		return 0;
	}
	d3d11Globals.factory->MakeWindowAssociation(d3d11Globals.window, DXGI_MWA_NO_ALT_ENTER);
	return 1;
}

static bool32
initD3D11(void)
{
	d3d11Globals.numTextures = 0;
	d3d11Globals.numVertexShaders = 0;
	d3d11Globals.numPixelShaders = 0;
	d3d11Globals.numVertexBuffers = 0;
	d3d11Globals.numIndexBuffers = 0;
	d3d11Globals.numInputLayouts = 0;

	if(!createDefaultViews())
		return 0;
	resetRenderState();
	if(!openIm2D())
		return 0;
	if(!openIm3D()){
		closeIm2D();
		return 0;
	}
	return 1;
}

static int
termD3D11(void)
{
	closeIm3D();
	closeIm2D();
	releaseDefaultViews();
	if(d3d11Globals.context){
		d3d11Globals.context->ClearState();
		d3d11Globals.context->Flush();
	}
	safeRelease(d3d11Globals.swapChain);
	safeRelease(d3d11Globals.context);
	safeRelease(d3d11Globals.device);
	return 1;
}

static int
finalizeD3D11(void)
{
	return 1;
}

static RasterLevels*
allocateLevels(int32 width, int32 height, int32 numlevels)
{
	if(numlevels <= 0)
		numlevels = 1;
	size_t size = sizeof(RasterLevels) + sizeof(RasterLevels::Level)*(numlevels-1);
	RasterLevels *levels = (RasterLevels*)rwNew(size, MEMDUR_EVENT | ID_DRIVER);
	memset(levels, 0, size);
	levels->numlevels = numlevels;
	levels->format = Raster::C8888;
	for(int32 i = 0; i < numlevels; i++){
		int32 w = width >> i;
		int32 h = height >> i;
		if(w < 1) w = 1;
		if(h < 1) h = 1;
		levels->levels[i].width = w;
		levels->levels[i].height = h;
		levels->levels[i].size = w*h*4;
		levels->levels[i].data = rwNewT(uint8, levels->levels[i].size, MEMDUR_EVENT | ID_DRIVER);
		memset(levels->levels[i].data, 0, levels->levels[i].size);
	}
	return levels;
}

static void
freeLevels(RasterLevels *levels)
{
	if(levels == nil)
		return;
	for(int32 i = 0; i < levels->numlevels; i++)
		rwFree(levels->levels[i].data);
	rwFree(levels);
}

static bool32
createTextureResources(Raster *raster, bool32 renderTarget)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	D3D11_TEXTURE2D_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Width = raster->width;
	desc.Height = raster->height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = getColorFormat();
	desc.SampleDesc.Count = renderTarget ? 1 : 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (renderTarget ? D3D11_BIND_RENDER_TARGET : 0);

	ID3D11Texture2D *texture = nil;
	HRESULT hr = d3d11Globals.device->CreateTexture2D(&desc, nil, &texture);
	if(FAILED(hr))
		return 0;

	ID3D11ShaderResourceView *srv = nil;
	hr = d3d11Globals.device->CreateShaderResourceView(texture, nil, &srv);
	if(FAILED(hr)){
		safeRelease(texture);
		return 0;
	}

	natras->texture = texture;
	natras->srv = srv;
	natras->format = desc.Format;
	natras->bpp = 4;
	natras->customFormat = 0;
	natras->autogenMipmap = 0;
	d3d11Globals.numTextures++;

	if(renderTarget){
		ID3D11RenderTargetView *rtv = nil;
		hr = d3d11Globals.device->CreateRenderTargetView(texture, nil, &rtv);
		if(FAILED(hr))
			return 0;
		natras->lockedSurf = rtv;
	}
	return 1;
}

static void
uploadRasterLevel(Raster *raster, int32 level)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	RasterLevels *levels = (RasterLevels*)natras->lockedSurf;
	if(levels == nil || natras->texture == nil)
		return;
	if(level < 0 || level >= levels->numlevels)
		return;
	d3d11Globals.context->UpdateSubresource((ID3D11Texture2D*)natras->texture, level,
		nil, levels->levels[level].data, levels->levels[level].width*4, 0);
}

Raster*
rasterCreate(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	memset(natras, 0, sizeof(*natras));

	if(raster->format == 0){
		switch(raster->type){
		case Raster::ZBUFFER:
			raster->format = Raster::D24;
			break;
		default:
			raster->format = Raster::C8888;
			break;
		}
	}

	if(raster->width == 0 || raster->height == 0){
		raster->flags |= Raster::DONTALLOCATE;
		raster->stride = 0;
		goto done;
	}
	if(raster->flags & Raster::DONTALLOCATE)
		goto done;

	switch(raster->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
		raster->depth = 32;
		raster->stride = raster->width*4;
		natras->hasAlpha = Raster::formatHasAlpha(raster->format);
		natras->bpp = 4;
		natras->lockedSurf = allocateLevels(raster->width, raster->height, 1);
		if(!createTextureResources(raster, 0)){
			freeLevels((RasterLevels*)natras->lockedSurf);
			natras->lockedSurf = nil;
			RWERROR((ERR_NOTEXTURE));
			return nil;
		}
		break;
	case Raster::CAMERATEXTURE:
		raster->depth = 32;
		raster->stride = raster->width*4;
		natras->hasAlpha = 1;
		if(!createTextureResources(raster, 1)){
			RWERROR((ERR_NOTEXTURE));
			return nil;
		}
		break;
	case Raster::CAMERA:
		raster->depth = findFormatDepth11(getColorFormat());
		raster->stride = raster->width*4;
		natras->bpp = 4;
		natras->hasAlpha = 1;
		natras->format = getColorFormat();
		break;
	case Raster::ZBUFFER:
		raster->depth = findFormatDepth11(getDepthFormat());
		raster->stride = 0;
		natras->format = getDepthFormat();
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return nil;
	}

done:
	raster->originalWidth = raster->width;
	raster->originalHeight = raster->height;
	raster->originalStride = raster->stride;
	raster->originalPixels = raster->pixels;
	return raster;
}

void
destroyRaster(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	if(rwStateCache.texstage[0].raster == raster)
		rwStateCache.texstage[0].raster = nil;

	switch(raster->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
		freeLevels((RasterLevels*)natras->lockedSurf);
		natras->lockedSurf = nil;
		safeRelease((ID3D11ShaderResourceView*&)natras->srv);
		safeRelease((ID3D11Texture2D*&)natras->texture);
		break;
	case Raster::CAMERATEXTURE:
		safeRelease((ID3D11RenderTargetView*&)natras->lockedSurf);
		safeRelease((ID3D11ShaderResourceView*&)natras->srv);
		safeRelease((ID3D11Texture2D*&)natras->texture);
		break;
	case Raster::CAMERA:
	case Raster::ZBUFFER:
	default:
		break;
	}
	natras->texture = nil;
	natras->srv = nil;
	natras->lockedSurf = nil;
}

uint8*
rasterLock(Raster *raster, int32 level, int32 lockMode)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	if(raster->privateFlags & (Raster::PRIVATELOCK_READ|Raster::PRIVATELOCK_WRITE))
		return nil;
	if(raster->type != Raster::NORMAL && raster->type != Raster::TEXTURE)
		return nil;

	RasterLevels *levels = (RasterLevels*)natras->lockedSurf;
	if(levels == nil || level >= levels->numlevels)
		return nil;

	raster->pixels = levels->levels[level].data;
	raster->width = levels->levels[level].width;
	raster->height = levels->levels[level].height;
	raster->stride = levels->levels[level].width*4;
	if(lockMode & Raster::LOCKREAD) raster->privateFlags |= Raster::PRIVATELOCK_READ;
	if(lockMode & Raster::LOCKWRITE) raster->privateFlags |= Raster::PRIVATELOCK_WRITE;
	return raster->pixels;
}

void
rasterUnlock(Raster *raster, int32 level)
{
	if((raster->privateFlags & Raster::PRIVATELOCK_WRITE) &&
	   (raster->type == Raster::NORMAL || raster->type == Raster::TEXTURE))
		uploadRasterLevel(raster, level);

	raster->width = raster->originalWidth;
	raster->height = raster->originalHeight;
	raster->stride = raster->originalStride;
	raster->pixels = raster->originalPixels;
	raster->privateFlags &= ~(Raster::PRIVATELOCK_READ|Raster::PRIVATELOCK_WRITE);
}

int32
rasterNumLevels(Raster *raster)
{
	D3dRaster *natras = GETD3DRASTEREXT(raster);
	if(raster->type == Raster::NORMAL || raster->type == Raster::TEXTURE){
		RasterLevels *levels = (RasterLevels*)natras->lockedSurf;
		return levels ? levels->numlevels : 1;
	}
	return 1;
}

bool32
imageFindRasterFormat(Image *img, int32 type,
	int32 *pWidth, int32 *pHeight, int32 *pDepth, int32 *pFormat)
{
	int32 width = img->width;
	int32 height = img->height;
	int32 depth = img->depth;
	int32 format;

	if(depth <= 8)
		depth = 32;

	switch(depth){
	case 32:
		format = img->hasAlpha() ? Raster::C8888 : Raster::C888;
		depth = img->hasAlpha() ? 32 : 24;
		break;
	case 24:
		format = Raster::C888;
		break;
	case 16:
		format = Raster::C1555;
		break;
	default:
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	*pWidth = width;
	*pHeight = height;
	*pDepth = depth;
	*pFormat = format | type;
	return 1;
}

bool32
rasterFromImage(Raster *raster, Image *image)
{
	if((raster->type & 0xF) != Raster::TEXTURE)
		return 0;

	Image *truecolimg = nil;
	if(image->depth <= 8){
		truecolimg = Image::create(image->width, image->height, image->depth);
		truecolimg->pixels = image->pixels;
		truecolimg->stride = image->stride;
		truecolimg->palette = image->palette;
		truecolimg->unpalettize();
		image = truecolimg;
	}

	void (*conv)(uint8 *out, uint8 *in) = nil;
	switch(image->depth){
	case 32: conv = conv_RGBA8888_from_RGBA8888; break;
	case 24: conv = conv_RGBA8888_from_RGB888; break;
	case 16: conv = conv_RGBA8888_from_ARGB1555; break;
	default:
		if(truecolimg)
			truecolimg->destroy();
		RWERROR((ERR_INVRASTER));
		return 0;
	}

	D3dRaster *natras = GETD3DRASTEREXT(raster);
	natras->hasAlpha = image->hasAlpha();

	bool unlock = false;
	if(raster->pixels == nil){
		raster->lock(0, Raster::LOCKWRITE | Raster::LOCKNOFETCH);
		unlock = true;
	}
	uint8 *pixels = raster->pixels;
	uint8 *imgpixels = image->pixels;
	for(int y = 0; y < image->height; y++){
		uint8 *imgrow = imgpixels;
		uint8 *rasrow = pixels;
		for(int x = 0; x < image->width; x++){
			conv(rasrow, imgrow);
			imgrow += image->bpp;
			rasrow += 4;
		}
		imgpixels += image->stride;
		pixels += raster->stride;
	}
	if(unlock)
		raster->unlock(0);
	if(truecolimg)
		truecolimg->destroy();
	return 1;
}

Image*
rasterToImage(Raster *raster)
{
	if(raster->type != Raster::NORMAL && raster->type != Raster::TEXTURE)
		return nil;

	bool unlock = false;
	if(raster->pixels == nil){
		raster->lock(0, Raster::LOCKREAD);
		unlock = true;
	}

	Image *image = Image::create(raster->width, raster->height, 32);
	image->allocate();
	for(int y = 0; y < raster->height; y++)
		memcpy(image->pixels + y*image->stride, raster->pixels + y*raster->stride, raster->width*4);

	if(unlock)
		raster->unlock(0);
	return image;
}

static int
deviceSystem(DeviceReq req, void *arg, int32 n)
{
	VideoMode *rwmode;
	switch(req){
	case DEVICEOPEN:
		return openD3D11((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeD3D11();
	case DEVICEINIT:
		return startD3D11() && initD3D11();
	case DEVICETERM:
		return termD3D11();
	case DEVICEFINALIZE:
		return finalizeD3D11();
	case DEVICEGETNUMSUBSYSTEMS:
		return d3d11Globals.numAdapters ? d3d11Globals.numAdapters : 1;
	case DEVICEGETCURRENTSUBSYSTEM:
		return d3d11Globals.adapter;
	case DEVICESETSUBSYSTEM:
		if(n < 0 || n >= d3d11Globals.numAdapters)
			return 0;
		d3d11Globals.adapter = n;
		return 1;
	case DEVICEGETSUBSSYSTEMINFO:
		if(n < 0 || n >= d3d11Globals.numAdapters)
			return 0;
		WideCharToMultiByte(CP_ACP, 0, d3d11Globals.adapterDescs[n].Description, -1,
			((SubSystemInfo*)arg)->name, sizeof(SubSystemInfo::name), nil, nil);
		return 1;
	case DEVICEGETNUMVIDEOMODES:
		return d3d11Globals.numModes ? d3d11Globals.numModes : 1;
	case DEVICEGETCURRENTVIDEOMODE:
		return d3d11Globals.currentMode;
	case DEVICESETVIDEOMODE:
		if(n < 0 || n >= d3d11Globals.numModes)
			return 0;
		d3d11Globals.currentMode = n;
		return 1;
	case DEVICEGETVIDEOMODEINFO:
		if(d3d11Globals.modes == nil || n < 0 || n >= d3d11Globals.numModes)
			return 0;
		rwmode = (VideoMode*)arg;
		rwmode->width = d3d11Globals.modes[n].mode.Width;
		rwmode->height = d3d11Globals.modes[n].mode.Height;
		rwmode->depth = findFormatDepth11(d3d11Globals.modes[n].mode.Format);
		rwmode->flags = d3d11Globals.modes[n].flags;
		return 1;
	case DEVICEGETMAXMULTISAMPLINGLEVELS:
		return 8;
	case DEVICEGETMULTISAMPLINGLEVELS:
		return d3d11Globals.msLevel ? d3d11Globals.msLevel : 1;
	case DEVICESETMULTISAMPLINGLEVELS:
		d3d11Globals.msLevel = (uint32)n;
		return 1;
	default:
		break;
	}
	return 1;
}

static D3D11_BLEND blendMap[] = {
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ZERO,
	D3D11_BLEND_ONE,
	D3D11_BLEND_SRC_COLOR,
	D3D11_BLEND_INV_SRC_COLOR,
	D3D11_BLEND_SRC_ALPHA,
	D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA,
	D3D11_BLEND_INV_DEST_ALPHA,
	D3D11_BLEND_DEST_COLOR,
	D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC_ALPHA_SAT
};

static D3D11_TEXTURE_ADDRESS_MODE addressMap[] = {
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_WRAP,
	D3D11_TEXTURE_ADDRESS_MIRROR,
	D3D11_TEXTURE_ADDRESS_CLAMP,
	D3D11_TEXTURE_ADDRESS_BORDER
};

static D3D11_FILTER
filterToD3D11(uint32 filter)
{
	switch(filter){
	case Texture::NEAREST: return D3D11_FILTER_MIN_MAG_MIP_POINT;
	case Texture::LINEAR: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case Texture::MIPNEAREST: return D3D11_FILTER_MIN_MAG_MIP_POINT;
	case Texture::MIPLINEAR: return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
	case Texture::LINEARMIPNEAREST: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	case Texture::LINEARMIPLINEAR: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	default: return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	}
}

static void
setRasterStage(Raster *raster)
{
	bool32 alpha = 0;
	rwStateCache.texstage[0].raster = raster;
	if(raster){
		D3dRaster *natras = GETD3DRASTEREXT(raster);
		alpha = natras->hasAlpha;
	}
	if(rwStateCache.textureAlpha != alpha){
		rwStateCache.textureAlpha = alpha;
		blendDirty = 1;
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		rwStateCache.vertexAlpha = enable;
		blendDirty = 1;
	}
}

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		depthDirty = 1;
	}
}

static void
setDepthWrite(bool32 enable)
{
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		depthDirty = 1;
	}
}

static void
setFilterMode(uint32 filter)
{
	if(rwStateCache.texstage[0].filter != (Texture::FilterMode)filter){
		rwStateCache.texstage[0].filter = (Texture::FilterMode)filter;
		samplerDirty = 1;
	}
}

static void
setAddressU(uint32 addressing)
{
	if(rwStateCache.texstage[0].addressingU != (Texture::Addressing)addressing){
		rwStateCache.texstage[0].addressingU = (Texture::Addressing)addressing;
		samplerDirty = 1;
	}
}

static void
setAddressV(uint32 addressing)
{
	if(rwStateCache.texstage[0].addressingV != (Texture::Addressing)addressing){
		rwStateCache.texstage[0].addressingV = (Texture::Addressing)addressing;
		samplerDirty = 1;
	}
}

static void
resetRenderState(void)
{
	memset(&rwStateCache, 0, sizeof(rwStateCache));
	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	rwStateCache.zwrite = 1;
	rwStateCache.ztest = 1;
	rwStateCache.cullmode = CULLNONE;
	rwStateCache.texstage[0].addressingU = Texture::WRAP;
	rwStateCache.texstage[0].addressingV = Texture::WRAP;
	rwStateCache.texstage[0].filter = Texture::NEAREST;
	rwStateCache.fogcolor = makeRGBA(0, 0, 0, 255);
	blendDirty = 1;
	depthDirty = 1;
	rasterizerDirty = 1;
	samplerDirty = 1;
}

static void
applyBlendState(void)
{
	if(!blendDirty)
		return;
	safeRelease(blendState);
	D3D11_BLEND_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.RenderTarget[0].BlendEnable = rwStateCache.vertexAlpha || rwStateCache.textureAlpha;
	desc.RenderTarget[0].SrcBlend = blendMap[rwStateCache.srcblend];
	desc.RenderTarget[0].DestBlend = blendMap[rwStateCache.destblend];
	desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	if(SUCCEEDED(d3d11Globals.device->CreateBlendState(&desc, &blendState))){
		float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		d3d11Globals.context->OMSetBlendState(blendState, blendFactor, 0xFFFFFFFF);
	}
	blendDirty = 0;
}

static void
applyDepthState(void)
{
	if(!depthDirty)
		return;
	safeRelease(depthStencilState);
	D3D11_DEPTH_STENCIL_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.DepthEnable = rwStateCache.ztest || rwStateCache.zwrite;
	desc.DepthWriteMask = rwStateCache.zwrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = rwStateCache.ztest ? D3D11_COMPARISON_LESS_EQUAL : D3D11_COMPARISON_ALWAYS;
	desc.StencilEnable = FALSE;
	if(SUCCEEDED(d3d11Globals.device->CreateDepthStencilState(&desc, &depthStencilState)))
		d3d11Globals.context->OMSetDepthStencilState(depthStencilState, 0);
	depthDirty = 0;
}

static void
applyRasterizerState(void)
{
	if(!rasterizerDirty)
		return;
	safeRelease(rasterizerState);
	D3D11_RASTERIZER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.FillMode = D3D11_FILL_SOLID;
	desc.FrontCounterClockwise = TRUE;
	switch(rwStateCache.cullmode){
	case CULLBACK: desc.CullMode = D3D11_CULL_BACK; break;
	case CULLFRONT: desc.CullMode = D3D11_CULL_FRONT; break;
	case CULLNONE:
	default: desc.CullMode = D3D11_CULL_NONE; break;
	}
	desc.DepthClipEnable = TRUE;
	desc.MultisampleEnable = d3d11Globals.present.SampleDesc.Count > 1;
	if(SUCCEEDED(d3d11Globals.device->CreateRasterizerState(&desc, &rasterizerState)))
		d3d11Globals.context->RSSetState(rasterizerState);
	rasterizerDirty = 0;
}

static void
applySamplerState(void)
{
	if(!samplerDirty)
		return;
	safeRelease(samplerState);
	D3D11_SAMPLER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.Filter = filterToD3D11(rwStateCache.texstage[0].filter);
	desc.AddressU = addressMap[rwStateCache.texstage[0].addressingU];
	desc.AddressV = addressMap[rwStateCache.texstage[0].addressingV];
	desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	desc.MaxLOD = D3D11_FLOAT32_MAX;
	desc.MaxAnisotropy = 1;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	if(SUCCEEDED(d3d11Globals.device->CreateSamplerState(&desc, &samplerState)))
		d3d11Globals.context->PSSetSamplers(0, 1, &samplerState);
	samplerDirty = 0;
}

static void
applyDrawState(void)
{
	applyBlendState();
	applyDepthState();
	applyRasterizerState();
	applySamplerState();

	ID3D11ShaderResourceView *srv = whiteSRV;
	Raster *raster = rwStateCache.texstage[0].raster;
	if(raster){
		D3dRaster *natras = GETD3DRASTEREXT(raster);
		if(natras->srv)
			srv = (ID3D11ShaderResourceView*)natras->srv;
	}
	d3d11Globals.context->PSSetShaderResources(0, 1, &srv);
}

static void
setRwRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER: setRasterStage((Raster*)pvalue); break;
	case TEXTUREADDRESS:
		setAddressU(value);
		setAddressV(value);
		break;
	case TEXTUREADDRESSU: setAddressU(value); break;
	case TEXTUREADDRESSV: setAddressV(value); break;
	case TEXTUREFILTER: setFilterMode(value); break;
	case VERTEXALPHA: setVertexAlpha(value != 0); break;
	case SRCBLEND:
		rwStateCache.srcblend = value;
		blendDirty = 1;
		break;
	case DESTBLEND:
		rwStateCache.destblend = value;
		blendDirty = 1;
		break;
	case ZTESTENABLE: setDepthTest(value != 0); break;
	case ZWRITEENABLE: setDepthWrite(value != 0); break;
	case CULLMODE:
		rwStateCache.cullmode = value;
		rasterizerDirty = 1;
		break;
	case FOGENABLE:
		rwStateCache.fogenable = value != 0;
		break;
	case FOGCOLOR:
		rwStateCache.fogcolor.red = value;
		rwStateCache.fogcolor.green = value >> 8;
		rwStateCache.fogcolor.blue = value >> 16;
		rwStateCache.fogcolor.alpha = value >> 24;
		break;
	default:
		break;
	}
}

static void*
getRwRenderState(int32 state)
{
	uint32 val = 0;
	switch(state){
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSU: val = rwStateCache.texstage[0].addressingU; break;
	case TEXTUREADDRESSV: val = rwStateCache.texstage[0].addressingV; break;
	case TEXTUREFILTER: val = rwStateCache.texstage[0].filter; break;
	case VERTEXALPHA: val = rwStateCache.vertexAlpha; break;
	case SRCBLEND: val = rwStateCache.srcblend; break;
	case DESTBLEND: val = rwStateCache.destblend; break;
	case ZTESTENABLE: val = rwStateCache.ztest; break;
	case ZWRITEENABLE: val = rwStateCache.zwrite; break;
	case CULLMODE: val = rwStateCache.cullmode; break;
	case FOGENABLE: val = rwStateCache.fogenable; break;
	case FOGCOLOR:
		val = RWRGBAINT(rwStateCache.fogcolor.red, rwStateCache.fogcolor.green,
			rwStateCache.fogcolor.blue, rwStateCache.fogcolor.alpha);
		break;
	default:
		break;
	}
	return (void*)(uintptr)val;
}

static void
setRenderSurfaces(Camera *cam)
{
	ID3D11RenderTargetView *rtv = d3d11Globals.defaultRenderTarget;
	ID3D11DepthStencilView *dsv = cam->zBuffer ? d3d11Globals.defaultDepthStencilView : nil;

	Raster *fbuf = cam->frameBuffer;
	if(fbuf && fbuf->parent)
		fbuf = fbuf->parent;
	if(fbuf && fbuf->type == Raster::CAMERATEXTURE){
		D3dRaster *natras = GETD3DRASTEREXT(fbuf);
		if(natras->lockedSurf)
			rtv = (ID3D11RenderTargetView*)natras->lockedSurf;
	}

	d3d11Globals.context->OMSetRenderTargets(1, &rtv, dsv);
}

static void
setViewport(Raster *fb)
{
	D3D11_VIEWPORT vp;
	memset(&vp, 0, sizeof(vp));
	vp.TopLeftX = (FLOAT)fb->offsetX;
	vp.TopLeftY = (FLOAT)fb->offsetY;
	vp.Width = (FLOAT)fb->width;
	vp.Height = (FLOAT)fb->height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	d3d11Globals.context->RSSetViewports(1, &vp);
}

static void
beginUpdate(Camera *cam)
{
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
	float32 invz = 1.0f/(cam->farPlane - cam->nearPlane);
	memset(proj, 0, sizeof(proj));
	proj[0] = invwx;
	proj[5] = invwy;
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

	ensureSwapChainSize();
	setRenderSurfaces(cam);
	setViewport(cam->frameBuffer);
}

static void
endUpdate(Camera *cam)
{
	(void)cam;
}

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	ensureSwapChainSize();
	setRenderSurfaces(cam);
	setViewport(cam->frameBuffer);

	if(mode & Camera::CLEARIMAGE){
		float c[4];
		c[0] = col->red / 255.0f;
		c[1] = col->green / 255.0f;
		c[2] = col->blue / 255.0f;
		c[3] = col->alpha / 255.0f;
		ID3D11RenderTargetView *rtv = d3d11Globals.defaultRenderTarget;
		Raster *fbuf = cam->frameBuffer;
		if(fbuf && fbuf->parent)
			fbuf = fbuf->parent;
		if(fbuf && fbuf->type == Raster::CAMERATEXTURE){
			D3dRaster *natras = GETD3DRASTEREXT(fbuf);
			if(natras->lockedSurf)
				rtv = (ID3D11RenderTargetView*)natras->lockedSurf;
		}
		d3d11Globals.context->ClearRenderTargetView(rtv, c);
	}
	if((mode & (Camera::CLEARZ | Camera::CLEARSTENCIL)) && d3d11Globals.defaultDepthStencilView){
		UINT flags = 0;
		if(mode & Camera::CLEARZ) flags |= D3D11_CLEAR_DEPTH;
		if(mode & Camera::CLEARSTENCIL) flags |= D3D11_CLEAR_STENCIL;
		d3d11Globals.context->ClearDepthStencilView(d3d11Globals.defaultDepthStencilView, flags, 1.0f, 0);
	}
}

static void
showRaster(Raster *raster, uint32 flags)
{
	(void)raster;
	UINT interval = (flags & Raster::FLIPWAITVSYNCH) ? 1 : 0;
	if(d3d11Globals.swapChain)
		d3d11Globals.swapChain->Present(interval, 0);
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	(void)raster;
	(void)x;
	(void)y;
	return 0;
}

static ID3DBlob*
compileShader(const char *src, const char *entry, const char *target)
{
	ID3DBlob *shader = nil;
	ID3DBlob *errors = nil;
	HRESULT hr = D3DCompile(src, strlen(src), nil, nil, nil, entry, target, 0, 0, &shader, &errors);
	if(errors){
		fprintf(stderr, "%s\n", (const char*)errors->GetBufferPointer());
		errors->Release();
	}
	if(FAILED(hr))
		return nil;
	return shader;
}

static bool32
ensureDynamicVertexBuffer(uint32 bytes)
{
	if(im2dVertexBuffer && im2dVertexBufferSize >= bytes)
		return 1;
	safeRelease(im2dVertexBuffer);
	im2dVertexBufferSize = im2dVertexBufferSize ? im2dVertexBufferSize : 4096*sizeof(Im2DVertex);
	while(im2dVertexBufferSize < bytes)
		im2dVertexBufferSize *= 2;

	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = im2dVertexBufferSize;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if(FAILED(d3d11Globals.device->CreateBuffer(&desc, nil, &im2dVertexBuffer)))
		return 0;
	return 1;
}

static bool32
ensureDynamicIndexBuffer(uint32 bytes)
{
	if(im2dIndexBuffer && im2dIndexBufferSize >= bytes)
		return 1;
	safeRelease(im2dIndexBuffer);
	im2dIndexBufferSize = im2dIndexBufferSize ? im2dIndexBufferSize : 8192*sizeof(uint16);
	while(im2dIndexBufferSize < bytes)
		im2dIndexBufferSize *= 2;

	D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = im2dIndexBufferSize;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if(FAILED(d3d11Globals.device->CreateBuffer(&desc, nil, &im2dIndexBuffer)))
		return 0;
	return 1;
}

static void
updateIm2DConstants(void)
{
	Camera *cam = engine->currentCamera;
	if(cam == nil || cam->frameBuffer == nil)
		return;
	Im2DConstants constants;
	constants.xform[0] = 2.0f / cam->frameBuffer->width;
	constants.xform[1] = -2.0f / cam->frameBuffer->height;
	constants.xform[2] = -1.0f;
	constants.xform[3] = 1.0f;
	d3d11Globals.context->UpdateSubresource(im2dConstantBuffer, 0, nil, &constants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(0, 1, &im2dConstantBuffer);
}

static D3D11_PRIMITIVE_TOPOLOGY
primitiveTypeToTopology(PrimitiveType primType)
{
	switch(primType){
	case PRIMTYPELINELIST: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case PRIMTYPEPOLYLINE: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case PRIMTYPETRILIST: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case PRIMTYPETRISTRIP: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case PRIMTYPEPOINTLIST: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	default: return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}
}

static void
prepareIm2DCommon(void)
{
	applyDrawState();
	updateIm2DConstants();
	d3d11Globals.context->VSSetShader(im2dVS, nil, 0);
	d3d11Globals.context->PSSetShader(im2dPS, nil, 0);
	d3d11Globals.context->IASetInputLayout(im2dLayout);
}

static bool32
openIm2D(void)
{
	static const char *vsSource =
		"cbuffer Im2DConstants : register(b0) { float4 xform; };\n"
		"struct VSIn { float4 pos : POSITION; float4 color : COLOR0; float2 tex : TEXCOORD0; };\n"
		"struct VSOut { float4 pos : SV_POSITION; float4 color : COLOR0; float2 tex : TEXCOORD0; };\n"
		"VSOut main(VSIn input){\n"
		"  VSOut output;\n"
		"  float z = input.pos.w;\n"
		"  float4 p = input.pos;\n"
		"  p.xy = p.xy * xform.xy + xform.zw;\n"
		"  p.xyz *= z;\n"
		"  p.w = z;\n"
		"  output.pos = p;\n"
		"  output.color = float4(input.color.z, input.color.y, input.color.x, input.color.w);\n"
		"  output.tex = input.tex;\n"
		"  return output;\n"
		"}\n";
	static const char *psSource =
		"Texture2D tex0 : register(t0);\n"
		"SamplerState samp0 : register(s0);\n"
		"struct PSIn { float4 pos : SV_POSITION; float4 color : COLOR0; float2 tex : TEXCOORD0; };\n"
		"float4 main(PSIn input) : SV_Target {\n"
		"  return input.color * tex0.Sample(samp0, input.tex);\n"
		"}\n";

	ID3DBlob *vsBlob = compileShader(vsSource, "main", "vs_4_0");
	ID3DBlob *psBlob = compileShader(psSource, "main", "ps_4_0");
	if(vsBlob == nil || psBlob == nil){
		if(vsBlob) vsBlob->Release();
		if(psBlob) psBlob->Release();
		return 0;
	}

	if(FAILED(d3d11Globals.device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nil, &im2dVS))){
		vsBlob->Release();
		psBlob->Release();
		return 0;
	}
	if(FAILED(d3d11Globals.device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nil, &im2dPS))){
		vsBlob->Release();
		psBlob->Release();
		return 0;
	}
	d3d11Globals.numVertexShaders++;
	d3d11Globals.numPixelShaders++;

	D3D11_INPUT_ELEMENT_DESC elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,    0, offsetof(Im2DVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Im2DVertex, u), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	if(FAILED(d3d11Globals.device->CreateInputLayout(elements, nelem(elements),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &im2dLayout))){
		vsBlob->Release();
		psBlob->Release();
		return 0;
	}
	d3d11Globals.numInputLayouts++;
	vsBlob->Release();
	psBlob->Release();

	D3D11_BUFFER_DESC cbd;
	memset(&cbd, 0, sizeof(cbd));
	cbd.ByteWidth = sizeof(Im2DConstants);
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(d3d11Globals.device->CreateBuffer(&cbd, nil, &im2dConstantBuffer)))
		return 0;

	uint8 whitePixel[4] = { 255, 255, 255, 255 };
	D3D11_TEXTURE2D_DESC td;
	memset(&td, 0, sizeof(td));
	td.Width = 1;
	td.Height = 1;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initData = { whitePixel, 4, 0 };
	if(FAILED(d3d11Globals.device->CreateTexture2D(&td, &initData, &whiteTexture)))
		return 0;
	if(FAILED(d3d11Globals.device->CreateShaderResourceView(whiteTexture, nil, &whiteSRV)))
		return 0;
	return 1;
}

static void
closeIm2D(void)
{
	safeRelease(whiteSRV);
	safeRelease(whiteTexture);
	safeRelease(im2dIndexBuffer);
	safeRelease(im2dVertexBuffer);
	safeRelease(im2dConstantBuffer);
	safeRelease(im2dLayout);
	safeRelease(im2dPS);
	safeRelease(im2dVS);
	im2dVertexBufferSize = 0;
	im2dIndexBufferSize = 0;
}

static bool32
openIm3D(void)
{
	D3D11_INPUT_ELEMENT_DESC elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Im3DVertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Im3DVertex, normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(Im3DVertex, color),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, offsetof(Im3DVertex, u),        D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3D11_BUFFER_DESC desc;

	ID3DBlob *vsBlob = compileShader(im3d_VS_d11_source, "main", "vs_4_0");
	ID3DBlob *psBlob = compileShader(im3d_PS_d11_source, "main", "ps_4_0");
	if(vsBlob == nil || psBlob == nil)
		goto fail;

	if(FAILED(d3d11Globals.device->CreateVertexShader(vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(), nil, &im3dVS)))
		goto fail;
	if(FAILED(d3d11Globals.device->CreatePixelShader(psBlob->GetBufferPointer(),
		psBlob->GetBufferSize(), nil, &im3dPS)))
		goto fail;
	default_amb_VS = im3dVS;
	default_amb_dir_VS = im3dVS;
	default_all_VS = im3dVS;
	default_PS = im3dPS;
	default_tex_PS = im3dPS;
	d3d11Globals.numVertexShaders++;
	d3d11Globals.numPixelShaders++;

	if(FAILED(d3d11Globals.device->CreateInputLayout(elements, nelem(elements),
		vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &im3dLayout)))
		goto fail;
	d3d11Globals.numInputLayouts++;

	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = sizeof(Im3DConstants);
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if(FAILED(d3d11Globals.device->CreateBuffer(&desc, nil, &im3dConstantBuffer)))
		goto fail;

	memset(&desc, 0, sizeof(desc));
	desc.ByteWidth = im3dMaxVertices*sizeof(Im3DVertex);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if(FAILED(d3d11Globals.device->CreateBuffer(&desc, nil, &im3dVertexBuffer)))
		goto fail;

	desc.ByteWidth = im3dMaxIndices*sizeof(uint16);
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	if(FAILED(d3d11Globals.device->CreateBuffer(&desc, nil, &im3dIndexBuffer)))
		goto fail;

	vsBlob->Release();
	psBlob->Release();
	return 1;

fail:
	if(vsBlob) vsBlob->Release();
	if(psBlob) psBlob->Release();
	closeIm3D();
	return 0;
}

static void
closeIm3D(void)
{
	default_amb_VS = nil;
	default_amb_dir_VS = nil;
	default_all_VS = nil;
	default_PS = nil;
	default_tex_PS = nil;
	safeRelease(im3dIndexBuffer);
	safeRelease(im3dVertexBuffer);
	safeRelease(im3dConstantBuffer);
	safeRelease(im3dLayout);
	safeRelease(im3dPS);
	safeRelease(im3dVS);
	num3DVertices = 0;
}

static void
uploadMatrices(void)
{
	RawMatrix combined, identity;
	Camera *cam = engine->currentCamera;
	RawMatrix::setIdentity(&identity);
	RawMatrix::mult(&combined, &cam->devView, &cam->devProj);

	Im3DConstants constants;
	memcpy(constants.combined, &combined, sizeof(combined));
	memcpy(constants.world, &identity, sizeof(identity));
	memcpy(constants.normal, &identity, sizeof(identity));
	d3d11Globals.context->UpdateSubresource(im3dConstantBuffer, 0, nil,
		&constants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(0, 1, &im3dConstantBuffer);
}

static void
uploadMatrices(Matrix *world)
{
	RawMatrix combined, worldMatrix, worldView;
	Camera *cam = engine->currentCamera;
	convMatrix(&worldMatrix, world);
	RawMatrix::mult(&worldView, &worldMatrix, &cam->devView);
	RawMatrix::mult(&combined, &worldView, &cam->devProj);

	Im3DConstants constants;
	memcpy(constants.combined, &combined, sizeof(combined));
	memcpy(constants.world, &worldMatrix, sizeof(worldMatrix));
	// TODO: use the inverse transpose of worldMatrix for normals.
	memcpy(constants.normal, &worldMatrix, sizeof(worldMatrix));
	d3d11Globals.context->UpdateSubresource(im3dConstantBuffer, 0, nil,
		&constants, 0, 0);
	d3d11Globals.context->VSSetConstantBuffers(0, 1, &im3dConstantBuffer);
}

static void
im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	Camera *cam = engine->currentCamera;
	if(cam == nil || vertices == nil || numVertices <= 0 ||
	   (uint32)numVertices > im3dMaxVertices)
		return;

	if(world == nil)
		uploadMatrices();
	else
		uploadMatrices(world);

	if((flags & im3d::VERTEXUV) == 0)
		SetRenderStatePtr(TEXTURERASTER, nil);

	void *shader = default_amb_VS;
	if(flags & im3d::LIGHTING){
		// TODO: upload lights and select the matching D3D11 vertex shader.
		shader = default_amb_VS;
	}else{
		// TODO: upload the unlit material constants.
	}

	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(d3d11Globals.context->Map(im3dVertexBuffer, 0,
		D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	memcpy(mapped.pData, vertices, numVertices*sizeof(Im3DVertex));
	d3d11Globals.context->Unmap(im3dVertexBuffer, 0);

	UINT stride = sizeof(Im3DVertex);
	UINT offset = 0;

	d3d11Globals.context->IASetVertexBuffers(0, 1, &im3dVertexBuffer,
		&stride, &offset);
	d3d11Globals.context->IASetInputLayout(im3dLayout);
	d3d11Globals.context->VSSetShader((ID3D11VertexShader*)shader, nil, 0);

	num3DVertices = numVertices;
}

static void
im3DRenderPrimitive(PrimitiveType primType)
{
	if(num3DVertices <= 0)
		return;

	D3D11_PRIMITIVE_TOPOLOGY topology = primitiveTypeToTopology(primType);
	if(topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return;

	void *shader;
	if(engine->device.getRenderState(TEXTURERASTER))
		shader = default_tex_PS;
	else
		shader = default_PS;

	applyDrawState();
	d3d11Globals.context->PSSetShader((ID3D11PixelShader*)shader, nil, 0);
	d3d11Globals.context->IASetPrimitiveTopology(topology);
	d3d11Globals.context->Draw(num3DVertices, 0);
}

static void
im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2)
{
	Im2DVertex tmp[2];
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmp[0] = verts[vert1];
	tmp[1] = verts[vert2];
	d3d11::im2DRenderPrimitive(PRIMTYPELINELIST, tmp, 2);
}

static void
im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3)
{
	Im2DVertex tmp[3];
	Im2DVertex *verts = (Im2DVertex*)vertices;
	tmp[0] = verts[vert1];
	tmp[1] = verts[vert2];
	tmp[2] = verts[vert3];
	d3d11::im2DRenderPrimitive(PRIMTYPETRILIST, tmp, 3);
}

static void
im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices)
{
	if(primType == PRIMTYPETRIFAN){
		if(numVertices < 3)
			return;
		int32 numIndices = (numVertices-2)*3;
		uint16 *indices = rwNewT(uint16, numIndices, MEMDUR_FUNCTION | ID_DRIVER);
		for(int32 i = 0; i < numVertices-2; i++){
			indices[i*3+0] = 0;
			indices[i*3+1] = i+1;
			indices[i*3+2] = i+2;
		}
		d3d11::im2DRenderIndexedPrimitive(PRIMTYPETRILIST, vertices, numVertices, indices, numIndices);
		rwFree(indices);
		return;
	}

	D3D11_PRIMITIVE_TOPOLOGY topology = primitiveTypeToTopology(primType);
	if(topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return;
	uint32 bytes = numVertices*sizeof(Im2DVertex);
	if(!ensureDynamicVertexBuffer(bytes))
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(d3d11Globals.context->Map(im2dVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	memcpy(mapped.pData, vertices, bytes);
	d3d11Globals.context->Unmap(im2dVertexBuffer, 0);

	UINT stride = sizeof(Im2DVertex);
	UINT offset = 0;
	prepareIm2DCommon();
	d3d11Globals.context->IASetPrimitiveTopology(topology);
	d3d11Globals.context->IASetVertexBuffers(0, 1, &im2dVertexBuffer, &stride, &offset);
	d3d11Globals.context->Draw(numVertices, 0);
}

static void
im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices)
{
	if(primType == PRIMTYPETRIFAN){
		if(numIndices < 3)
			return;
		int32 outCount = (numIndices-2)*3;
		uint16 *out = rwNewT(uint16, outCount, MEMDUR_FUNCTION | ID_DRIVER);
		uint16 *in = (uint16*)indices;
		for(int32 i = 0; i < numIndices-2; i++){
			out[i*3+0] = in[0];
			out[i*3+1] = in[i+1];
			out[i*3+2] = in[i+2];
		}
		d3d11::im2DRenderIndexedPrimitive(PRIMTYPETRILIST, vertices, numVertices, out, outCount);
		rwFree(out);
		return;
	}

	D3D11_PRIMITIVE_TOPOLOGY topology = primitiveTypeToTopology(primType);
	if(topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		return;
	uint32 vbytes = numVertices*sizeof(Im2DVertex);
	uint32 ibytes = numIndices*sizeof(uint16);
	if(!ensureDynamicVertexBuffer(vbytes) || !ensureDynamicIndexBuffer(ibytes))
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(d3d11Globals.context->Map(im2dVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	memcpy(mapped.pData, vertices, vbytes);
	d3d11Globals.context->Unmap(im2dVertexBuffer, 0);
	if(FAILED(d3d11Globals.context->Map(im2dIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		return;
	memcpy(mapped.pData, indices, ibytes);
	d3d11Globals.context->Unmap(im2dIndexBuffer, 0);

	UINT stride = sizeof(Im2DVertex);
	UINT offset = 0;
	prepareIm2DCommon();
	d3d11Globals.context->IASetPrimitiveTopology(topology);
	d3d11Globals.context->IASetVertexBuffers(0, 1, &im2dVertexBuffer, &stride, &offset);
	d3d11Globals.context->IASetIndexBuffer(im2dIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	d3d11Globals.context->DrawIndexed(numIndices, 0, 0);
}

Device renderdevice = {
	0.0f, 1.0f,
	d3d11::beginUpdate,
	d3d11::endUpdate,
	d3d11::clearCamera,
	d3d11::showRaster,
	d3d11::rasterRenderFast,
	d3d11::setRwRenderState,
	d3d11::getRwRenderState,
	d3d11::im2DRenderLine,
	d3d11::im2DRenderTriangle,
	d3d11::im2DRenderPrimitive,
	d3d11::im2DRenderIndexedPrimitive,
	d3d11::im3DTransform,
	d3d11::im3DRenderPrimitive,
	null::im3DRenderIndexedPrimitive,
	null::im3DEnd,
	d3d11::deviceSystem
};

}
}

#endif
