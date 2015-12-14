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
 : rw::Pipeline(platform), instanceCB(NULL), uninstanceCB(NULL),
   allocateCB(NULL), finishCB(NULL)
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

struct InstMeshInfo
{
	uint32 numAttribs, numBrokenAttribs;
	uint32 batchVertCount, lastBatchVertCount;
	uint32 numBatches;
	uint32 batchSize, lastBatchSize;
	uint32 size, size2, stride;
	uint32 attribPos[10];
};

InstMeshInfo
getInstMeshInfo(MatPipeline *pipe, Geometry *g, Mesh *m)
{
	PipeAttribute *a;
	InstMeshInfo im;
	im.numAttribs = 0;
	im.numBrokenAttribs = 0;
	im.stride = 0;
	for(uint i = 0; i < nelem(pipe->attribs); i++)
		if(a = pipe->attribs[i])
			if(a->attrib & AT_RW)
				im.numBrokenAttribs++;
			else{
				im.stride += attribSize(a->attrib);
				im.numAttribs++;
			}
	uint32 totalVerts = 0;
	if(g->meshHeader->flags == 1){	// tristrip
		im.numBatches = 0;
		for(uint i = 0; i < m->numIndices; i += pipe->triStripCount-2){
			im.numBatches++;
			totalVerts += m->numIndices-i < pipe->triStripCount ?
			              m->numIndices-i : pipe->triStripCount;
		}
		im.batchVertCount = pipe->triStripCount;
		im.lastBatchVertCount = totalVerts % pipe->triStripCount;
	}else{				// trilist
		im.numBatches = (m->numIndices+pipe->triListCount-1) /
		                 pipe->triListCount;
		im.batchVertCount = pipe->triListCount;
		im.lastBatchVertCount = m->numIndices % pipe->triListCount;
	}

	im.batchSize = getBatchSize(pipe, im.batchVertCount);
	im.lastBatchSize = getBatchSize(pipe, im.lastBatchVertCount);
	im.size = 0;
	if(im.numBrokenAttribs == 0)
		im.size = 1 + im.batchSize*(im.numBatches-1) + im.lastBatchSize;
	else
		im.size = 2*im.numBatches +
		          (1+im.batchSize)*(im.numBatches-1) + 1+im.lastBatchSize;

	/* figure out size and addresses of broken out sections */
	im.size2 = 0;
	for(uint i = 0; i < nelem(im.attribPos); i++)
		if((a = pipe->attribs[i]) && a->attrib & AT_RW){
			im.attribPos[i] = im.size2 + im.size;
			im.size2 += QWC(m->numIndices*attribSize(a->attrib));
		}

	return im;
}

void
MatPipeline::instance(Geometry *g, InstanceData *inst, Mesh *m)
{
	PipeAttribute *a;
	InstMeshInfo im = getInstMeshInfo(this, g, m);

	inst->dataSize = (im.size+im.size2)<<4;
	inst->arePointersFixed = im.numBrokenAttribs == 0;
	// TODO: force alignment
	inst->data = new uint8[inst->dataSize];

	/* make array of addresses of broken out sections */
	uint8 *datap[nelem(this->attribs)];
	uint8 **dp = datap;
	for(uint i = 0; i < nelem(this->attribs); i++)
		if((a = this->attribs[i]) && a->attrib & AT_RW)
			*dp++ = inst->data + im.attribPos[i]*0x10;

	uint32 idx = 0;
	uint32 *p = (uint32*)inst->data;
	if(im.numBrokenAttribs == 0){
		*p++ = 0x60000000 | im.size-1;
		*p++ = 0;
		*p++ = 0x11000000;	// FLUSH
		*p++ = 0x06000000;	// MSKPATH3; SA: FLUSH
	}
	for(uint32 j = 0; j < im.numBatches; j++){
		uint32 nverts, bsize;
		if(j < im.numBatches-1){
			bsize = im.batchSize;
			nverts = im.batchVertCount;
		}else{
			bsize = im.lastBatchSize;
			nverts = im.lastBatchVertCount;
		}
		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && a->attrib & AT_RW){
				uint32 atsz = attribSize(a->attrib);
				*p++ = 0x30000000 | QWC(nverts*atsz);
				*p++ = im.attribPos[i];
				*p++ = 0x01000100 |
					this->inputStride;	// STCYCL
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | nverts << 16 | i; // UNPACK

				*p++ = 0x10000000;
				*p++ = 0x0;
				*p++ = 0x0;
				*p++ = 0x0;

				im.attribPos[i] += g->meshHeader->flags == 1 ?
					QWC((im.batchVertCount-2)*atsz) :
					QWC(im.batchVertCount*atsz);
			}
		if(im.numBrokenAttribs){
			*p++ = (j < im.numBatches-1 ? 0x10000000 : 0x60000000) |
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
			? im.batchVertCount-2 : im.batchVertCount;

		*p++ = 0x04000000 | nverts;	// ITOP
		*p++ = j == 0 ? 0x15000000 : 0x17000000;
		if(j < im.numBatches-1){
			*p++ = 0x0;
			*p++ = 0x0;
		}else{
			*p++ = 0x11000000;	// FLUSH
			*p++ = 0x06000000;	// MSKPATH3; SA: FLUSH
		}
	}

	if(instanceCB)
		instanceCB(this, g, m, datap, im.numBrokenAttribs);
}

uint8*
MatPipeline::collectData(Geometry *g, InstanceData *inst, Mesh *m, uint8 *data[])
{
	PipeAttribute *a;
	InstMeshInfo im = getInstMeshInfo(this, g, m);

	uint8 *raw = im.stride*m->numIndices ? new uint8[im.stride*m->numIndices] : NULL;
	uint8 *dp = raw;
	for(uint i = 0; i < nelem(this->attribs); i++)
		if(a = this->attribs[i])
			if(a->attrib & AT_RW){
				data[i] = inst->data + im.attribPos[i]*0x10;
			}else{
				data[i] = dp;
				dp += m->numIndices*attribSize(a->attrib);
			}

	uint8 *datap[nelem(this->attribs)];
	for(uint i = 0; i < nelem(this->attribs); i++){
		datap[i] = data[i];
		//printf("%p %x, %x\n", datap[i], datap[i]-datap[0], im.attribPos[i]*0x10);
	}

	uint32 overlap = g->meshHeader->flags == 1 ? 2 : 0;
	uint32 *p = (uint32*)inst->data;
	if(im.numBrokenAttribs == 0)
		p += 4;
	for(uint32 j = 0; j < im.numBatches; j++){
		uint32 nverts = j < im.numBatches-1 ? im.batchVertCount :
		                                      im.lastBatchVertCount;
		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && a->attrib & AT_RW)
				p += 8;
		if(im.numBrokenAttribs)
			p += 4;
		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && (a->attrib & AT_RW) == 0){
				uint32 asz = attribSize(a->attrib);
				p += 4;
				if((p[-1] & 0xff004000) != a->attrib)
					printf("unexpected unpack: %08x %08x\n", p[-1], a->attrib);
				memcpy(datap[i], p, asz*nverts);
				datap[i] += asz*(nverts-overlap);
				p += QWC(asz*nverts)*4;
			}
		p += 4;
	}
	return raw;
}

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform), groupPipeline(NULL) { }

void
ObjPipeline::instance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo->geoflags & Geometry::NATIVE)
		return;
	InstanceDataHeader *header = new InstanceDataHeader;
	geo->instData = header;
	header->platform = PLATFORM_PS2;
	assert(geo->meshHeader != NULL);
	header->numMeshes = geo->meshHeader->numMeshes;
	header->instanceMeshes = new InstanceData[header->numMeshes];
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];

		MatPipeline *m;
		m = this->groupPipeline ?
		    this->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL)
			m = defaultMatPipe;
		m->instance(geo, instance, mesh);
	}
	geo->geoflags |= Geometry::NATIVE;
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

void
ObjPipeline::uninstance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		return;
	assert(geo->instData != NULL);
	assert(geo->instData->platform == PLATFORM_PS2);
	// highest possible number of vertices
	geo->numVertices = geo->meshHeader->totalIndices;
	geo->geoflags &= ~Geometry::NATIVE;
	geo->allocateData();
	geo->meshHeader->allocateIndices();
	uint32 *flags = new uint32[geo->numVertices];
	memset(flags, 0, 4*geo->numVertices);
	memset(geo->meshHeader->mesh[0].indices, 0, 2*geo->meshHeader->totalIndices);
	geo->numVertices = 0;
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];

		MatPipeline *m;
		m = this->groupPipeline ?
		    this->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;
		if(m->allocateCB) m->allocateCB(m, geo);

		uint8 *data[nelem(m->attribs)] = { NULL };
		uint8 *raw = m->collectData(geo, instance, mesh, data);
		assert(m->uninstanceCB);
		m->uninstanceCB(m, geo, flags, mesh, data);
		if(raw) delete[] raw;
	}
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		MatPipeline *m;
		m = this->groupPipeline ?
		    this->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;
		if(m->finishCB) m->finishCB(m, geo);
	}

	geo->generateTriangles();
	delete[] flags;
	destroyNativeData(geo, 0, 0);
	geo->instData = NULL;

/*	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geometry->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];
//		printf("numIndices: %d\n", mesh->numIndices);
//		printDMA(instance);
		printVertCounts(instance, geometry->meshHeader->flags);
	}*/
}

int32
findVertex(Geometry *g, uint32 flags[], uint32 mask, int32 first,
           float *v, float *t0, float *t1, uint8 *c, float *n)
{
	float32 *verts = &g->morphTargets[0].vertices[first*3];
	float32 *tex0 = &g->texCoords[0][first*2];
	float32 *tex1 = &g->texCoords[1][first*2];
	float32 *norms = &g->morphTargets[0].normals[first*3];
	uint8 *cols = &g->colors[first*4];

	for(int32 i = first; i < g->numVertices; i++){
		if(mask & flags[i] & 0x1 &&
		   !(verts[0] == v[0] && verts[1] == v[1] && verts[2] == v[2]))
			goto cont;
		if(mask & flags[i] & 0x10 &&
		   !(norms[0] == n[0] && norms[1] == n[1] && norms[2] == n[2]))
			goto cont;
		if(mask & flags[i] & 0x100 &&
		   !(cols[0] == c[0] && cols[1] == c[1] && cols[2] == c[2] && cols[3] == c[3]))
			goto cont;
		if(mask & flags[i] & 0x1000 &&
		   !(tex0[0] == t0[0] && tex0[1] == t0[1]))
			goto cont;
		if(mask & flags[i] & 0x2000 &&
		   !(tex1[0] == t1[0] && tex1[1] == t1[1]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex0 += 2;
		tex1 += 2;
		norms += 3;
		cols += 4;
	}
	return -1;
}

void
insertVertex(Geometry *geo, int32 i, uint32 mask, float *v, float *t0, float *t1, uint8 *c, float *n)
{
	if(mask & 0x1)
		memcpy(&geo->morphTargets[0].vertices[i*3], v, 12);
	if(mask & 0x10)
		memcpy(&geo->morphTargets[0].normals[i*3], n, 12);
	if(mask & 0x100)
		memcpy(&geo->colors[i*4], c, 4);
	if(mask & 0x1000)
		memcpy(&geo->texCoords[0][i*2], t0, 8);
	if(mask & 0x2000)
		memcpy(&geo->texCoords[1][i*2], t1, 8);
}

void
defaultUninstanceCB(MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
{
	float32 *verts     = (float32*)data[AT_XYZ];
	float32 *texcoords = (float32*)data[AT_UV];
	uint8 *colors      = (uint8*)data[AT_RGBA];
	int8 *norms        = (int8*)data[AT_NORMAL];
	uint32 mask = 0x1;	// vertices
	if(geo->geoflags & Geometry::NORMALS)
		mask |= 0x10;
	if(geo->geoflags & Geometry::PRELIT)
		mask |= 0x100;
	if(geo->numTexCoordSets > 0)
		mask |= 0x1000;

	float32 n[3];
	for(uint32 i = 0; i < mesh->numIndices; i++){
		if(mask & 0x10){
			n[0] = norms[0]/127.0f;
			n[1] = norms[1]/127.0f;
			n[2] = norms[2]/127.0f;
		}
		int32 idx = findVertex(geo, flags, mask, 0, verts, texcoords, NULL, colors, n);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		flags[idx] = mask;
		insertVertex(geo, idx, mask, verts, texcoords, NULL, colors, n);
		verts += 3;
		texcoords += 2;
		colors += 4;
		norms += 3;
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
		pipe->uninstanceCB = defaultUninstanceCB;
		defaultMatPipe = pipe;
	}

	if(defaultObjPipe == NULL){
		ObjPipeline *opipe = new ObjPipeline(PLATFORM_PS2);
		defaultObjPipe = opipe;
	}
	return defaultObjPipe;
}

static void skinInstanceCB(MatPipeline*, Geometry*, Mesh*, uint8**, int32);
static void skinUninstanceCB(MatPipeline*, Geometry*, uint32*, Mesh*, uint8**);
static void skinAllocateCB(MatPipeline*, Geometry*);
static void skinFinishCB(MatPipeline*, Geometry*);

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
	pipe->uninstanceCB = skinUninstanceCB;
	pipe->allocateCB = skinAllocateCB;
	pipe->finishCB = skinFinishCB;

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
	pipe->uninstanceCB = defaultUninstanceCB;

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

	// numUsedBones and numWeights appear in/after 34003 but not in/before 33002
	// (probably rw::version >= 0x34000)
	bool oldFormat = header[1] == 0;

	// Use numBones for numUsedBones to allocate data
	if(oldFormat)
		skin->init(header[0], header[0], 0);
	else
		skin->init(header[0], header[1], 0);
	skin->numWeights = header[2];

	if(!oldFormat)
		stream->read(skin->usedBones, skin->numUsedBones);
	if(skin->numBones)
		stream->read(skin->inverseMatrices, skin->numBones*64);

	// dummy data in case we need to write data in the new format
	if(oldFormat){
		skin->numWeights = 4;
		for(int32 i = 0; i < skin->numUsedBones; i++)
			skin->usedBones[i] = i;
	}

	if(!oldFormat)
		// last 3 ints are split data as in the other formats
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
	// not sure which version introduced the new format
	bool oldFormat = version < 0x34000;
	header[0] = skin->numBones;
	if(oldFormat){
		header[1] = 0;
		header[2] = 0;
	}else{
		header[1] = skin->numUsedBones;
		header[2] = skin->numWeights;
	}
	header[3] = 0;
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
	if(version >= 0x34000)
		size += skin->numUsedBones + 16 + 12;
	return size;
}

static void
skinInstanceCB(MatPipeline *, Geometry *g, Mesh *m, uint8 **data, int32 n)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(skin == NULL || n < 1)
		return;
	float32 *weights = (float32*)data[0];
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

// TODO: call base function perhaps?
int32
findVertexSkin(Geometry *g, uint32 flags[], uint32 mask, int32 first,
               float32 *v, float32 *t0, float32 *t1, uint8 *c, float32 *n,
               float32 *w, uint8 *ix)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	float32 *wghts = &skin->weights[first*4];
	uint8 *inds = &skin->indices[first*4];

	float32 *verts = &g->morphTargets[0].vertices[first*3];
	float32 *tex0 = &g->texCoords[0][first*2];
	float32 *tex1 = &g->texCoords[1][first*2];
	float32 *norms = &g->morphTargets[0].normals[first*3];
	uint8 *cols = &g->colors[first*4];

	for(int32 i = first; i < g->numVertices; i++){
		if(mask & flags[i] & 0x1 &&
		   !(verts[0] == v[0] && verts[1] == v[1] && verts[2] == v[2]))
			goto cont;
		if(mask & flags[i] & 0x10 &&
		   !(norms[0] == n[0] && norms[1] == n[1] && norms[2] == n[2]))
			goto cont;
		if(mask & flags[i] & 0x100 &&
		   !(cols[0] == c[0] && cols[1] == c[1] && cols[2] == c[2] && cols[3] == c[3]))
			goto cont;
		if(mask & flags[i] & 0x1000 &&
		   !(tex0[0] == t0[0] && tex0[1] == t0[1]))
			goto cont;
		if(mask & flags[i] & 0x2000 &&
		   !(tex1[0] == t1[0] && tex1[1] == t1[1]))
			goto cont;
		if(mask & flags[i] & 0x10000 &&
		   !(wghts[0] == w[0] && wghts[1] == w[1] &&
		     wghts[2] == w[2] && wghts[3] == w[3] &&
		     inds[0] == ix[0] && inds[1] == ix[1] &&
		     inds[2] == ix[2] && inds[3] == ix[3]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex0 += 2;
		tex1 += 2;
		norms += 3;
		cols += 4;
		wghts += 4;
		inds += 4;
	}
	return -1;
}

void
insertVertexSkin(Geometry *geo, int32 i, uint32 mask, float32 *v, float32 *t0, float32 *t1, uint8 *c, float32 *n,
	float32 *w, uint8 *ix)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	insertVertex(geo, i, mask, v, t0, t1, c, n);
	if(mask & 0x10000){
		memcpy(&skin->weights[i*4], w, 16);
		memcpy(&skin->indices[i*4], ix, 4);
	}
}

static void
skinUninstanceCB(MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
{
	float32 *verts     = (float32*)data[AT_XYZ];
	float32 *texcoords = (float32*)data[AT_UV];
	uint8 *colors      = (uint8*)data[AT_RGBA];
	int8 *norms        = (int8*)data[AT_NORMAL];
	uint32 *wghts      = (uint32*)data[AT_NORMAL+1];
	uint32 mask = 0x1;	// vertices
	if(geo->geoflags & Geometry::NORMALS)
		mask |= 0x10;
	if(geo->geoflags & Geometry::PRELIT)
		mask |= 0x100;
	if(geo->numTexCoordSets > 0)
		mask |= 0x1000;
	mask |= 0x10000;

	float32 n[3];
	float32 w[4];
	uint8 ix[4];
	for(uint32 i = 0; i < mesh->numIndices; i++){
		if(mask & 0x10){
			n[0] = norms[0]/127.0f;
			n[1] = norms[1]/127.0f;
			n[2] = norms[2]/127.0f;
		}
		for(int j = 0; j < 4; j++){
			((uint32*)w)[j] = wghts[j] & ~0x3FF;
			ix[j] = (wghts[j] & 0x3FF) >> 2;
			if(ix[j]) ix[j]--;
			if(w[j] == 0.0f) ix[j] = 0;
		}
		int32 idx = findVertexSkin(geo, flags, mask, 0, verts, texcoords, NULL, colors, n, w, ix);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		flags[idx] = mask;
		insertVertexSkin(geo, idx, mask, verts, texcoords, NULL, colors, n, w, ix);
		verts += 3;
		texcoords += 2;
		colors += 4;
		norms += 3;
		wghts += 4;
	}
}

static void
skinAllocateCB(MatPipeline*, Geometry *geo)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	// If weight/index data is allocated don't do it again as this function
	// can be called multiple times per geometry.
	if(skin == NULL || skin->weights)
		return;

	uint8 *data = skin->data;
	float *invMats = skin->inverseMatrices;
	// meshHeader->totalIndices is highest possible number of vertices again
	skin->init(skin->numBones, skin->numBones, geo->meshHeader->totalIndices);
	memcpy(skin->inverseMatrices, invMats, skin->numBones*64);
	delete[] data;
}

static void
skinFinishCB(MatPipeline*, Geometry *geo)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	skin->findNumWeights(geo->numVertices);
	skin->findUsedBones(geo->numVertices);
}

// ADC

// TODO: look at PC SA rccam.dff bloodrb.dff, Xbox csbigbear.dff

static void
rotateface(int f[])
{
	int tmp;
	while(f[0] > f[1] || f[0] > f[2]){
		tmp = f[0];
		f[0] = f[1];
		f[1] = f[2];
		f[2] = tmp;
	}

/*
	if(f[0] > f[1]){
		tmp = f[0];
		f[0] = f[1];
		f[1] = tmp;
	}
	if(f[0] > f[2]){
		tmp = f[0];
		f[0] = f[2];
		f[2] = tmp;
	}
	if(f[1] > f[2]){
		tmp = f[1];
		f[1] = f[2];
		f[2] = tmp;
	}
*/
}

static int
validFace(Geometry *g, uint16 *f, int j, int m)
{
	int f1[3], f2[3];
	f1[0] = f[j+0 + (j%2)];
	f1[1] = f[j+1 - (j%2)];
	f1[2] = f[j+2];
//printf("-> %d %d %d\n", f1[0], f1[1], f1[2]);
	rotateface(f1);
	uint16 *fs = g->triangles;
	for(int i = 0; i < g->numTriangles; i++, fs += 4){
		if(fs[2] != m)
			continue;
		f2[0] = fs[1];
		f2[1] = fs[0];
		f2[2] = fs[3];
//printf("<- %d %d %d\n", f2[0], f2[1], f2[2]);
		rotateface(f2);
		if(f1[0] == f2[0] &&
		   f1[1] == f2[1] &&
		   f1[2] == f2[2])
			return 1;
	}
	return 0;
}

static int
isdegenerate(uint16 *f)
{
	return f[0] == f[1] ||
	       f[0] == f[2] ||
	       f[1] == f[2];
}

static int
debugadc(Geometry *g, MeshHeader *mh, ADCData *adc)
{
	int n = 0;
	for(int i = 0; i < mh->numMeshes; i++){
		uint16 *idx = mh->mesh[i].indices;
		for(int j = 0; j < mh->mesh[i].numIndices-2; j++){
			if(!validFace(g, idx, j, i) && !isdegenerate(idx+j)){
				n++;
//				printf("> %d %d %d %d\n", i, idx[j+0], idx[j+1], idx[j+2]);
			}
		}
	}
	return n;
}

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

/*
	Geometry *geometry = (Geometry*)object;
	int ones = 0, zeroes = 0;
	for(int i = 0; i < adc->numBits; i++)
		if(adc->adcBits[i])
			ones++;
		else
			zeroes++;
	MeshHeader *meshHeader = geometry->meshHeader;
	int n = debugadc(geometry, meshHeader, adc);
	printf("%X %X | %X %X %X\n",
		meshHeader->totalIndices,
		meshHeader->totalIndices + n,
		adc->numBits, zeroes, ones);
*/
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

// Raster

int32 nativeRasterOffset;

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, object, offset);
	new (raster) Ps2Raster;
	raster->mipmap = 0xFC0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	return object;
}

static void*
copyNativeRaster(void *dst, void *src, int32 offset, int32)
{
	Ps2Raster *dstraster = PLUGINOFFSET(Ps2Raster, dst, offset);
	Ps2Raster *srcraster = PLUGINOFFSET(Ps2Raster, src, offset);
	dstraster->mipmap = srcraster->mipmap;
	return dst;
}

static void
readMipmap(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	int32 val = stream->readI32();
	Texture *tex = (Texture*)object;
	if(tex->raster == NULL)
		return;
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, nativeRasterOffset);
	raster->mipmap = val;
}

static void
writeMipmap(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	Texture *tex = (Texture*)object;
	assert(tex->raster);
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, nativeRasterOffset);
	stream->writeI32(raster->mipmap);
}

static int32
getSizeMipmap(void *object, int32 offset, int32)
{
	return rw::platform == PLATFORM_PS2 ? 4 : 0;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(Ps2Raster),
	                                            0x12340000 | PLATFORM_PS2, 
                                                    createNativeRaster,
                                                    destroyNativeRaster,
                                                    copyNativeRaster);
	Raster::nativeOffsets[PLATFORM_PS2] = nativeRasterOffset;
	Texture::registerPlugin(0, ID_SKYMIPMAP, NULL, NULL, NULL);
	Texture::registerPluginStream(ID_SKYMIPMAP, readMipmap, writeMipmap, getSizeMipmap);
}

}
}
