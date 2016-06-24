#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwplugins.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#endif
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

// TODO: make some of these things platform-independent

void
initializePlatform(void)
{
#ifdef RW_OPENGL
        driver[PLATFORM_GL3].defaultPipeline = makeDefaultPipeline();
	matFXGlobals.pipelines[PLATFORM_GL3] = makeMatFXPipeline();
	skinGlobals.pipelines[PLATFORM_GL3] = makeSkinPipeline();
#endif

	initializeRender();
}

#ifdef RW_OPENGL

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if(geo->geoflags & Geometry::NATIVE)
		return;
	geo->geoflags |= Geometry::NATIVE;
	InstanceDataHeader *header = new InstanceDataHeader;
	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_GL3;

	header->serialNumber = 0;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ? GL_TRIANGLE_STRIP : GL_TRIANGLES;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = new InstanceData[header->numMeshes];

	header->indexBuffer = new uint16[header->totalNumIndex];
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->mesh;
	uint32 offset = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, nil);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->program = 0;
		inst->offset = offset;
		memcpy((uint8*)header->indexBuffer + inst->offset,
		       mesh->indices, inst->numIndex*2);
		offset += inst->numIndex*2;
		mesh++;
		inst++;
	}

	header->vertexBuffer = nil;
	header->numAttribs = 0;
	header->attribDesc = nil;
	header->ibo = 0;
	header->vbo = 0;

	glGenBuffers(1, &header->ibo);
	glBindBuffer(GL_ARRAY_BUFFER, header->ibo);
	glBufferData(GL_ARRAY_BUFFER, header->totalNumIndex*2,
			header->indexBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	pipe->instanceCB(geo, header);
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	assert(0 && "can't uninstance");
}

static void
render(rw::ObjPipeline *rwpipe, Atomic *atomic)
{       
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		pipe->instance(atomic);
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_GL3);
	if(pipe->renderCB)
		pipe->renderCB(atomic, (InstanceDataHeader*)geo->instData);
}

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform)
{       
	this->impl.instance = gl3::instance;
	this->impl.uninstance = gl3::uninstance;
	this->impl.render = gl3::render;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
	this->renderCB = nil;
}       

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	AttribDesc attribs[12], *a;
	uint32 stride;

	//
	// Create attribute descriptions
	//
	a = attribs;
	stride = 0;

	// Positions
	a->index = ATTRIB_POS;
	a->size = 3;
	a->type = GL_FLOAT;
	a->normalized = GL_FALSE;
	a->offset = stride;
	stride += 12;
	a++;

	// Normals
	// TODO: compress
	bool hasNormals = !!(geo->geoflags & Geometry::NORMALS);
	if(hasNormals){
		a->index = ATTRIB_NORMAL;
		a->size = 3;
		a->type = GL_FLOAT;
		a->normalized = GL_FALSE;
		a->offset = stride;
		stride += 12;
		a++;
	}

	// Prelighting
	bool isPrelit = !!(geo->geoflags & Geometry::PRELIT);
	if(isPrelit){
		a->index = ATTRIB_COLOR;
		a->size = 4;
		a->type = GL_UNSIGNED_BYTE;
		a->normalized = GL_TRUE;
		a->offset = stride;
		stride += 4;
		a++;
	}

	// Texture coordinates
	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		a->index = ATTRIB_TEXCOORDS0+n;
		a->size = 2;
		a->type = GL_FLOAT;
		a->normalized = GL_FALSE;
		a->offset = stride;
		stride += 8;
		a++;
	}

	header->numAttribs = a - attribs;
	for(a = attribs; a != &attribs[header->numAttribs]; a++)
		a->stride = stride;
	header->attribDesc = new AttribDesc[header->numAttribs];
	memcpy(header->attribDesc, attribs,
	       header->numAttribs*sizeof(AttribDesc));

	//
	// Allocate and fill vertex buffer
	//
	uint8 *verts = new uint8[header->totalNumVertex*stride];
	header->vertexBuffer = verts;

	// Positions
	for(a = attribs; a->index != ATTRIB_POS; a++)
		;
	instV3d(VERT_FLOAT3, verts + a->offset,
	        geo->morphTargets[0].vertices,
	        header->totalNumVertex, a->stride);

	// Normals
	if(hasNormals){
		for(a = attribs; a->index != ATTRIB_NORMAL; a++)
			;
		instV3d(VERT_FLOAT3, verts + a->offset,
			geo->morphTargets[0].normals,
			header->totalNumVertex, a->stride);
	}

	// Prelighting
	if(isPrelit){
		for(a = attribs; a->index != ATTRIB_COLOR; a++)
			;
		instColor(VERT_RGBA, verts + a->offset,
			  geo->colors,
			  header->totalNumVertex, a->stride);
	}

	// Texture coordinates
	for(int32 n = 0; n < geo->numTexCoordSets; n++){
		for(a = attribs; a->index != ATTRIB_TEXCOORDS0+n; a++)
			;
		instV2d(VERT_FLOAT2, verts + a->offset,
			geo->texCoords[n],
			header->totalNumVertex, a->stride);
	}

	glGenBuffers(1, &header->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, header->vbo);
	glBufferData(GL_ARRAY_BUFFER, header->totalNumVertex*stride,
	             header->vertexBuffer, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	assert(0 && "can't uninstance");
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_GL3);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	return pipe;
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_GL3);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_GL3);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = defaultRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

#endif

}
}
