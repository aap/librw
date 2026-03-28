#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WITH_D3D
#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwd3d.h"
#include "rwd3d11.h"

#include "rwd3dimpl.h"

#define PLUGIN_ID 2

namespace rw {
namespace d3d11 {
using namespace d3d;

// TODO: move to header, but not as #define
#ifndef RW_D3D11
static VertexElement _d3ddec_end = {0xFF,0,0,0,0,0};
#define D3DDECL_END() _d3ddec_end
#endif

#define NUMDECLELT 12

static void*
driverOpen(void *o, int32, int32)
{
#ifdef RW_D3D11
	// createDefaultShaders();
#endif
	engine->driver[PLATFORM_D3D11]->defaultPipeline = makeDefaultPipeline();

	engine->driver[PLATFORM_D3D11]->rasterNativeOffset = nativeRasterOffset;
	engine->driver[PLATFORM_D3D11]->rasterCreate       = rasterCreate;
	engine->driver[PLATFORM_D3D11]->rasterLock         = rasterLock;
	engine->driver[PLATFORM_D3D11]->rasterUnlock       = rasterUnlock;
	engine->driver[PLATFORM_D3D11]->rasterNumLevels    = rasterNumLevels;
	engine->driver[PLATFORM_D3D11]->imageFindRasterFormat = imageFindRasterFormat;
	engine->driver[PLATFORM_D3D11]->rasterFromImage    = rasterFromImage;
	engine->driver[PLATFORM_D3D11]->rasterToImage      = rasterToImage;
	return o;
}

static void*
driverClose(void *o, int32, int32)
{
#ifdef RW_D3D11
	// destroyDefaultShaders();
#endif
	return o;
}

void
registerPlatformPlugins(void)
{
	Driver::registerPlugin(PLATFORM_D3D11, 0, PLATFORM_D3D11,
	                       driverOpen, driverClose);
	// shared between D3D8, 9 and 11
	if(nativeRasterOffset == 0)
		registerNativeRaster();
}

void*
createVertexDeclaration(VertexElement *elements)
{
#ifdef RW_D3D11
	// TODO: Implement D3D11 vertex declaration creation
	return nil;
#else
	int n = 0;
	VertexElement *e = (VertexElement*)elements;
	while(e[n++].stream != 0xFF)
		;
	e = rwNewT(VertexElement, n, MEMDUR_EVENT | ID_DRIVER);
	memcpy(e, elements, n*sizeof(VertexElement));
	return e;
#endif
}

void
destroyVertexDeclaration(void *declaration)
{
#ifdef RW_D3D11
	// TODO: Implement D3D11 vertex declaration destruction
#else
	rwFree(declaration);
#endif
}

uint32
getDeclaration(void *declaration, VertexElement *elements)
{
#ifdef RW_D3D11
	// TODO: Implement D3D11 vertex declaration retrieval
	return 0;
#else
	int n = 0;
	VertexElement *e = (VertexElement*)declaration;
	while(e[n++].stream != 0xFF)
		;
	if(elements)
		memcpy(elements, declaration, n*sizeof(VertexElement));
	return n;
#endif
}

void
freeInstanceData(Geometry *geometry)
{
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_D3D11)
		return;
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	destroyVertexDeclaration(header->vertexDeclaration);
	// TODO: Implement D3D11 buffer destruction
	// destroyIndexBuffer(header->indexBuffer);
	// destroyVertexBuffer(header->vertexStream[0].vertexBuffer);
	// destroyVertexBuffer(header->vertexStream[1].vertexBuffer);
	rwFree(header->inst);
	rwFree(header);
	return;
}


void*
destroyNativeData(void *object, int32, int32)
{
	freeInstanceData((Geometry*)object);
	return object;
}

Stream*
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	// TODO: Implement D3D11 native data reading
	return stream;
}

Stream*
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	// TODO: Implement D3D11 native data writing
	return stream;
}

int32
getSizeNativeData(void *object, int32, int32)
{
	// TODO: Implement D3D11 native data size calculation
	return 0;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         nil, destroyNativeData, nil);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
}

static InstanceDataHeader*
instanceMesh(rw::ObjPipeline *rwpipe, Geometry *geo)
{
	// TODO: Implement D3D11 mesh instancing
	return nil;
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	// TODO: Implement D3D11 instancing
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	// TODO: Implement D3D11 uninstancing
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	// TODO: Implement D3D11 rendering
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

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance)
{
	// TODO: Implement D3D11 default instance callback
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	// TODO: Implement D3D11 default uninstance callback
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = ObjPipeline::create();
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB_Shader;
	return pipe;
}

void
defaultRenderCB_Fix(Atomic *atomic, InstanceDataHeader *header)
{
	// TODO: Implement D3D11 default render callback (fixed function)
}

// Native Texture and Raster

Texture*
readNativeTexture(Stream *stream)
{
	// TODO: Implement D3D11 native texture reading
	return nil;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	// TODO: Implement D3D11 native texture writing
}

uint32
getSizeNativeTexture(Texture *tex)
{
	// TODO: Implement D3D11 native texture size calculation
	return 0;
}

}
}
