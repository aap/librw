#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rw.h"
#include "rwps2.h"

using namespace std;

namespace Rw {

void*
DestroyNativeDataPS2(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_PS2);
	PS2InstanceDataHeader *header =
		(PS2InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++)
		delete[] header->instanceMeshes[i].data;
	delete[] header->instanceMeshes;
	delete header;
	return object;
}

void
ReadNativeDataPS2(istream &stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	assert(readUInt32(stream) == PLATFORM_PS2);
	PS2InstanceDataHeader *header = new PS2InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_PS2;
	assert(geometry->meshHeader != NULL);
	header->numMeshes = geometry->meshHeader->numMeshes;
	header->instanceMeshes = new PS2InstanceData[header->numMeshes];
	for(uint32 i = 0; i < header->numMeshes; i++){
		PS2InstanceData *instance = &header->instanceMeshes[i];
		uint32 buf[2];
		stream.read((char*)buf, 8);
		instance->dataSize = buf[0];
		instance->noRefChain = buf[1];
		instance->data = new uint8[instance->dataSize];
		stream.read((char*)instance->data, instance->dataSize);
	}
}

void
WriteNativeDataPS2(ostream &stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	WriteChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData->platform == PLATFORM_PS2);
	writeUInt32(PLATFORM_PS2, stream);
	assert(geometry->instData != NULL);
	PS2InstanceDataHeader *header =
		(PS2InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		PS2InstanceData *instance = &header->instanceMeshes[i];
		uint32 buf[2];
		buf[0] = instance->dataSize;
		buf[1] = instance->noRefChain;
		stream.write((char*)buf, 8);
		stream.write((char*)instance->data, instance->dataSize);
	}
}

int32
GetSizeNativeDataPS2(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	int32 size = 16;
	assert(geometry->instData->platform == PLATFORM_PS2);
	assert(geometry->instData != NULL);
	PS2InstanceDataHeader *header =
		(PS2InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		PS2InstanceData *instance = &header->instanceMeshes[i];
		size += 8;
		size += instance->dataSize;
	}
	return size;
}

}
