#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwd3d9.h"

using namespace std;

namespace rw {
namespace d3d9 {

#ifdef RW_D3D9
IDirect3DDevice9 *device = NULL;
#else
enum {
	D3DLOCK_NOSYSLOCK     =  0,  // ignored
	D3DPOOL_MANAGED       =  0,  // ignored
	D3DPT_TRIANGLELIST    =  4,
	D3DPT_TRIANGLESTRIP   =  5,


	D3DDECLTYPE_FLOAT1    =  0,  // 1D float expanded to (value, 0., 0., 1.)
	D3DDECLTYPE_FLOAT2    =  1,  // 2D float expanded to (value, value, 0., 1.)
	D3DDECLTYPE_FLOAT3    =  2,  // 3D float expanded to (value, value, value, 1.)
	D3DDECLTYPE_FLOAT4    =  3,  // 4D float
	D3DDECLTYPE_D3DCOLOR  =  4,  // 4D packed unsigned bytes mapped to 0. to 1. range
	                             // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
	D3DDECLTYPE_UBYTE4    =  5,  // 4D unsigned byte
	D3DDECLTYPE_SHORT2    =  6,  // 2D signed short expanded to (value, value, 0., 1.)
	D3DDECLTYPE_SHORT4    =  7,  // 4D signed short

	D3DDECLTYPE_UBYTE4N   =  8,  // Each of 4 bytes is normalized by dividing to 255.0
	D3DDECLTYPE_SHORT2N   =  9,  // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
	D3DDECLTYPE_SHORT4N   = 10,  // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
	D3DDECLTYPE_USHORT2N  = 11,  // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
	D3DDECLTYPE_USHORT4N  = 12,  // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
	D3DDECLTYPE_UDEC3     = 13,  // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
	D3DDECLTYPE_DEC3N     = 14,  // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
	D3DDECLTYPE_FLOAT16_2 = 15,  // Two 16-bit floating point values, expanded to (value, value, 0, 1)
	D3DDECLTYPE_FLOAT16_4 = 16,  // Four 16-bit floating point values
	D3DDECLTYPE_UNUSED    = 17,  // When the type field in a decl is unused.


	D3DDECLMETHOD_DEFAULT =  0,


	D3DDECLUSAGE_POSITION = 0,
	D3DDECLUSAGE_BLENDWEIGHT,   // 1
	D3DDECLUSAGE_BLENDINDICES,  // 2
	D3DDECLUSAGE_NORMAL,        // 3
	D3DDECLUSAGE_PSIZE,         // 4
	D3DDECLUSAGE_TEXCOORD,      // 5
	D3DDECLUSAGE_TANGENT,       // 6
	D3DDECLUSAGE_BINORMAL,      // 7
	D3DDECLUSAGE_TESSFACTOR,    // 8
	D3DDECLUSAGE_POSITIONT,     // 9
	D3DDECLUSAGE_COLOR,         // 10
	D3DDECLUSAGE_FOG,           // 11
	D3DDECLUSAGE_DEPTH,         // 12
	D3DDECLUSAGE_SAMPLE,        // 13
};
#define D3DDECL_END() {0xFF,0,D3DDECLTYPE_UNUSED,0,0,0}
#define D3DCOLOR_ARGB(a,r,g,b) \
    ((uint32)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#endif

int vertFormatMap[] = {
	-1, VERT_FLOAT2, VERT_FLOAT3, -1, VERT_ARGB
};

uint16*
lockIndices(void *indexBuffer, uint32 offset, uint32 size, uint32 flags)
{
#ifdef RW_D3D9
	uint16 *indices;
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Lock(offset, size, (void**)&indices, flags);
	return indices;
#else
	return (uint16*)indexBuffer;
#endif
}

void
unlockIndices(void *indexBuffer)
{
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf = (IDirect3DIndexBuffer9*)indexBuffer;
	ibuf->Unlock();
#endif
}

uint8*
lockVertices(void *vertexBuffer, uint32 offset, uint32 size, uint32 flags)
{
#ifdef RW_D3D9
	uint8 *verts;
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Lock(offset, size, (void**)&verts, flags);
	return verts;
#else
	return (uint8*)vertexBuffer;
#endif
}

void
unlockVertices(void *vertexBuffer)
{
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vertbuf = (IDirect3DVertexBuffer9*)vertexBuffer;
	vertbuf->Unlock();
#endif
}

void*
createVertexDeclaration(VertexElement *elements)
{
#ifdef RW_D3D9
	IDirect3DVertexDeclaration9 *decl = 0;
	device->CreateVertexDeclaration((D3DVERTEXELEMENT9*)elements, &decl);
	return decl;
#else
	int n = 0;
	VertexElement *e = (VertexElement*)elements;
	while(e[n++].stream != 0xFF)
		;
	e = new VertexElement[n];
	memcpy(e, elements, n*sizeof(VertexElement));
	return e;
#endif
}

uint32
getDeclaration(void *declaration, VertexElement *elements)
{
#ifdef RW_D3D9
	IDirect3DVertexDeclaration9 *decl = (IDirect3DVertexDeclaration9*)declaration;
	UINT numElt;
	decl->GetDeclaration((D3DVERTEXELEMENT9*)elements, &numElt);
	return numElt;
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

void*
createIndexBuffer(uint32 length)
{
#ifdef RW_D3D9
	IDirect3DIndexBuffer9 *ibuf;
	device->CreateIndexBuffer(length, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, 0);
	return ibuf;
#else
	return new uint8[length];
#endif
}

void*
createVertexBuffer(uint32 length, int32 pool)
{
#ifdef RW_D3D9
	IDirect3DVertexBuffer9 *vbuf;
	device->CreateVertexBuffer(length, D3DUSAGE_WRITEONLY, 0, (D3DPOOL)pool, &vbuf, 0);
	return vbuf;
#else
	return new uint8[length];
#endif
}

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D9);
	// TODO
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	delete header;
	return object;
}

void
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	uint32 vers;
	assert(findChunk(stream, ID_STRUCT, NULL, &vers));
	assert(stream->readU32() == PLATFORM_D3D9);
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_D3D9;
	int32 size = stream->readI32();
	uint8 *data = new uint8[size];
	stream->read(data, size);
	uint8 *p = data;
	header->serialNumber = *(uint32*)p; p += 4;
	header->numMeshes = *(uint32*)p; p += 4;
	header->indexBuffer = NULL; p += 4;
	header->primType = *(uint32*)p; p += 4;
	p += 16*2;	// skip vertex streams, they're repeated with the vertex buffers
	header->useOffsets = *(bool32*)p; p += 4;
	header->vertexDeclaration = NULL; p += 4;
	header->totalNumIndex = *(uint32*)p; p += 4;
	header->totalNumVertex = *(uint32*)p; p += 4;
	header->inst = new InstanceData[header->numMeshes];

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		inst->numIndex = *(uint32*)p; p += 4;
		inst->minVert = *(uint32*)p; p += 4;
		uint32 matid = *(uint32*)p; p += 4;
		inst->material = geometry->materialList[matid];
		inst->vertexAlpha = *(bool32*)p; p += 4;
		inst->vertexShader = NULL; p += 4;
		inst->baseIndex = 0; p += 4;
		inst->numVertices = *(uint32*)p; p += 4;
		inst->startIndex = *(uint32*)p; p += 4;
		inst->numPrimitives = *(uint32*)p; p += 4;
		inst++;
	}

	VertexElement elements[10];
	uint32 numDeclarations = stream->readU32();
	stream->read(elements, numDeclarations*8);
	header->vertexDeclaration = createVertexDeclaration(elements);

	header->indexBuffer = createIndexBuffer(header->totalNumIndex*sizeof(uint16));

	uint16 *indices = lockIndices(header->indexBuffer, 0, 0, 0);
	stream->read(indices, 2*header->totalNumIndex);
	unlockIndices(header->indexBuffer);

	VertexStream *s;
	p = data;
	for(int i = 0; i < 2; i++){
		stream->read(p, 16);
		s = &header->vertexStream[i];
		s->vertexBuffer = (void*)*(uint32*)p; p += 4;
		s->offset = 0; p += 4;
		s->stride = *(uint32*)p; p += 4;
		s->geometryFlags = *(uint16*)p; p += 2;
		s->managed = *p++;
		s->dynamicLock = *p++;

		if(s->vertexBuffer == NULL)
			continue;
		// TODO: unset managed flag when using morph targets.
		//       also uses different buffer type and locks differently
		s->vertexBuffer = createVertexBuffer(s->stride*header->totalNumVertex, D3DPOOL_MANAGED);

		uint8 *verts = lockVertices(s->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
		stream->read(verts, s->stride*header->totalNumVertex);
		unlockVertices(s->vertexBuffer);
	}

	// TODO: somehow depends on number of streams used (baseIndex = minVert when more than one)
	inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		inst->baseIndex = inst->minVert + header->vertexStream[0].offset / header->vertexStream[0].stride;
		inst++;
	}

	delete[] data;
}

void
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D9);
	stream->writeU32(PLATFORM_D3D9);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	int32 size = 64 + geometry->meshHeader->numMeshes*36;
	uint8 *data = new uint8[size];
	stream->writeI32(size);

	uint8 *p = data;
	*(uint32*)p = header->serialNumber; p += 4;
	*(uint32*)p = header->numMeshes; p += 4;
	p += 4;		// skip index buffer
	*(uint32*)p = header->primType; p += 4;
	p += 16*2;	// skip vertex streams, they're repeated with the vertex buffers
	*(bool32*)p = header->useOffsets; p += 4;
	p += 4;		// skip vertex declaration
	*(uint32*)p = header->totalNumIndex; p += 4;
	*(uint32*)p = header->totalNumVertex; p += 4;

	InstanceData *inst = header->inst;
	for(uint32 i = 0; i < header->numMeshes; i++){
		*(uint32*)p = inst->numIndex; p += 4;
		*(uint32*)p = inst->minVert; p += 4;
		int32 matid = findPointer(inst->material, (void**)geometry->materialList, geometry->numMaterials);
		*(int32*)p = matid; p += 4;
		*(bool32*)p = inst->vertexAlpha; p += 4;
		*(uint32*)p = 0; p += 4;		// vertex shader
		*(uint32*)p = inst->baseIndex; p += 4;	// not used but meh...
		*(uint32*)p = inst->numVertices; p += 4;
		*(uint32*)p = inst->startIndex; p += 4;
		*(uint32*)p = inst->numPrimitives; p += 4;
		inst++;
	}
	stream->write(data, size);

	VertexElement elements[10];
	uint32 numElt = getDeclaration(header->vertexDeclaration, elements);
	stream->writeU32(numElt);
	stream->write(elements, 8*numElt);

	uint16 *indices = lockIndices(header->indexBuffer, 0, 0, 0);
	stream->write(indices, 2*header->totalNumIndex);
	unlockIndices(header->indexBuffer);

	VertexStream *s;
	for(int i = 0; i < 2; i++){
		s = &header->vertexStream[i];
		p = data;
		*(uint32*)p = s->vertexBuffer ? 0xbadeaffe : 0; p += 4;
		*(uint32*)p = s->offset; p += 4;
		*(uint32*)p = s->stride; p += 4;
		*(uint16*)p = s->geometryFlags; p += 2;
		*p++ = s->managed;
		*p++ = s->dynamicLock;
		stream->write(data, 16);

		if(s->vertexBuffer == NULL)
			continue;
		uint8 *verts = lockVertices(s->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
		stream->write(verts, s->stride*header->totalNumVertex);
		unlockVertices(s->vertexBuffer);
	}

	delete[] data;
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_D3D9);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	int32 size = 12 + 4 + 4 + 64 + header->numMeshes*36;
	uint32 numElt = getDeclaration(header->vertexDeclaration, NULL);
	size += 4 + numElt*8;
	size += 2*header->totalNumIndex;
	size += 0x10 + header->vertexStream[0].stride*header->totalNumVertex;
	size += 0x10 + header->vertexStream[1].stride*header->totalNumVertex;
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
	header->platform = PLATFORM_D3D9;

	header->serialNumber = 0;
	header->numMeshes = meshh->numMeshes;
	header->primType = meshh->flags == 1 ? D3DPT_TRIANGLESTRIP : D3DPT_TRIANGLELIST;
	header->useOffsets = 0;
	header->totalNumVertex = geo->numVertices;
	header->totalNumIndex = meshh->totalIndices;
	header->inst = new InstanceData[header->numMeshes];

	header->indexBuffer = createIndexBuffer(header->totalNumIndex*sizeof(uint16));

	uint16 *indices = lockIndices(header->indexBuffer, 0, 0, 0);
	InstanceData *inst = header->inst;
	Mesh *mesh = meshh->mesh;
	uint32 startindex = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		findMinVertAndNumVertices(mesh->indices, mesh->numIndices,
		                          &inst->minVert, &inst->numVertices);
		inst->numIndex = mesh->numIndices;
		inst->material = mesh->material;
		inst->vertexAlpha = 0;
		inst->vertexShader = NULL;
		inst->baseIndex = inst->minVert;
		inst->startIndex = startindex;
		inst->numPrimitives = header->primType == D3DPT_TRIANGLESTRIP ? inst->numIndex-2 : inst->numIndex/3;
		if(inst->minVert == 0)
			memcpy(&indices[inst->startIndex], mesh->indices, inst->numIndex*sizeof(uint16));
		else
			for(uint32 j = 0; j < inst->numIndex; j++)
				indices[inst->startIndex+j] = mesh->indices[j]-inst->minVert;
		startindex += inst->numIndex;
		mesh++;
		inst++;
	}
	unlockIndices(header->indexBuffer);

	VertexStream *s;
	for(int i = 0; i < 2; i++){
		s = &header->vertexStream[i];
		s->vertexBuffer = NULL;
		s->offset = 0;
		s->stride = 0;
		s->geometryFlags = 0;
		s->managed = 0;
		s->dynamicLock = 0;
	}

	this->instanceCB(geo, header);
}

void
ObjPipeline::uninstance(Atomic *atomic)
{
	assert(0 && "can't uninstance");
}

void
defaultInstanceCB(Geometry *geo, InstanceDataHeader *header)
{
	VertexElement dcl[6];

	VertexStream *s = &header->vertexStream[0];
	s->offset = 0;
	s->managed = 1;
	s->geometryFlags = 0;
	s->dynamicLock = 0;

	int i = 0;
	dcl[i++] = {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0};
	uint32 stride = 12;
	s->geometryFlags |= 0x2;

	bool isPrelit = (geo->geoflags & Geometry::PRELIT) != 0;
	if(isPrelit){
		dcl[i++] = {0, stride, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0};
		s->geometryFlags |= 0x8;
		stride += 4;
	}

	bool isTextured = (geo->geoflags & (Geometry::TEXTURED | Geometry::TEXTURED2)) != 0;
	if(isTextured){
		dcl[i++] = {0, stride, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0};
		s->geometryFlags |= 0x10;
		stride += 8;
	}

	bool hasNormals = (geo->geoflags & Geometry::NORMALS) != 0;
	if(hasNormals){
		dcl[i++] = {0, stride, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0};
		s->geometryFlags |= 0x4;
		stride += 12;
	}
	dcl[i] = D3DDECL_END();
	header->vertexStream[0].stride = stride;

	header->vertexDeclaration = createVertexDeclaration((VertexElement*)dcl);

	s->vertexBuffer = createVertexBuffer(header->totalNumVertex*s->stride, D3DPOOL_MANAGED);

	uint8 *verts = lockVertices(s->vertexBuffer, 0, 0, D3DLOCK_NOSYSLOCK);
	for(i = 0; dcl[i].usage != D3DDECLUSAGE_POSITION || dcl[i].usageIndex != 0; i++)
		;
	instV3d(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
		geo->morphTargets[0].vertices,
		header->totalNumVertex,
		header->vertexStream[dcl[i].stream].stride);

	if(isPrelit){
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_COLOR || dcl[i].usageIndex != 0; i++)
			;
		instColor(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
			  geo->colors,
			  header->totalNumVertex,
			  header->vertexStream[dcl[i].stream].stride);
	}

	if(isTextured){
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_TEXCOORD || dcl[i].usageIndex != 0; i++)
			;
		instV2d(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
			geo->texCoords[0],
			header->totalNumVertex,
			header->vertexStream[dcl[i].stream].stride);
	}

	if(hasNormals){
		for(i = 0; dcl[i].usage != D3DDECLUSAGE_NORMAL || dcl[i].usageIndex != 0; i++)
			;
		instV3d(vertFormatMap[dcl[i].type], verts + dcl[i].offset,
			geo->morphTargets[0].normals,
			header->totalNumVertex,
			header->vertexStream[dcl[i].stream].stride);
	}
	unlockVertices(s->vertexBuffer);
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D9);
	pipe->instanceCB = defaultInstanceCB;
	return pipe;
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D9);
	pipe->instanceCB = defaultInstanceCB;
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	return pipe;
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_D3D9);
	pipe->instanceCB = defaultInstanceCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

}
}
