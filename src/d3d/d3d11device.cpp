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
#include "rwd3dimpl.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#define PLUGIN_ID 0

namespace rw {
namespace d3d11 {
using namespace d3d;

#ifdef RW_D3D11

D3d11Globals d3d11Globals;

static int
getClientWidth(void)
{
	RECT rect;
	if(d3d11Globals.window && GetClientRect(d3d11Globals.window, &rect))
		return rect.right - rect.left;
	return 640;
}

static int
getClientHeight(void)
{
	RECT rect;
	if(d3d11Globals.window && GetClientRect(d3d11Globals.window, &rect))
		return rect.bottom - rect.top;
	return 480;
}

static int
findFormatDepth11(DXGI_FORMAT format)
{
	switch(format){
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_D32_FLOAT:
		return 32;
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_D16_UNORM:
		return 16;
	default:
		return 0;
	}
}

static void
initDefaultMode(DisplayMode *mode)
{
	memset(mode, 0, sizeof(*mode));
	mode->mode.Width = getClientWidth();
	mode->mode.Height = getClientHeight();
	mode->mode.RefreshRate.Numerator = 60;
	mode->mode.RefreshRate.Denominator = 1;
	mode->mode.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	mode->mode.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	mode->mode.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	mode->flags = 0;
}

static int
openD3D11(EngineOpenParams *params)
{
	memset(&d3d11Globals, 0, sizeof(d3d11Globals));

	d3d11Globals.window = params->window;
	d3d11Globals.numAdapters = 1;
	d3d11Globals.adapter = 0;
	d3d11Globals.msLevel = 1;
	d3d11Globals.numModes = 1;
	d3d11Globals.currentMode = 0;

	d3d11Globals.modes = rwNewT(DisplayMode, d3d11Globals.numModes,
		ID_DRIVER | MEMDUR_EVENT);
	initDefaultMode(&d3d11Globals.modes[0]);
	d3d11Globals.startMode = d3d11Globals.modes[0];

	memset(&d3d11Globals.present, 0, sizeof(d3d11Globals.present));
	d3d11Globals.present.BufferDesc = d3d11Globals.startMode.mode;
	d3d11Globals.present.SampleDesc.Count = d3d11Globals.msLevel;
	d3d11Globals.present.SampleDesc.Quality = 0;
	d3d11Globals.present.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	d3d11Globals.present.BufferCount = 1;
	d3d11Globals.present.OutputWindow = d3d11Globals.window;
	d3d11Globals.present.Windowed = TRUE;
	d3d11Globals.present.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	return 1;
}

static int
closeD3D11(void)
{
	rwFree(d3d11Globals.modes);
	d3d11Globals.modes = nil;
	d3d11Globals.numModes = 0;
	d3d11Globals.currentMode = 0;
	d3d11Globals.numAdapters = 0;
	return 1;
}

static int
startD3D11(void)
{
	return 1;
}

static int
initD3D11(void)
{
	d3d11Globals.numTextures = 0;
	d3d11Globals.numVertexShaders = 0;
	d3d11Globals.numPixelShaders = 0;
	d3d11Globals.numVertexBuffers = 0;
	d3d11Globals.numIndexBuffers = 0;
	d3d11Globals.numInputLayouts = 0;
	return 1;
}

static int
termD3D11(void)
{
	return 1;
}

static int
finalizeD3D11(void)
{
	return 1;
}

void
addDynamicVB(uint32, uint32, ID3D11Buffer**)
{
}

void
removeDynamicVB(ID3D11Buffer**)
{
}

void
addDynamicIB(uint32, ID3D11Buffer**)
{
}

void
removeDynamicIB(ID3D11Buffer**)
{
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
		return d3d11Globals.numAdapters;

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
		strncpy(((SubSystemInfo*)arg)->name, "Direct3D 11 (stub)",
			sizeof(SubSystemInfo::name));
		((SubSystemInfo*)arg)->name[sizeof(SubSystemInfo::name)-1] = '\0';
		return 1;

	case DEVICEGETNUMVIDEOMODES:
		return d3d11Globals.numModes;

	case DEVICEGETCURRENTVIDEOMODE:
		return d3d11Globals.currentMode;

	case DEVICESETVIDEOMODE:
		if(n < 0 || n >= d3d11Globals.numModes)
			return 0;
		d3d11Globals.currentMode = n;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		if(n < 0 || n >= d3d11Globals.numModes)
			return 0;
		rwmode = (VideoMode*)arg;
		rwmode->width = d3d11Globals.modes[n].mode.Width;
		rwmode->height = d3d11Globals.modes[n].mode.Height;
		rwmode->depth = findFormatDepth11(d3d11Globals.modes[n].mode.Format);
		rwmode->flags = d3d11Globals.modes[n].flags;
		return 1;

	case DEVICEGETMAXMULTISAMPLINGLEVELS:
		return 1;
	case DEVICEGETMULTISAMPLINGLEVELS:
		return d3d11Globals.msLevel ? d3d11Globals.msLevel : 1;
	case DEVICESETMULTISAMPLINGLEVELS:
		d3d11Globals.msLevel = n > 0 ? (uint32)n : 1;
		d3d11Globals.present.SampleDesc.Count = d3d11Globals.msLevel;
		return 1;
	}
	return 1;
}

static void
beginUpdate(Camera *cam)
{
	//TODO: beginUpdate d3d11device
}

static void
endUpdate(Camera* cam) 
{
	//TODO: endUpdate d3d11device
}


Device renderdevice = {
	0.0f, 1.0f,
	d3d11::beginUpdate,
	d3d11::endUpdate,
	null::clearCamera,
	null::showRaster,
	null::rasterRenderFast,
	null::setRenderState,
	null::getRenderState,
	null::im2DRenderLine,
	null::im2DRenderTriangle,
	null::im2DRenderPrimitive,
	null::im2DRenderIndexedPrimitive,
	null::im3DTransform,
	null::im3DRenderPrimitive,
	null::im3DRenderIndexedPrimitive,
	null::im3DEnd,
	d3d11::deviceSystem,
};

#endif

}
}
