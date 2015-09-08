#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwd3d.h"
#include "rwd3d8.h"

using namespace std;

namespace rw {
namespace d3d8 {
using namespace d3d;

uint32
makeFVFDeclaration(uint32 flags, int32 numTex)
{
	uint32 fvf = 0x2;
	if(flags & Geometry::NORMALS)
		fvf |= 0x10;
	if(flags & Geometry::PRELIT)
		fvf |= 0x40;
	fvf |= numTex << 8;
	return fvf;
}

int32
getStride(uint32 flags, int32 numTex)
{
	int32 stride = 12;
	if(flags & Geometry::NORMALS)
		stride += 12;;
	if(flags & Geometry::PRELIT)
		stride += 4;
	stride += numTex*8;
	return stride;
}

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D8);
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	geometry->instData = NULL;
	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		deleteObject(inst->indexBuffer);
		deleteObject(inst->vertexBuffer);
		inst++;
	}
	delete[] header->inst;
	delete header;
	return object;
}

void
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	uint32 vers;
	assert(findChunk(stream, ID_STRUCT, NULL, &vers));
	assert(stream->readU32() == PLATFORM_D3D8);
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_D3D8;

	int32 size = stream->readI32();
	uint8 *data = new uint8[size];
	stream->read(data, size);
	uint8 *p = data;
	header->serialNumber = *(uint16*)p; p += 2;
	header->numMeshes = *(uint16*)p; p += 2;
	header->inst = new InstanceData[header->numMeshes];

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		inst->minVert = *(uint32*)p; p += 4;
		inst->stride = *(uint32*)p; p += 4;
		inst->numVertices = *(uint32*)p; p += 4;
		inst->numIndices = *(uint32*)p; p += 4;
		uint32 matid = *(uint32*)p; p += 4;
		inst->material = geometry->materialList[matid];
		inst->vertexShader = *(uint32*)p; p += 4;
		inst->primType = *(uint32*)p; p += 4;
		inst->indexBuffer = NULL; p += 4;
		inst->vertexBuffer = NULL; p += 4;
		inst->baseIndex = 0; p += 4;
		inst->vertexAlpha = *p++;
		inst->managed = 0; p++;
		inst->remapped = 0; p++;	// TODO: really unused? and what's that anyway?
		inst++;
	}
	delete[] data;

	inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		inst->indexBuffer = createIndexBuffer(inst->numIndices*2);
		uint16 *indices = lockIndices(inst->indexBuffer, 0, 0, 0);
		stream->read(indices, 2*inst->numIndices);
		unlockIndices(inst->indexBuffer);

		inst->managed = 1;
		inst->vertexBuffer = createVertexBuffer(inst->stride*inst->numVertices, 0, D3DPOOL_MANAGED);
		uint8 *verts = lockVertices(inst->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
		stream->read(verts, inst->stride*inst->numVertices);
		unlockVertices(inst->vertexBuffer);

		inst++;
	}
}

void
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D8);
	stream->writeU32(PLATFORM_D3D8);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;

	int32 size = 4 + geometry->meshHeader->numMeshes*0x2C;
	uint8 *data = new uint8[size];
	stream->writeI32(size);
	uint8 *p = data;
	*(uint16*)p = header->serialNumber; p += 2;
	*(uint16*)p = header->numMeshes; p += 2;

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		*(uint32*)p = inst->minVert; p += 4;
		*(uint32*)p = inst->stride; p += 4;
		*(uint32*)p = inst->numVertices; p += 4;
		*(uint32*)p = inst->numIndices; p += 4;
		int32 matid = findPointer(inst->material, (void**)geometry->materialList, geometry->numMaterials);
		*(int32*)p = matid; p += 4;
		*(uint32*)p = inst->vertexShader; p += 4;
		*(uint32*)p = inst->primType; p += 4;
		*(uint32*)p = 0; p += 4;		// index buffer
		*(uint32*)p = 0; p += 4;		// vertex buffer
		*(uint32*)p = inst->baseIndex; p += 4;
		*p++ = inst->vertexAlpha;
		*p++ = inst->managed;
		*p++ = inst->remapped;
		inst++;
	}
	stream->write(data, size);
	delete[] data;

	inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		uint16 *indices = lockIndices(inst->indexBuffer, 0, 0, 0);
		stream->write(indices, 2*inst->numIndices);
		unlockIndices(inst->indexBuffer);

		uint8 *verts = lockVertices(inst->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
		stream->write(verts, inst->stride*inst->numVertices);
		unlockVertices(inst->vertexBuffer);
		inst++;
	}
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D8);

	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	InstanceData *inst = header->inst;
	int32 size = 12 + 4 + 4 + 4 + header->numMeshes*0x2C;
	for(int32 i = 0; i < header->numMeshes; i++){
		size += inst->numIndices*2 + inst->numVertices*inst->stride;
		inst++;
	}
	return size;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         NULL, destroyNativeData, NULL);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
}


ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform),
   instanceCB(NULL), uninstanceCB(NULL) { }

void
ObjPipeline::instance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo->geoflags & Geometry::NATIVE)
		return;
	geo->geoflags |= Geometry::NATIVE;
	InstanceDataHeader *header = new InstanceDataHeader;
	MeshHeader *meshh = geo->meshHeader;
	geo->instData = header;
	header->platform = PLATFORM_D3D8;

	header->serialNumber = 0;
	header->numMeshes = meshh->numMeshes;
	header->inst = new InstanceData[header->numMeshes];

	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->mesh;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, &inst->numVertices);
		inst->numIndices = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexShader = 0;
		inst->primType = meshh->flags == 1 ? D3DPT_TRIANGLESTRIP : D3DPT_TRIANGLELIST;
		inst->vertexBuffer = NULL;
		inst->baseIndex = 0;	// (maybe) not used by us
		inst->vertexAlpha = 0;
		inst->managed = 0;
		inst->remapped = 0;

		inst->indexBuffer = createIndexBuffer(inst->numIndices*2);
		uint16 *indices = lockIndices(inst->indexBuffer, 0, 0, 0);
		if(inst->minVert == 0)
			memcpy(indices, mesh->indices, inst->numIndices*2);
		else
			for(int32 j = 0; j < inst->numIndices; j++)
				indices[j] = mesh->indices[j] - inst->minVert;
		unlockIndices(inst->indexBuffer);

		this->instanceCB(geo, inst);
		mesh++;
		inst++;
	}
}

void
ObjPipeline::uninstance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		return;
	assert(geo->instData != NULL);
	assert(geo->instData->platform == PLATFORM_D3D8);
	geo->geoflags &= ~Geometry::NATIVE;
	geo->allocateData();
	geo->meshHeader->allocateIndices();

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	InstanceData *inst = header->inst;
	Mesh *mesh = geo->meshHeader->mesh;
	for(uint32 i = 0; i < header->numMeshes; i++){
		uint16 *indices = lockIndices(inst->indexBuffer, 0, 0, 0);
		if(inst->minVert == 0)
			memcpy(mesh->indices, indices, inst->numIndices*2);
		else
			for(int32 j = 0; j < inst->numIndices; j++)
				mesh->indices[j] = indices[j] + inst->minVert;
		unlockIndices(inst->indexBuffer);

		this->uninstanceCB(geo, inst);
		mesh++;
		inst++;
	}
	geo->generateTriangles();
	destroyNativeData(geo, 0, 0);
}

void
defaultInstanceCB(Geometry *geo, InstanceData *inst)
{
	inst->vertexShader = makeFVFDeclaration(geo->geoflags, geo->numTexCoordSets);
	inst->stride = getStride(geo->geoflags, geo->numTexCoordSets);

	inst->vertexBuffer = createVertexBuffer(inst->numVertices*inst->stride,
	                                              inst->vertexShader, D3DPOOL_MANAGED);
	inst->managed = 1;

	uint8 *dst = lockVertices(inst->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
	instV3d(VERT_FLOAT3, dst,
		&geo->morphTargets[0].vertices[3*inst->minVert],
		inst->numVertices, inst->stride);
	dst += 12;

	if(geo->geoflags & Geometry::NORMALS){
		instV3d(VERT_FLOAT3, dst,
		        &geo->morphTargets[0].normals[3*inst->minVert],
		        inst->numVertices, inst->stride);
		dst += 12;
	}

	inst->vertexAlpha = 0;
	if(geo->geoflags & Geometry::PRELIT){
		inst->vertexAlpha = instColor(VERT_ARGB, dst, &geo->colors[4*inst->minVert],
		                              inst->numVertices, inst->stride);
		dst += 4;
	}

	for(int32 i = 0; i < geo->numTexCoordSets; i++){
		instV2d(VERT_FLOAT2, dst, &geo->texCoords[i][2*inst->minVert],
		        inst->numVertices, inst->stride);
		dst += 8;
	}
	unlockVertices(inst->vertexBuffer);
}

void
defaultUninstanceCB(Geometry *geo, InstanceData *inst)
{
	uint8 *src = lockVertices(inst->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
	uninstV3d(VERT_FLOAT3,
		&geo->morphTargets[0].vertices[3*inst->minVert],
		src, inst->numVertices, inst->stride);
	src += 12;

	if(geo->geoflags & Geometry::NORMALS){
		uninstV3d(VERT_FLOAT3,
		          &geo->morphTargets[0].normals[3*inst->minVert],
		          src, inst->numVertices, inst->stride);
		src += 12;
	}

	inst->vertexAlpha = 0;
	if(geo->geoflags & Geometry::PRELIT){
		uninstColor(VERT_ARGB, &geo->colors[4*inst->minVert], src,
		            inst->numVertices, inst->stride);
		src += 4;
	}

	for(int32 i = 0; i < geo->numTexCoordSets; i++){
		uninstV2d(VERT_FLOAT2, &geo->texCoords[i][2*inst->minVert], src,
		          inst->numVertices, inst->stride);
		src += 8;
	}
	unlockVertices(inst->vertexBuffer);
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D8);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	return pipe;
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D8);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D8);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

}
}
