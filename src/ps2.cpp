#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwps2.h"

using namespace std;

namespace rw {
namespace ps2 {

ObjPipeline *defaultObjPipe;
MatPipeline *defaultMatPipe;

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_PS2);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++)
		delete[] header->instanceMeshes[i].data;
	delete[] header->instanceMeshes;
	delete header;
	return object;
}

void
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
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
//		sizedebug(instance);
	}
}

void
writeNativeData(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_STRUCT, len-12);
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_PS2);
	stream->writeU32(PLATFORM_PS2);
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
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	int32 size = 16;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_PS2);
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
	                         NULL, destroyNativeData, NULL);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
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
			tag[1] = base + tag[1]<<4;
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
	(void)inst;
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
			tag[1] = (tag[1] - base)>>4;
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

// Pipeline

enum PS2Attribs {
	AT_V2_32	= 0x64000000,
	AT_V2_16	= 0x65000000,
	AT_V2_8		= 0x66000000,
	AT_V3_32	= 0x68000000,
	AT_V3_16	= 0x69000000,
	AT_V3_8		= 0x6A000000,
	AT_V4_32	= 0x6C000000,
	AT_V4_16	= 0x6D000000,
	AT_V4_8		= 0x6E000000,
	AT_UNSGN	= 0x00004000,

	AT_RW		= 0x6
};

enum PS2AttibTypes {
	AT_XYZ		= 0,
	AT_UV		= 1,
	AT_RGBA		= 2,
	AT_NORMAL	= 3
};

PipeAttribute attribXYZ = { 
	"XYZ",
	AT_V3_32
};

PipeAttribute attribUV = {
	"UV",
	AT_V2_32
};

PipeAttribute attribUV2 = {
	"UV2",
	AT_V4_32
};

PipeAttribute attribRGBA = {
	"RGBA",
	AT_V4_8 | AT_UNSGN
};

PipeAttribute attribNormal = {
	"Normal",
	AT_V3_8		// RW has V4_8 but uses V3_8, wtf?
};

PipeAttribute attribWeights = {
	"Weights",
	AT_V4_32 | AT_RW
};

static uint32
attribSize(uint32 unpack)
{
	static uint32 size[] = { 32, 16, 8, 16 };
	return ((unpack>>26 & 3)+1)*size[unpack>>24 & 3]/8;
}

#define QWC(x) (((x)+0xF)>>4)

static uint32
getBatchSize(MatPipeline *pipe, uint32 vertCount)
{
	PipeAttribute *a;
	uint32 size = 1;
	for(uint i = 0; i < nelem(pipe->attribs); i++)
		if((a = pipe->attribs[i]) && (a->attrib & AT_RW) == 0){
			size++;
			size += QWC(vertCount*attribSize(a->attrib));
		}
	return size;
}

uint32*
instanceXYZ(uint32 *p, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	uint32 *d = (uint32*)g->morphTargets[0].vertices;
	for(uint32 i = idx; i < idx+n; i++){
		j = m->indices[i];
		*p++ = d[j*3+0];
		*p++ = d[j*3+1];
		*p++ = d[j*3+2];
	}
	while((uintptr)p % 0x10)
		*p++ = 0;
	return p;
}

uint32*
instanceUV(uint32 *p, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	uint32 *d = (uint32*)g->texCoords[0];
	if((g->geoflags & Geometry::TEXTURED) ||
	   (g->geoflags & Geometry::TEXTURED2))
		for(uint32 i = idx; i < idx+n; i++){
			j = m->indices[i];
			*p++ = d[j*2+0];
			*p++ = d[j*2+1];
		}
	else
		for(uint32 i = idx; i < idx+n; i++){
			*p++ = 0;
			*p++ = 0;
		}
	while((uintptr)p % 0x10)
		*p++ = 0;
	return p;
}

uint32*
instanceRGBA(uint32 *p, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	uint32 *d = (uint32*)g->colors;
	if((g->geoflags & Geometry::PRELIT))
		for(uint32 i = idx; i < idx+n; i++){
			j = m->indices[i];
			*p++ = d[j];
		}
	else
		for(uint32 i = idx; i < idx+n; i++)
			*p++ = 0xFF000000;
	while((uintptr)p % 0x10)
		*p++ = 0;
	return p;
}

uint32*
instanceNormal(uint32 *wp, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	float *d = g->morphTargets[0].normals;
	uint8 *p = (uint8*)wp;
	if((g->geoflags & Geometry::NORMALS))
		for(uint32 i = idx; i < idx+n; i++){
			j = m->indices[i];
			*p++ = d[j*3+0]*127.0f;
			*p++ = d[j*3+1]*127.0f;
			*p++ = d[j*3+2]*127.0f;
		}
	else
		for(uint32 i = idx; i < idx+n; i++){
			*p++ = 0;
			*p++ = 0;
			*p++ = 0;
		}
	while((uintptr)p % 0x10)
		*p++ = 0;
	return (uint32*)p;
}

MatPipeline::MatPipeline(uint32 platform)
 : rw::Pipeline(platform), instanceCB(NULL)
{
	for(int i = 0; i < 10; i++)
		this->attribs[i] = NULL;
}

void
MatPipeline::dump(void)
{
	if(this->platform != PLATFORM_PS2)
		return;
	PipeAttribute *a;
	for(uint i = 0; i < nelem(this->attribs); i++){
		a = this->attribs[i];
		if(a)
			printf("%d %s: %x\n", i, a->name, a->attrib);
	}
	printf("stride: %x\n", this->inputStride);
	printf("triSCount: %x\n", this->triStripCount);
	printf("triLCount: %x\n", this->triListCount);
	printf("vifOffset: %x\n", this->vifOffset);
}

void
MatPipeline::setTriBufferSizes(uint32 inputStride, uint32 stripCount)
{
	this->inputStride = inputStride;
	this->triListCount = stripCount/12*12;
	PipeAttribute *a;
	for(uint i = 0; i < nelem(this->attribs); i++){
		a = this->attribs[i];
		if(a && a->attrib & AT_RW)
			goto brokenout;
	}
	this->triStripCount = stripCount/4*4;
	return;
brokenout:
	this->triStripCount = (stripCount-2)/4*4+2;
}

uint32 markcnt = 0xf790;

void
MatPipeline::instance(Geometry *g, InstanceData *inst, Mesh *m)
{
	PipeAttribute *a;
	uint32 numAttribs = 0;
	uint32 numBrokenAttribs = 0;
	for(uint i = 0; i < nelem(this->attribs); i++)
		if(a = this->attribs[i])
			if(a->attrib & AT_RW)
				numBrokenAttribs++;
			else
				numAttribs++;
	uint32 numBatches = 0;
	uint32 totalVerts = 0;
	uint32 batchVertCount, lastBatchVertCount;
	if(g->meshHeader->flags == 1){	// tristrip
		for(uint i = 0; i < m->numIndices; i += this->triStripCount-2){
			numBatches++;
			totalVerts += m->numIndices-i < this->triStripCount ?
				m->numIndices-i : this->triStripCount;
		}
		batchVertCount = this->triStripCount;
		lastBatchVertCount = totalVerts%this->triStripCount;
	}else{				// trilist
		numBatches = (m->numIndices+this->triListCount-1) /
			this->triListCount;
		totalVerts = m->numIndices;
		batchVertCount = this->triListCount;
		lastBatchVertCount = totalVerts%this->triListCount;
	}

	uint32 batchSize = getBatchSize(this, batchVertCount);
	uint32 lastBatchSize = getBatchSize(this, lastBatchVertCount);
	uint32 size = 0;
	if(numBrokenAttribs == 0)
		size = 1 + batchSize*(numBatches-1) + lastBatchSize;
	else
		size = 2*numBatches +
		       (1+batchSize)*(numBatches-1) + 1+lastBatchSize;

	/* figure out size and addresses of broken out sections */
	uint32 attribPos[nelem(this->attribs)];
	uint32 size2 = 0;
	for(uint i = 0; i < nelem(this->attribs); i++)
		if((a = this->attribs[i]) && a->attrib & AT_RW){
			attribPos[i] = size2 + size;
			size2 += QWC(m->numIndices*attribSize(a->attrib));
		}

	inst->dataSize = (size+size2)<<4;
	inst->arePointersFixed = numBrokenAttribs == 0;
	// TODO: force alignment
	inst->data = new uint8[inst->dataSize];

	/* make array of addresses of broken out sections */
	uint8 *datap[nelem(this->attribs)];
	uint8 **dp = datap;
	for(uint i = 0; i < nelem(this->attribs); i++)
		if((a = this->attribs[i]) && a->attrib & AT_RW)
			*dp++ = inst->data + attribPos[i]*0x10;

	uint32 idx = 0;
	uint32 *p = (uint32*)inst->data;
	if(numBrokenAttribs == 0){
		*p++ = 0x60000000 | size-1;
		*p++ = 0;
		*p++ = 0x11000000;	// FLUSH
		*p++ = 0x06000000;	// MSKPATH3; SA: FLUSH
	}
	for(uint32 j = 0; j < numBatches; j++){
		uint32 nverts, bsize;
		if(j < numBatches-1){
			bsize = batchSize;
			nverts = batchVertCount;
		}else{
			bsize = lastBatchSize;
			nverts = lastBatchVertCount;
		}
		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && a->attrib & AT_RW){
				uint32 atsz = attribSize(a->attrib);
				*p++ = 0x30000000 | QWC(nverts*atsz);
				*p++ = attribPos[i];
				*p++ = 0x01000100 |
					this->inputStride;	// STCYCL
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | nverts << 16 | i; // UNPACK

				*p++ = 0x10000000;
				*p++ = 0x0;
				*p++ = 0x0;
				*p++ = 0x0;

				attribPos[i] += g->meshHeader->flags == 1 ?
					QWC((batchVertCount-2)*atsz) :
					QWC(batchVertCount*atsz);
			}
		if(numBrokenAttribs){
			*p++ = (j < numBatches-1 ? 0x10000000 : 0x60000000) |
			       bsize;
			*p++ = 0x0;
			*p++ = 0x0;
			*p++ = 0x0;
		}

		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && (a->attrib & AT_RW) == 0){
				*p++ = 0x07000000 | markcnt++; // MARK (SA: NOP)
				*p++ = 0x05000000;		// STMOD
				*p++ = 0x01000100 |
					this->inputStride;	// STCYCL
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | nverts << 16 | i; // UNPACK

				switch(i){
				case 0:
					p = instanceXYZ(p, g, m, idx, nverts);
					break;
				case 1:
					p = instanceUV(p, g, m, idx, nverts);
					break;
				case 2:
					p = instanceRGBA(p, g, m, idx, nverts);
					break;
				case 3:
					p = instanceNormal(p,g, m, idx, nverts);
					break;
				}
			}
		idx += g->meshHeader->flags == 1
			? batchVertCount-2 : batchVertCount;

		*p++ = 0x04000000 | nverts;	// ITOP
		*p++ = j == 0 ? 0x15000000 : 0x17000000;
		if(j < numBatches-1){
			*p++ = 0x0;
			*p++ = 0x0;
		}else{
			*p++ = 0x11000000;	// FLUSH
			*p++ = 0x06000000;	// MSKPATH3; SA: FLUSH
		}
	}

	if(instanceCB)
		instanceCB(this, g, m, datap, numBrokenAttribs);
}


ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform), groupPipeline(NULL) { }

void
ObjPipeline::instance(Atomic *atomic)
{
	Geometry *geometry = atomic->geometry;
	if(geometry->geoflags & Geometry::NATIVE)
		return;
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_PS2;
	assert(geometry->meshHeader != NULL);
	header->numMeshes = geometry->meshHeader->numMeshes;
	header->instanceMeshes = new InstanceData[header->numMeshes];
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geometry->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];

		MatPipeline *m;
		m = this->groupPipeline ?
			this->groupPipeline :
			(MatPipeline*)mesh->material->pipeline;
		if(m == NULL)
			m = defaultMatPipe;
		m->instance(geometry, instance, mesh);
	}
	geometry->geoflags |= Geometry::NATIVE;
}

void
printVertCounts(InstanceData *inst, int flag)
{
	uint32 *d = (uint32*)inst->data;
	int stride;
	if(inst->arePointersFixed){
		d += 4;
		while(d[3]&0x60000000){	// skip UNPACKs
			stride = d[2]&0xFF;
			d += 4 + 4*QWC(attribSize(d[3])*((d[3]>>16)&0xFF));
		}
		if(d[2] == 0)
			printf("ITOP %x %d (%d)\n", *d, stride, flag);
	}else{
		while((*d&0x70000000) == 0x30000000){
			stride = d[2]&0xFF;
			d += 8;
		}
		if((*d&0x70000000) == 0x10000000){
			d += (*d&0xFFFF)*4;
			printf("ITOP %x %d (%d)\n", *d, stride, flag);
		}
	}
}

// Only a dummy right now
void
ObjPipeline::uninstance(Atomic *atomic)
{
	Geometry *geometry = atomic->geometry;
	assert(geometry->instData != NULL);
	assert(geometry->instData->platform == PLATFORM_PS2);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geometry->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];
//		printf("numIndices: %d\n", mesh->numIndices);
//		printDMA(instance);
		printVertCounts(instance, geometry->meshHeader->flags);
	}
}

#undef QWC

ObjPipeline*
makeDefaultPipeline(void)
{
	if(defaultMatPipe == NULL){
		MatPipeline *pipe = new MatPipeline(PLATFORM_PS2);
		pipe->attribs[AT_XYZ] = &attribXYZ;
		pipe->attribs[AT_UV] = &attribUV;
		pipe->attribs[AT_RGBA] = &attribRGBA;
		pipe->attribs[AT_NORMAL] = &attribNormal;
		uint32 vertCount = MatPipeline::getVertCount(VU_Lights,4,3,2);
		pipe->setTriBufferSizes(4, vertCount);
		pipe->vifOffset = pipe->inputStride*vertCount;
		defaultMatPipe = pipe;
	}

	if(defaultObjPipe == NULL){
		ObjPipeline *opipe = new ObjPipeline(PLATFORM_PS2);
		defaultObjPipe = opipe;
	}
	return defaultObjPipe;
}

static void skinInstanceCB(MatPipeline*, Geometry*, Mesh*, uint8**, int32);

ObjPipeline*
makeSkinPipeline(void)
{
	MatPipeline *pipe = new MatPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	pipe->attribs[AT_XYZ] = &attribXYZ;
	pipe->attribs[AT_UV] = &attribUV;
	pipe->attribs[AT_RGBA] = &attribRGBA;
	pipe->attribs[AT_NORMAL] = &attribNormal;
	pipe->attribs[AT_NORMAL+1] = &attribWeights;
	uint32 vertCount = MatPipeline::getVertCount(VU_Lights-0x100, 5, 3, 2);
	pipe->setTriBufferSizes(5, vertCount);
	pipe->vifOffset = pipe->inputStride*vertCount;
	pipe->instanceCB = skinInstanceCB;

	ObjPipeline *opipe = new ObjPipeline(PLATFORM_PS2);
	opipe->pluginID = ID_SKIN;
	opipe->pluginData = 1;
	opipe->groupPipeline = pipe;
	return opipe;
}

ObjPipeline*
makeMatFXPipeline(void)
{
	MatPipeline *pipe = new MatPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	pipe->attribs[AT_XYZ] = &attribXYZ;
	pipe->attribs[AT_UV] = &attribUV;
	pipe->attribs[AT_RGBA] = &attribRGBA;
	pipe->attribs[AT_NORMAL] = &attribNormal;
	uint32 vertCount = MatPipeline::getVertCount(0x3C5, 4, 3, 3);
	pipe->setTriBufferSizes(4, vertCount);
	pipe->vifOffset = pipe->inputStride*vertCount;

	ObjPipeline *opipe = new ObjPipeline(PLATFORM_PS2);
	opipe->pluginID = ID_MATFX;
	opipe->pluginData = 0;
	opipe->groupPipeline = pipe;
	return opipe;
}

// Skin

void
readNativeSkin(Stream *stream, int32, void *object, int32 offset)
{
	uint8 header[4];
	uint32 vers;
	Geometry *geometry = (Geometry*)object;
	assert(findChunk(stream, ID_STRUCT, NULL, &vers));
	assert(stream->readU32() == PLATFORM_PS2);
	stream->read(header, 4);
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;
	skin->numBones = header[0];

	// both values unused in/before 33002, used in/after 34003
	skin->numUsedBones = header[1];
	skin->maxIndex = header[2];

	bool oldFormat = skin->numUsedBones == 0;
	int32 size = skin->numUsedBones + skin->numBones*64 + 15;
	uint8 *data = new uint8[size];
	skin->data = data;
	skin->indices = NULL;
	skin->weights = NULL;

	skin->usedBones = NULL;
	if(skin->numUsedBones){
		skin->usedBones = data;
		data += skin->numUsedBones;
		stream->read(skin->data, skin->numUsedBones);
	}

	uintptr ptr = (uintptr)data + 15;
	ptr &= ~0xF;
	data = (uint8*)ptr;
	skin->inverseMatrices = NULL;
	if(skin->numBones){
		skin->inverseMatrices = (float*)data;
		stream->read(skin->inverseMatrices, skin->numBones*64);
	}

	if(!oldFormat)
		// last 3 ints are probably the same as in generic format
		// TODO: what are the other 4?
		stream->seek(7*4);
}

void
writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset)
{
	uint8 header[4];

	writeChunkHeader(stream, ID_STRUCT, len-12);
	stream->writeU32(PLATFORM_PS2);
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	bool oldFormat = version < 0x34003;
	header[0] = skin->numBones;
	header[1] = skin->numUsedBones;
	header[2] = skin->maxIndex;
	header[3] = 0;
	if(oldFormat){
		header[1] = 0;
		header[2] = 0;
	}
	stream->write(header, 4);

	if(!oldFormat)
		stream->write(skin->usedBones, skin->numUsedBones);
	stream->write(skin->inverseMatrices, skin->numBones*64);
	if(!oldFormat){
		uint32 buffer[7] = { 0, 0, 0, 0, 0, 0, 0 };
		stream->write(buffer, 7*4);
	}
}

int32
getSizeNativeSkin(void *object, int32 offset)
{
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == NULL)
		return -1;
	int32 size = 12 + 4 + 4 + skin->numBones*64;
	// not sure which version introduced the new format
	if(version >= 0x34003)
		size += skin->numUsedBones + 16 + 12;
	return size;
}

static void
skinInstanceCB(MatPipeline *, Geometry *g, Mesh *m, uint8 **data, int32 n)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(skin == NULL || n < 1)
		return;
	float *weights = (float*)data[0];
	uint32 *indices = (uint32*)data[0];
	uint16 j;
	for(uint32 i = 0; i < m->numIndices; i++){
		j = m->indices[i];
		*weights++ = skin->weights[j*4+0];
		*indices &= ~0x3FF;
		*indices++ |= skin->indices[j*4+0] && skin->weights[j*4+0] ?
				(skin->indices[j*4+0]+1) << 2 : 0;
		*weights++ = skin->weights[j*4+1];
		*indices &= ~0x3FF;
		*indices++ |= skin->indices[j*4+1] && skin->weights[j*4+1] ?
				(skin->indices[j*4+1]+1) << 2 : 0;
		*weights++ = skin->weights[j*4+2];
		*indices &= ~0x3FF;
		*indices++ |= skin->indices[j*4+2] && skin->weights[j*4+2] ?
				(skin->indices[j*4+2]+1) << 2 : 0;
		*weights++ = skin->weights[j*4+3];
		*indices &= ~0x3FF;
		*indices++ |= skin->indices[j*4+3] && skin->weights[j*4+3] ?
				(skin->indices[j*4+3]+1) << 2 : 0;
	}
}


// ADC

// TODO: look at PC SA rccam.dff bloodrb.dff, Xbox csbigbear.dff

static void*
createADC(void *object, int32 offset, int32)
{
	ADCData *adc = PLUGINOFFSET(ADCData, object, offset);
	adc->adcFormatted = 0;
	return object;
}

static void*
copyADC(void *dst, void *src, int32 offset, int32)
{
	ADCData *dstadc = PLUGINOFFSET(ADCData, dst, offset);
	ADCData *srcadc = PLUGINOFFSET(ADCData, src, offset);
	dstadc->adcFormatted = srcadc->adcFormatted;
	if(!dstadc->adcFormatted)
		return dst;
	dstadc->numBits = srcadc->numBits;
	int32 size = dstadc->numBits+3 & ~3;
	dstadc->adcBits = new int8[size];
	memcpy(dstadc->adcBits, srcadc->adcBits, size);
	return dst;
}

static void*
destroyADC(void *object, int32 offset, int32)
{
	ADCData *adc = PLUGINOFFSET(ADCData, object, offset);
	if(adc->adcFormatted)
		delete[] adc->adcBits;
	return object;
}

static void
readADC(Stream *stream, int32, void *object, int32 offset, int32)
{
	ADCData *adc = PLUGINOFFSET(ADCData, object, offset);
	assert(findChunk(stream, ID_ADC, NULL, NULL));
	adc->numBits = stream->readI32();
	if(adc->numBits == 0){
		adc->adcFormatted = 0;
		return;
	}
	adc->adcFormatted = 1;
	int32 size = adc->numBits+3 & ~3;
	adc->adcBits = new int8[size];
	stream->read(adc->adcBits, size);

	Geometry *geometry = (Geometry*)object;
	int ones = 0, zeroes = 0;
	for(int i = 0; i < adc->numBits; i++)
		if(adc->adcBits[i] == 0)
			zeroes++;
		else if(adc->adcBits[i] == 1)
			ones++;
		else
			fprintf(stderr, "what the fuck man\n");
	printf("%X %X %X\n", adc->numBits, zeroes, ones);
	MeshHeader *meshHeader = geometry->meshHeader;
	printf("%X\n", meshHeader->totalIndices);
}

static void
writeADC(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	ADCData *adc = PLUGINOFFSET(ADCData, object, offset);
	Geometry *geometry = (Geometry*)object;
	writeChunkHeader(stream, ID_ADC, len-12);
	if(geometry->geoflags & Geometry::NATIVE){
		stream->writeI32(0);
		return;
	}
	stream->writeI32(adc->numBits);
	int32 size = adc->numBits+3 & ~3;
	stream->write(adc->adcBits, size);
}

static int32
getSizeADC(void *object, int32 offset, int32)
{
	Geometry *geometry = (Geometry*)object;
	ADCData *adc = PLUGINOFFSET(ADCData, object, offset);
	if(!adc->adcFormatted)
		return -1;
	if(geometry->geoflags & Geometry::NATIVE)
		return 16;
	return 16 + (adc->numBits+3 & ~3);
}

void
registerADCPlugin(void)
{
	Geometry::registerPlugin(sizeof(ADCData), ID_ADC,
	                         createADC, destroyADC, copyADC);
	Geometry::registerPluginStream(ID_ADC,
	                               readADC,
	                               writeADC,
	                               getSizeADC);
}


// PDS plugin

static void
atomicPDSRights(void *object, int32, int32, uint32 data)
{
	Atomic *a = (Atomic*)object;
	// TODO: lookup pipeline by data
	a->pipeline = new ObjPipeline(PLATFORM_PS2);
	a->pipeline->pluginID = ID_PDS;
	a->pipeline->pluginData = data;
}

static void
materialPDSRights(void *object, int32, int32, uint32 data)
{
	Material *m = (Material*)object;
	// TODO: lookup pipeline by data
	m->pipeline = new Pipeline(PLATFORM_PS2);
	m->pipeline->pluginID = ID_PDS;
	m->pipeline->pluginData = data;
}

void
registerPDSPlugin(void)
{
	Atomic::registerPlugin(0, ID_PDS, NULL, NULL, NULL);
	Atomic::setStreamRightsCallback(ID_PDS, atomicPDSRights);

	Material::registerPlugin(0, ID_PDS, NULL, NULL, NULL);
	Material::setStreamRightsCallback(ID_PDS, materialPDSRights);
}


// misc stuff

void
printDMA(InstanceData *inst)
{
	uint32 *tag = (uint32*)inst->data;
	for(;;){
		switch(tag[0]&0x70000000){
		// DMAcnt
		case 0x10000000:
			printf("%08x %08x\n", tag[0], tag[1]);
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		// DMAref
		case 0x30000000:
			printf("%08x %08x\n", tag[0], tag[1]);
			tag += 4;
			break;

		// DMAret
		case 0x60000000:
			printf("%08x %08x\n", tag[0], tag[1]);
			return;
		}
	}
}

/* Function to specifically walk geometry chains */
void
walkDMA(InstanceData *inst, void (*f)(uint32 *data, int32 size))
{
	if(inst->arePointersFixed == 2)
		return;
	uint32 *base = (uint32*)inst->data;
	uint32 *tag = (uint32*)inst->data;
	for(;;){
		switch(tag[0]&0x70000000){
		// DMAcnt
		case 0x10000000:
			f(tag+2, 2+(tag[0]&0xFFFF)*4);
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		// DMAref
		case 0x30000000:
			f(base + tag[1]*4, (tag[0]&0xFFFF)*4);
			tag += 4;
			break;

		// DMAret
		case 0x60000000:
			f(tag+2, 2+(tag[0]&0xFFFF)*4);
			return;
		}
	}
}

void
sizedebug(InstanceData *inst)
{
	if(inst->arePointersFixed == 2)
		return;
	uint32 *base = (uint32*)inst->data;
	uint32 *tag = (uint32*)inst->data;
	uint32 *last = NULL;
	for(;;){
		switch(tag[0]&0x70000000){
		// DMAcnt
		case 0x10000000:
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		// DMAref
		case 0x30000000:
			last = base + tag[1]*4 + (tag[0]&0xFFFF)*4;
			tag += 4;
			break;

		// DMAret
		case 0x60000000:
			tag += (1+(tag[0]&0xFFFF))*4;
			uint32 diff;
			if(!last)
				diff = (uint8*)tag - (uint8*)base;
			else
				diff = (uint8*)last - (uint8*)base;
			printf("%x %x %x\n", inst->dataSize-diff, diff, inst->dataSize);
			return;

		default:
			printf("unkown DMAtag: %X %X\n", tag[0], tag[1]);
			break;
		}
	}
}

}
}
