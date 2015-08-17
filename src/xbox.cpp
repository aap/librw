#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwxbox.h"

using namespace std;

namespace rw {
namespace xbox {

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_XBOX);
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
	assert(stream->readU32() == PLATFORM_XBOX);
	assert(vers >= 0x35000 && "can't handle native Xbox data < 0x35000");
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_XBOX;

	int32 size = stream->readI32();
	// The 0x18 byte are the resentryheader.
	// We don't have it but it's used for alignment.
	header->data = new uint8[size + 0x18];
	uint8 *p = header->data+0x18+4;
	stream->read(p, size-4);

	header->size = size;
	header->serialNumber = *(uint16*)p; p += 2;
	header->numMeshes = *(uint16*)p; p += 2;
	header->primType = *(uint32*)p; p += 4;
	header->numVertices = *(uint32*)p; p += 4;
	header->stride = *(uint32*)p; p += 4;
	// RxXboxVertexFormat in 3.3 here
	p += 4;	// skip vertexBuffer pointer
	header->vertexAlpha = *(bool32*)p; p += 4;
	p += 8; // skip begin, end pointers

	InstanceData *inst  = new InstanceData[header->numMeshes];
	header->begin = inst;
	for(int i = 0; i < header->numMeshes; i++){
		inst->minVert = *(uint32*)p; p += 4;
		inst->numVertices = *(int32*)p; p += 4;
		inst->numIndices = *(int32*)p; p += 4;
		inst->indexBuffer = header->data + *(uint32*)p; p += 4;
		p += 8; // skip material and vertexShader
		inst->vertexShader = 0;
		// pixelShader in 3.3 here
		inst++;
	}
	header->end = inst;

	header->vertexBuffer = new uint8[header->stride*header->numVertices];
	stream->read(header->vertexBuffer, header->stride*header->numVertices);
}

void
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_XBOX);
	stream->writeU32(PLATFORM_XBOX);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;

	// we just fill header->data and write that
	uint8 *p = header->data+0x18;
	//uint8 *end = (uint8*)header->begin->indexBuffer;
	//memset(p, 0xAB, end-p);
	*(int32*)p = header->size; p += 4;
	*(uint16*)p = header->serialNumber; p += 2;
	*(uint16*)p = header->numMeshes; p += 2;
	*(uint32*)p = header->primType; p += 4;
	*(uint32*)p = header->numVertices; p += 4;
	*(uint32*)p = header->stride; p += 4;
	// RxXboxVertexFormat in 3.3 here
	p += 4;	// skip vertexBuffer pointer
	*(bool32*)p = header->vertexAlpha; p += 4;
	p += 8; // skip begin, end pointers

	InstanceData *inst = header->begin;
	for(int i = 0; i < header->numMeshes; i++){
		*(uint32*)p = inst->minVert; p += 4;
		*(int32*)p = inst->numVertices; p += 4;
		*(int32*)p = inst->numIndices; p += 4;
		*(uint32*)p = (uint8*)inst->indexBuffer - header->data; p += 4;
		p += 8; // skip material and vertexShader
		// pixelShader in 3.3 here
		inst++;
	}

	stream->write(header->data+0x18, header->size);
	stream->write(header->vertexBuffer, header->stride*header->numVertices);
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_XBOX);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	return 12 + 4 + header->size + header->stride*header->numVertices;
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

// Skin plugin

struct NativeSkin
{
	int32 table1[256];	// maps indices to bones
	int32 table2[256];	// maps bones to indices
	int32 numUsedBones;
	void *vertexBuffer;
	int32 stride;
};

void
readNativeSkin(Stream *stream, int32, void *object, int32 offset)
{
	Geometry *geometry = (Geometry*)object;
	uint32 vers;
	assert(findChunk(stream, ID_STRUCT, NULL, &vers));
	assert(vers >= 0x35000 && "can't handle native xbox skin < 0x35000");
	assert(stream->readU32() == PLATFORM_XBOX);
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;
	skin->numBones = stream->readI32();

	// only allocate matrices
	skin->numUsedBones = 0;
	skin->allocateData(0);
	NativeSkin *natskin = new NativeSkin;
	skin->platformData = natskin;
	stream->read(natskin->table1, 256*sizeof(int32));
	stream->read(natskin->table2, 256*sizeof(int32));
	// we use our own variable for this due to allocation
	natskin->numUsedBones = stream->readI32();
	skin->maxIndex = stream->readI32();
	stream->seek(4);	// skip pointer to vertexBuffer
	natskin->stride = stream->readI32();
	int32 size = geometry->numVertices*natskin->stride;
	natskin->vertexBuffer = new uint8[size];
	stream->read(natskin->vertexBuffer, size);
	stream->read(skin->inverseMatrices, skin->numBones*64);

	// no split skins in GTA
	stream->seek(12);
}

void
writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset)
{
	Geometry *geometry = (Geometry*)object;
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	assert(skin->platformData);
	assert(rw::version >= 0x35000 && "can't handle native xbox skin < 0x35000");
	NativeSkin *natskin = (NativeSkin*)skin->platformData;

	writeChunkHeader(stream, ID_STRUCT, len-12);
	stream->writeU32(PLATFORM_XBOX);
	stream->writeI32(skin->numBones);
	stream->write(natskin->table1, 256*sizeof(int32));
	stream->write(natskin->table2, 256*sizeof(int32));
	stream->writeI32(natskin->numUsedBones);
	stream->writeI32(skin->maxIndex);
	stream->writeU32(0xBADEAFFE);	// pointer to vertexBuffer
	stream->writeI32(natskin->stride);
	stream->write(natskin->vertexBuffer,
	              geometry->numVertices*natskin->stride);
	stream->write(skin->inverseMatrices, skin->numBones*64);
	int32 buffer[3] = { 0, 0, 0};
	stream->write(buffer, 12);
}

int32
getSizeNativeSkin(void *object, int32 offset)
{
	Geometry *geometry = (Geometry*)object;
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == NULL)
		return -1;
	assert(skin->platformData);
	NativeSkin *natskin = (NativeSkin*)skin->platformData;
	return 12 + 8 + 2*256*4 + 4*4 +
	        natskin->stride*geometry->numVertices + skin->numBones*64 + 12;
}

// Vertex Format Plugin

static void*
createVertexFmt(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, object, offset) = 0;
	return object;
}

static void*
copyVertexFmt(void *dst, void *src, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, dst, offset) = *PLUGINOFFSET(uint32, src, offset);
	return dst;
}

static void
readVertexFmt(Stream *stream, int32, void *object, int32 offset, int32)
{
	uint32 fmt = stream->readU32();
//	printf("vertexfmt: %X\n", fmt);
	*PLUGINOFFSET(uint32, object, offset) = fmt;
	// TODO: create and attach "vertex shader"
}

static void
writeVertexFmt(Stream *stream, int32, void *object, int32 offset, int32)
{
	stream->writeI32(*PLUGINOFFSET(uint32, object, offset));
}

static int32
getSizeVertexFmt(void*, int32, int32)
{
	// TODO: make dependent on platform
	return 4;
}

void
registerVertexFormatPlugin(void)
{
	Geometry::registerPlugin(sizeof(uint32), ID_VERTEXFMT,
	                         createVertexFmt, NULL, copyVertexFmt);
	Geometry::registerPluginStream(ID_VERTEXFMT,
	                               readVertexFmt,
	                               writeVertexFmt,
	                               getSizeVertexFmt);
}

}
}
