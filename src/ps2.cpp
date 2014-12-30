#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "rwps2.h"

using namespace std;

namespace Rw {
namespace Ps2 {

void*
DestroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_PS2);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++)
		delete[] header->instanceMeshes[i].data;
	delete[] header->instanceMeshes;
	delete header;
	return object;
}

void
ReadNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	assert(stream->readU32() == PLATFORM_PS2);
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_PS2;
	assert(geometry->meshHeader != NULL);
	header->numMeshes = geometry->meshHeader->numMeshes;
	header->instanceMeshes = new InstanceData[header->numMeshes];
	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *instance = &header->instanceMeshes[i];
		uint32 buf[2];
		stream->read(buf, 8);
		instance->dataSize = buf[0];
		instance->arePointersFixed = buf[1];
// TODO: force alignment
		instance->data = new uint8[instance->dataSize];
#ifdef RW_PS2
		uint32 a = (uint32)instance->data;
		assert(a % 0x10 == 0);
#endif
		stream->read(instance->data, instance->dataSize);
	}
}

void
WriteNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	WriteChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData->platform == PLATFORM_PS2);
	stream->writeU32(PLATFORM_PS2);
	assert(geometry->instData != NULL);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *instance = &header->instanceMeshes[i];
		if(instance->arePointersFixed == 2)
			unfixDmaOffsets(instance);
		uint32 buf[2];
		buf[0] = instance->dataSize;
		buf[1] = instance->arePointersFixed;
		stream->write(buf, 8);
		stream->write(instance->data, instance->dataSize);
	}
}

int32
GetSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	int32 size = 16;
	assert(geometry->instData->platform == PLATFORM_PS2);
	assert(geometry->instData != NULL);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		InstanceData *instance = &header->instanceMeshes[i];
		size += 8;
		size += instance->dataSize;
	}
	return size;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         NULL, DestroyNativeData, NULL);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               (StreamRead)ReadNativeData,
	                               (StreamWrite)WriteNativeData,
	                               (StreamGetSize)GetSizeNativeData);
}

#ifdef RW_PS2
void
fixDmaOffsets(InstanceData *inst)
{
	if(inst->arePointersFixed)
		return;

	uint32 base = (uint32)inst->data;
	uint32 *tag = (uint32*)inst->data;
	for(;;){
		switch(tag[0]&0x70000000){
		// DMAcnt
		case 0x10000000:
			// no need to fix
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		// DMAref
		case 0x30000000:
			// fix address and jump to next
			tag[1] = base + tag[1]*0x10;
			tag += 4;
			break;

		// DMAret
		case 0x60000000:
			// we're done
			inst->arePointersFixed = 2;
			return;

		default:
			fprintf(stderr, "error: unknown DMAtag %X\n", tag[0]);
			return;
		}
	}
}
#endif

void
unfixDmaOffsets(InstanceData *inst)
{
#ifdef RW_PS2
	if(inst->arePointersFixed != 2)
		return;

	uint32 base = (uint32)inst->data;
	uint32 *tag = (uint32*)inst->data;
	for(;;){
		switch(tag[0]&0x70000000){
		// DMAcnt
		case 0x10000000:
			// no need to unfix
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		// DMAref
		case 0x30000000:
			// unfix address and jump to next
			tag[1] = (tag[1] - base)/0x10;
			tag += 4;
			break;

		// DMAret
		case 0x60000000:
			// we're done
			inst->arePointersFixed = 0;
			return;

		default:
			fprintf(stderr, "error: unknown DMAtag %X\n", tag[0]);
			return;
		}
	}
#endif
}

}
}
