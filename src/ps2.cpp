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
		instance->material = geometry->meshHeader->mesh[i].material;
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
	uint32 size = 1;	// ITOP &c. at the end
	for(uint i = 0; i < nelem(pipe->attribs); i++)
		if((a = pipe->attribs[i]) && (a->attrib & AT_RW) == 0){
			size++;	// UNPACK &c.
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
   preUninstCB(NULL), postUninstCB(NULL)
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
		im.size = 2*im.numBrokenAttribs*im.numBatches +
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
			dp[i] = inst->data + im.attribPos[i]*0x10;

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
				// Round up nverts so UNPACK will fit exactly into the DMA packet
				//  (can't pad with zeroes in broken out sections).
				// TODO: check for clash with vifOffset somewhere
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | (QWC(nverts*atsz)<<4)/atsz << 16 | i; // UNPACK

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

	if(this->instanceCB)
		this->instanceCB(this, g, m, datap);
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
	memcpy(datap, data, sizeof(datap));

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
		instance->material = mesh->material;
	}
	geo->geoflags |= Geometry::NATIVE;
}

void
printVertCounts(InstanceData *inst, int flag)
{
	uint32 *d = (uint32*)inst->data;
	uint32 id = 0;
	if(inst->material->pipeline)
		id = inst->material->pipeline->pluginData;
	int stride;
	if(inst->arePointersFixed){
		d += 4;
		while(d[3]&0x60000000){	// skip UNPACKs
			stride = d[2]&0xFF;
			d += 4 + 4*QWC(attribSize(d[3])*((d[3]>>16)&0xFF));
		}
		if(d[2] == 0)
			printf("ITOP %x %d (%d) %x\n", *d, stride, flag, id);
	}else{
		while((*d&0x70000000) == 0x30000000){
			stride = d[2]&0xFF;
			printf("UNPACK %x %d (%d) %x\n", d[3], stride, flag, id);
			d += 8;
		}
		if((*d&0x70000000) == 0x10000000){
			d += (*d&0xFFFF)*4;
			printf("ITOP %x %d (%d) %x\n", *d, stride, flag, id);
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
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	// highest possible number of vertices
	geo->numVertices = geo->meshHeader->totalIndices;
	geo->geoflags &= ~Geometry::NATIVE;
	geo->allocateData();
	geo->meshHeader->allocateIndices();
	uint32 *flags = new uint32[geo->numVertices];
	memset(flags, 0, 4*geo->numVertices);
	memset(geo->meshHeader->mesh[0].indices, 0, 2*geo->meshHeader->totalIndices);
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		MatPipeline *m;
		m = this->groupPipeline ?
		    this->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;
		if(m->preUninstCB) m->preUninstCB(m, geo);
	}
	geo->numVertices = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];
		MatPipeline *m;
		m = this->groupPipeline ?
		    this->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;

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
		if(m->postUninstCB) m->postUninstCB(m, geo);
	}

	int8 *bits = NULL;
	if(adcOffset){
		ADCData *adc = PLUGINOFFSET(ADCData, geo, adcOffset);
		if(adc->adcFormatted)
			bits = adc->adcBits;
	}
	geo->generateTriangles(bits);
	delete[] flags;
	destroyNativeData(geo, 0, 0);
	geo->instData = NULL;
/*
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];
//		printf("numIndices: %d\n", mesh->numIndices);
//		printDMA(instance);
		printVertCounts(instance, geo->meshHeader->flags);
	}
*/
}

int32
findVertex(Geometry *g, uint32 flags[], uint32 mask, Vertex *v)
{
	float32 *verts = g->morphTargets[0].vertices;
	float32 *tex = g->texCoords[0];
	float32 *norms = g->morphTargets[0].normals;
	uint8 *cols = g->colors;

	for(int32 i = 0; i < g->numVertices; i++){
		if(mask & flags[i] & 0x1 &&
		   !(verts[0] == v->p[0] && verts[1] == v->p[1] && verts[2] == v->p[2]))
			goto cont;
		if(mask & flags[i] & 0x10 &&
		   !(norms[0] == v->n[0] && norms[1] == v->n[1] && norms[2] == v->n[2]))
			goto cont;
		if(mask & flags[i] & 0x100 &&
		   !(cols[0] == v->c[0] && cols[1] == v->c[1] && cols[2] == v->c[2] && cols[3] == v->c[3]))
			goto cont;
		if(mask & flags[i] & 0x1000 &&
		   !(tex[0] == v->t[0] && tex[1] == v->t[1]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex += 2;
		norms += 3;
		cols += 4;
	}
	return -1;
}

void
insertVertex(Geometry *geo, int32 i, uint32 mask, Vertex *v)
{
	if(mask & 0x1)
		memcpy(&geo->morphTargets[0].vertices[i*3], v->p, 12);
	if(mask & 0x10)
		memcpy(&geo->morphTargets[0].normals[i*3], v->n, 12);
	if(mask & 0x100)
		memcpy(&geo->colors[i*4], v->c, 4);
	if(mask & 0x1000)
		memcpy(&geo->texCoords[0][i*2], v->t, 8);
}

void
defaultUninstanceCB(MatPipeline*, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
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

	Vertex v;
	for(uint32 i = 0; i < mesh->numIndices; i++){
		if(mask & 0x1)
			memcpy(&v.p, verts, 12);
		if(mask & 0x10){
			v.n[0] = norms[0]/127.0f;
			v.n[1] = norms[1]/127.0f;
			v.n[2] = norms[2]/127.0f;
		}
		if(mask & 0x100)
			memcpy(&v.c, colors, 4);
		if(mask & 0x1000)
			memcpy(&v.t, texcoords, 8);

		int32 idx = findVertex(geo, flags, mask, &v);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		flags[idx] = mask;
		insertVertex(geo, idx, mask, &v);
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

static void skinInstanceCB(MatPipeline*, Geometry*, Mesh*, uint8**);
static void skinUninstanceCB(MatPipeline*, Geometry*, uint32*, Mesh*, uint8**);

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
	pipe->preUninstCB = skinPreCB;
	pipe->postUninstCB = skinPostCB;

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

void
instanceSkinData(Geometry*, Mesh *m, Skin *skin, uint32 *data)
{
	uint16 j;
	float32 *weights = (float32*)data;
	uint32 *indices = data;
	for(uint32 i = 0; i < m->numIndices; i++){
		j = m->indices[i];
		for(int32 k = 0; k < 4; k++){
			*weights++ = skin->weights[j*4+k];
			*indices &= ~0x3FF;
			*indices++ |= skin->indices[j*4+k] && skin->weights[j*4+k] ?
					(skin->indices[j*4+k]+1) << 2 : 0;
		}
	}
}

static void
skinInstanceCB(MatPipeline *, Geometry *g, Mesh *m, uint8 **data)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(skin == NULL)
		return;
	instanceSkinData(g, m, skin, (uint32*)data[4]);
}

// TODO: call base function perhaps?
int32
findVertexSkin(Geometry *g, uint32 flags[], uint32 mask, SkinVertex *v)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	float32 *wghts = NULL;
	uint8 *inds = NULL;
	if(skin){
		wghts = skin->weights;
		inds = skin->indices;
	}

	float32 *verts = g->morphTargets[0].vertices;
	float32 *tex = g->texCoords[0];
	float32 *norms = g->morphTargets[0].normals;
	uint8 *cols = g->colors;

	for(int32 i = 0; i < g->numVertices; i++){
		uint32 flag = flags ? flags[i] : ~0;
		if(mask & flag & 0x1 &&
		   !(verts[0] == v->p[0] && verts[1] == v->p[1] && verts[2] == v->p[2]))
			goto cont;
		if(mask & flag & 0x10 &&
		   !(norms[0] == v->n[0] && norms[1] == v->n[1] && norms[2] == v->n[2]))
			goto cont;
		if(mask & flag & 0x100 &&
		   !(cols[0] == v->c[0] && cols[1] == v->c[1] && cols[2] == v->c[2] && cols[3] == v->c[3]))
			goto cont;
		if(mask & flag & 0x1000 &&
		   !(tex[0] == v->t[0] && tex[1] == v->t[1]))
			goto cont;
		if(mask & flag & 0x10000 &&
		   !(wghts[0] == v->w[0] && wghts[1] == v->w[1] &&
		     wghts[2] == v->w[2] && wghts[3] == v->w[3] &&
		     inds[0] == v->i[0] && inds[1] == v->i[1] &&
		     inds[2] == v->i[2] && inds[3] == v->i[3]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex += 2;
		norms += 3;
		cols += 4;
		wghts += 4;
		inds += 4;
	}
	return -1;
}

void
insertVertexSkin(Geometry *geo, int32 i, uint32 mask, SkinVertex *v)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	insertVertex(geo, i, mask, v);
	if(mask & 0x10000){
		memcpy(&skin->weights[i*4], v->w, 16);
		memcpy(&skin->indices[i*4], v->i, 4);
	}
}

static void
skinUninstanceCB(MatPipeline*, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
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

	SkinVertex v;
	for(uint32 i = 0; i < mesh->numIndices; i++){
		if(mask & 0x1)
			memcpy(&v.p, verts, 12);
		if(mask & 0x10){
			v.n[0] = norms[0]/127.0f;
			v.n[1] = norms[1]/127.0f;
			v.n[2] = norms[2]/127.0f;
		}
		if(mask & 0x100)
			memcpy(&v.c, colors, 4);
		if(mask & 0x1000)
			memcpy(&v.t, texcoords, 8);
		for(int j = 0; j < 4; j++){
			((uint32*)v.w)[j] = wghts[j] & ~0x3FF;
			v.i[j] = (wghts[j] & 0x3FF) >> 2;
			if(v.i[j]) v.i[j]--;
			if(v.w[j] == 0.0f) v.i[j] = 0;
		}
		int32 idx = findVertexSkin(geo, flags, mask, &v);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		flags[idx] = mask;
		insertVertexSkin(geo, idx, mask, &v);
		verts += 3;
		texcoords += 2;
		colors += 4;
		norms += 3;
		wghts += 4;
	}
}

void
skinPreCB(MatPipeline*, Geometry *geo)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	if(skin == NULL)
		return;
	uint8 *data = skin->data;
	float *invMats = skin->inverseMatrices;
	// meshHeader->totalIndices is highest possible number of vertices again
	skin->init(skin->numBones, skin->numBones, geo->meshHeader->totalIndices);
	memcpy(skin->inverseMatrices, invMats, skin->numBones*64);
	delete[] data;
}

void
skinPostCB(MatPipeline*, Geometry *geo)
{
	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	skin->findNumWeights(geo->numVertices);
	skin->findUsedBones(geo->numVertices);
}

// ADC

int32 adcOffset;

// TODO
void
convertADC(Geometry*)
{
}

// Not optimal but works
void
unconvertADC(Geometry *g)
{
	ADCData *adc = PLUGINOFFSET(ADCData, g, adcOffset);
	if(!adc->adcFormatted)
		return;
	int8 *b = adc->adcBits;
	MeshHeader *h = new MeshHeader;
	h->flags = g->meshHeader->flags;	// should be tristrip
	h->numMeshes = g->meshHeader->numMeshes;
	h->mesh = new Mesh[h->numMeshes];
	Mesh *oldm = g->meshHeader->mesh;
	Mesh *newm = h->mesh;
	h->totalIndices = 0;
	for(int32 i = 0; i < h->numMeshes; i++){
		newm->material = oldm->material;
		newm->numIndices = oldm->numIndices;
		for(uint32 j = 0; j < oldm->numIndices; j++)
			if(*b++)
				newm->numIndices += 2;
		h->totalIndices += newm->numIndices;
		newm++;
		oldm++;
	}
	h->allocateIndices();
	b = adc->adcBits;
	oldm = g->meshHeader->mesh;
	newm = h->mesh;
	for(int32 i = 0; i < h->numMeshes; i++){
		int32 n = 0;
		for(uint32 j = 0; j < oldm->numIndices; j++){
			if(*b++){
				newm->indices[n++] = oldm->indices[j-1];
				newm->indices[n++] = oldm->indices[j-1];
			}
			newm->indices[n++] = oldm->indices[j];
		}
		newm++;
		oldm++;
	}
	delete g->meshHeader;
	g->meshHeader = h;
	adc->adcFormatted = 0;
	delete[] adc->adcBits;
	adc->adcBits = NULL;
	adc->numBits = 0;
}

void
allocateADC(Geometry *geo)
{
	ADCData *adc = PLUGINOFFSET(ADCData, geo, adcOffset);
	adc->adcFormatted = 1;
	adc->numBits = geo->meshHeader->totalIndices;
	int32 size = adc->numBits+3 & ~3;
	adc->adcBits = new int8[size];
	memset(adc->adcBits, 0, size);
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
	adc->adcFormatted = 1;
	if(adc->numBits == 0){
		adc->adcBits = NULL;
		adc->numBits = 0;
		return;
	}
	int32 size = adc->numBits+3 & ~3;
	adc->adcBits = new int8[size];
	stream->read(adc->adcBits, size);
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
		return 0;
	if(geometry->geoflags & Geometry::NATIVE)
		return 16;
	return 16 + (adc->numBits+3 & ~3);
}

void
registerADCPlugin(void)
{
	adcOffset = Geometry::registerPlugin(sizeof(ADCData), ID_ADC,
	                                     createADC, destroyADC, copyADC);
	Geometry::registerPluginStream(ID_ADC,
	                               readADC,
	                               writeADC,
	                               getSizeADC);
}


// PDS plugin

struct PdsGlobals
{
	Pipeline **pipes;
	int32 maxPipes;
	int32 numPipes;
};
PdsGlobals pdsGlobals;

Pipeline*
getPDSPipe(uint32 data)
{
	for(int32 i = 0; i < pdsGlobals.numPipes; i++)
		if(pdsGlobals.pipes[i]->pluginData == data)
			return pdsGlobals.pipes[i];
	return NULL;
}

void
registerPDSPipe(Pipeline *pipe)
{
	assert(pdsGlobals.numPipes < pdsGlobals.maxPipes);
	pdsGlobals.pipes[pdsGlobals.numPipes++] = pipe;
}

static void
atomicPDSRights(void *object, int32, int32, uint32 data)
{
	Atomic *a = (Atomic*)object;
	a->pipeline = (ObjPipeline*)getPDSPipe(data);
}

static void
materialPDSRights(void *object, int32, int32, uint32 data)
{
	Material *m = (Material*)object;
	m->pipeline = (ObjPipeline*)getPDSPipe(data);
}

void
registerPDSPlugin(int32 n)
{
	pdsGlobals.maxPipes = n;
	pdsGlobals.numPipes = 0;
	pdsGlobals.pipes = new Pipeline*[n];
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

#define MAXLEVEL(r) ((r)->tex1[1]>>18 & 0x3F)
#define SETMAXLEVEL(r, l) ((r)->tex1[1] = (r)->tex1[1]&~0xFF0000 | l<<18)
#define SETKL(r, val) ((r)->tex1[1] = (r)->tex1[1]&~0xFFFF | (uint16)(val))
static bool32 noNewStyleRasters;

// i don't really understand this, stolen from RW
static void
ps2MinSize(int32 psm, int32 flags, int32 *minw, int32 *minh)
{
	*minh = 1;
	switch(psm){
	case 0x00:
	case 0x30:
		*minw = 2; // 32 bit
		break;
	case 0x02:
	case 0x0A:
	case 0x32:
	case 0x3A:
		*minw = 4; // 16 bit
		break;
	case 0x01:
	case 0x13:
	case 0x14:
	case 0x1B:
	case 0x24:
	case 0x2C:
	case 0x31:
		*minw = 8; // everything else
		break;
	}
	if(flags & 0x2 && psm == 0x13){	// PSMT8
		*minw = 16;
		*minh = 4;
	}
	if(flags & 0x4 && psm == 0x14){	// PSMT4
		*minw = 32;
		*minh = 4;
	}
}

void
Ps2Raster::create(Raster *raster)
{
	int32 pageWidth, pageHeight;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	//printf("%x %x %x %x\n", raster->format, raster->flags, raster->type, noNewStyleRasters);
	assert(raster->type == 4);	// Texture
	switch(raster->depth){
	case 4:
		pageWidth = 128;
		pageHeight = 128;
		break;
	case 8:
		pageWidth = 128;
		pageHeight = 64;
		break;
	case 16:
		pageWidth = 64;
		pageHeight = 64;
		break;
	case 32:
		pageWidth = 64;
		pageHeight = 32;
		break;
	default:
		assert(0 && "unsupported depth");
	}
	int32 logw = 0, logh = 0;
	int32 s;
	for(s = 1; s < raster->width; s *= 2)
		logw++;
	for(s = 1; s < raster->height; s *= 2)
		logh++;
	SETKL(ras, 0xFC0);
	//printf("%d %d %d %d\n", raster->width, logw, raster->height, logh);
	ras->tex0[0] |= (raster->width < pageWidth ? pageWidth : raster->width)/64 << 14;
	ras->tex0[0] |= logw << 26;
	ras->tex0[0] |= logh << 30;
	ras->tex0[1] |= logh >> 2;

	int32 paletteWidth, paletteHeight, paletteDepth;
	int32 palettePagewidth, palettePageheight;
	if(raster->format & (Raster::PAL4 | Raster::PAL8))
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[1] |= 0xA << 19; // PSMCT16S
			paletteDepth = 2;
			palettePagewidth = palettePageheight = 64;
			break;
		case Raster::C8888:
			// PSMCT32
			paletteDepth = 4;
			palettePagewidth = 64;
			palettePageheight = 32;
			break;
		default:
			assert(0 && "unsupported palette format\n");
		}
	if(raster->format & Raster::PAL4){
		ras->tex0[0] |= 0x14 << 20;   // PSMT4
		ras->tex0[1] |= 1<<29 | 1<<2; // CLD 1, TCC RGBA
		paletteWidth = 8;
		paletteHeight = 2;
	}else if(raster->format & Raster::PAL8){
		ras->tex0[0] |= 0x13 << 20;   // PSMT8
		ras->tex0[1] |= 1<<29 | 1<<2; // CLD 1, TCC RGBA
		paletteWidth = paletteHeight = 16;
	}else{
		paletteWidth = 0;
		paletteHeight = 0;
		paletteDepth = 0;
		palettePagewidth = 0;
		palettePageheight = 0;
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[0] |= 0xA << 20; // PSMCT16S
			ras->tex0[1] |= 1 << 2;  // TCC RGBA
			break;
		case Raster::C8888:
			// PSMCT32
			ras->tex0[1] |= 1 << 2;  // TCC RGBA
			break;
		case Raster::C888:
			ras->tex0[0] |= 1 << 20; // PSMCT24
			break;
		default:
			assert(0 && "unsupported raster format\n");
		}
	}

	raster->stride = raster->width*raster->depth/8;
	if(raster->format & Raster::MIPMAP){
		assert(0);
	}else{
		ras->texelSize = raster->stride*raster->depth+0xF & ~0xF;
		ras->paletteSize = paletteWidth*paletteHeight*paletteDepth;
		ras->miptbp1[0] |= 1<<14;        // TBW1
		ras->miptbp1[1] |= 1<<2 | 1<<22; // TBW2,3
		ras->miptbp2[0] |= 1<<14;	 // TBW4
		ras->miptbp2[1] |= 1<<2 | 1<<22; // TBW5,6
		SETMAXLEVEL(ras, 0);
		int32 nPagW = (raster->width + pageWidth-1)/pageWidth;
		int32 nPagH = (raster->height + pageHeight-1)/pageHeight;
		ras->gsSize = (nPagW*nPagH*0x800)&~0x7FF;
		if(ras->paletteSize){
			// BITBLTBUF DBP
			if(pageWidth*nPagW > raster->width ||
			   pageHeight*nPagH > raster->height)
				ras->tex1[0] = (ras->gsSize >> 6) - 4;
			else{
				ras->tex1[0] = ras->gsSize >> 6;
			}
			nPagW = (paletteWidth + palettePagewidth-1)/palettePagewidth;
			nPagH = (paletteHeight + palettePageheight-1)/palettePageheight;
			ras->gsSize += (nPagW*nPagH*0x800)&~0x7FF;
		}else
			ras->tex1[0] = 0;
	}

	// allocate data and fill with GIF packets
	ras->texelSize = ras->texelSize+0xF & ~0xF;
	int32 numLevels = MAXLEVEL(ras)+1;
	if(noNewStyleRasters ||
	   (raster->width*raster->height*raster->depth & ~0x7F) >= 0x3FFF80){
		assert(0);
	}else{
		ras->flags |= 1;	// include GIF packets
		int32 psm = ras->tex0[0]>>20 & 0x3F;
		//int32 cpsm = ras->tex0[1]>>19 & 0x3F;
		if(psm == 0x13){	// PSMT8
			ras->flags |= 2;
			// TODO: stuff
		}
		if(psm == 0x14){	// PSMT4
			// swizzle flag probably depends on version :/
			if(rw::version > 0x31000)
				ras->flags |= 4;
			// TODO: stuff
		}
		ras->texelSize = 0x50*numLevels;	// GIF packets
		int32 minW, minH;
		ps2MinSize(psm, ras->flags, &minW, &minH);
		int32 w = raster->width;
		int32 h = raster->height;
		int32 mipw, miph;
		int32 n = numLevels;
		while(n--){
			mipw = w < minW ? minW : w;
			miph = h < minH ? minH : h;
			ras->texelSize += mipw*miph*raster->depth/8+0xF & ~0xF;
			w /= 2;
			h /= 2;
		}
		if(ras->paletteSize){
			if(rw::version > 0x31000 && paletteHeight == 2)
				paletteHeight = 3;
			ras->paletteSize = 0x50 +
			    paletteDepth*paletteWidth*paletteHeight;
		}
		// TODO: allocate space for more DMA packets
		ras->dataSize = ras->paletteSize+ras->texelSize;
		uint8 *data = new uint8[ras->dataSize];
		assert(data);
		ras->data = data;
		raster->texels = data + 0x50;
		if(ras->paletteSize)
			raster->palette = data + ras->texelSize + 0x50;
		uint32 *p = (uint32*)data;
		w = raster->width;
		h = raster->height;
		for(n = 0; n < numLevels; n++){
			mipw = w < minW ? minW : w;
			miph = h < minH ? minH : h;

			// GIF tag
			*p++ = 3;          // NLOOP = 3
			*p++ = 0x10000000; // NREG = 1
			*p++ = 0xE;        // A+D
			*p++ = 0;

			// TRXPOS
			*p++ = 0;	// TODO
			*p++ = 0;	// TODO
			*p++ = 0x51;
			*p++ = 0;

			// TRXREG
			if(ras->flags & 2 && psm == 0x13 ||
			   ras->flags & 4 && psm == 0x14){
				*p++ = mipw/2;
				*p++ = miph/2;
			}else{
				*p++ = mipw;
				*p++ = miph;
			}
			*p++ = 0x52;
			*p++ = 0;

			// TRXDIR
			*p++ = 0;	// host -> local
			*p++ = 0;
			*p++ = 0x53;
			*p++ = 0;

			// GIF tag
			uint32 sz = mipw*miph*raster->depth/8 + 0xF >> 4;
			*p++ = sz;
			*p++ = 0x08000000; // IMAGE
			*p++ = 0;
			*p++ = 0;

			p += sz*4;
			w /= 2;
			h /= 2;
		}
		if(ras->paletteSize){
			p = (uint32*)(raster->palette - 0x50);
			// GIF tag
			*p++ = 3;          // NLOOP = 3
			*p++ = 0x10000000; // NREG = 1
			*p++ = 0xE;        // A+D
			*p++ = 0;

			// TRXPOS
			*p++ = 0;	// TODO
			*p++ = 0;	// TODO
			*p++ = 0x51;
			*p++ = 0;

			// TRXREG
			*p++ = paletteWidth;
			*p++ = paletteHeight;
			*p++ = 0x52;
			*p++ = 0;

			// TRXDIR
			*p++ = 0;	// host -> local
			*p++ = 0;
			*p++ = 0x53;
			*p++ = 0;

			// GIF tag
			uint32 sz = paletteSize - 0x50 + 0xF >> 4;
			*p++ = sz;
			*p++ = 0x08000000; // IMAGE
			*p++ = 0;
			*p++ = 0;

		}
	}
}

uint8*
Ps2Raster::lock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
	return NULL;
}

void
Ps2Raster::unlock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
}

int32
Ps2Raster::getNumLevels(Raster *raster)
{
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	if(raster->texels == NULL) return 0;
	if(raster->format & Raster::MIPMAP)
		return MAXLEVEL(ras)+1;
	return 1;
}
	
static void*
createNativeRaster(void *object, int32 offset, int32)
{
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, object, offset);
	new (raster) Ps2Raster;
	raster->tex0[0] = 0;
	raster->tex0[1] = 0;
	raster->tex1[0] = 0;
	raster->tex1[1] = 0;
	raster->miptbp1[0] = 0;
	raster->miptbp1[1] = 0;
	raster->miptbp2[0] = 0;
	raster->miptbp2[1] = 0;
	raster->texelSize = 0;
	raster->paletteSize = 0;
	raster->gsSize = 0;
	raster->flags = 0;
	SETKL(raster, 0xFC0);

	raster->dataSize = 0;
	raster->data = NULL;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	// TODO
	(void)offset;
	return object;
}

static void*
copyNativeRaster(void *dst, void *src, int32 offset, int32)
{
	Ps2Raster *dstraster = PLUGINOFFSET(Ps2Raster, dst, offset);
	Ps2Raster *srcraster = PLUGINOFFSET(Ps2Raster, src, offset);
	*dstraster = *srcraster;
	return dst;
}

static void
readMipmap(Stream *stream, int32, void *object, int32 offset, int32)
{
	uint16 val = stream->readI32();
	Texture *tex = (Texture*)object;
	if(tex->raster == NULL)
		return;
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, offset);
	SETKL(raster, val);
}

static void
writeMipmap(Stream *stream, int32, void *object, int32 offset, int32)
{
	Texture *tex = (Texture*)object;
	assert(tex->raster);
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, offset);
	stream->writeI32(raster->tex1[1]&0xFFFF);
}

static int32
getSizeMipmap(void*, int32, int32)
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

struct StreamRasterExt
{
	int32 width;
	int32 height;
	int32 depth;
	uint16 rasterFormat;
	int16  type;
	uint32 tex0[2];
	uint32 tex1[2];
	uint32 miptbp1[2];
	uint32 miptbp2[2];
	uint32 texelSize;
	uint32 paletteSize;
	uint32 gsSize;
	uint32 mipmapVal;
};

Texture*
readNativeTexture(Stream *stream)
{
	uint32 length, oldversion, version;
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	assert(stream->readU32() == 0x00325350);	// 'PS2\0'
	Texture *tex = Texture::create(NULL);

	// Texture
	tex->filterAddressing = stream->readU32();
	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(tex->name, length);
	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(tex->mask, length);

	// Raster
	StreamRasterExt streamExt;
	oldversion = rw::version;
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(&streamExt, 0x40);
	Raster *raster;
	noNewStyleRasters = streamExt.type < 2;
	rw::version = version;
	raster = Raster::create(streamExt.width, streamExt.height,
	                        streamExt.depth, streamExt.rasterFormat,
	                        PLATFORM_PS2);
	noNewStyleRasters = 0;
	rw::version = oldversion;
	tex->raster = raster;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	//printf("%08X%08X %08X%08X %08X%08X %08X%08X\n",
	//       ras->tex0[1], ras->tex0[0], ras->tex1[1], ras->tex1[0],
	//       ras->miptbp1[0], ras->miptbp1[1], ras->miptbp2[0], ras->miptbp2[1]);
	ras->tex0[0] = streamExt.tex0[0];
	ras->tex0[1] = streamExt.tex0[1];
	ras->tex1[0] = streamExt.tex1[0];
	ras->tex1[1] = ras->tex1[1]&~0xFF0000 | streamExt.tex1[1]<<16 & 0xFF0000;
	ras->miptbp1[0] = streamExt.miptbp1[0];
	ras->miptbp1[1] = streamExt.miptbp1[1];
	ras->miptbp2[0] = streamExt.miptbp2[0];
	ras->miptbp2[1] = streamExt.miptbp2[1];
	ras->texelSize = streamExt.texelSize;
	ras->paletteSize = streamExt.paletteSize;
	ras->gsSize = streamExt.gsSize;
	SETKL(ras, streamExt.mipmapVal);
	//printf("%08X%08X %08X%08X %08X%08X %08X%08X\n",
	//       ras->tex0[1], ras->tex0[0], ras->tex1[1], ras->tex1[0],
	//       ras->miptbp1[0], ras->miptbp1[1], ras->miptbp2[0], ras->miptbp2[1]);

	assert(findChunk(stream, ID_STRUCT, &length, NULL));
	if(streamExt.type < 2){
		stream->read(raster->texels, length);
	}else{
		stream->read(raster->texels-0x50, ras->texelSize);
		stream->read(raster->palette-0x50, ras->paletteSize);
	}

	tex->streamReadPlugins(stream);
	return tex;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_TEXTURENATIVE, chunksize);
	writeChunkHeader(stream, ID_STRUCT, 8);
	stream->writeU32(FOURCC_PS2);
	stream->writeU32(tex->filterAddressing);
	int32 len = strlen(tex->name)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, len);
	stream->write(tex->name, len);
	len = strlen(tex->mask)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, len);
	stream->write(tex->mask, len);

	int32 sz = ras->texelSize + ras->paletteSize;
	writeChunkHeader(stream, ID_STRUCT, 12 + 64 + 12 + sz);
	writeChunkHeader(stream, ID_STRUCT, 64);
	StreamRasterExt streamExt;
	streamExt.width = raster->width;
	streamExt.height = raster->height;
	streamExt.depth = raster->depth;
	streamExt.rasterFormat = raster->format | raster->type;
	streamExt.type = 0;
	if(ras->flags == 2 && raster->depth == 8)
		streamExt.type = 1;
	if(ras->flags & 1)
		streamExt.type = 2;
	streamExt.tex0[0] = ras->tex0[0];
	streamExt.tex0[1] = ras->tex0[1];
	streamExt.tex1[0] = ras->tex1[0];
	streamExt.tex1[1] = ras->tex1[1]>>16 & 0xFF;
	streamExt.miptbp1[0] = ras->miptbp1[0];
	streamExt.miptbp1[1] = ras->miptbp1[1];
	streamExt.miptbp2[0] = ras->miptbp2[0];
	streamExt.miptbp2[1] = ras->miptbp2[1];
	streamExt.texelSize = ras->texelSize;
	streamExt.paletteSize = ras->paletteSize;
	streamExt.gsSize = ras->gsSize;
	streamExt.mipmapVal = ras->tex1[1]&0xFFFF;
	stream->write(&streamExt, 64);

	writeChunkHeader(stream, ID_STRUCT, sz);
	if(streamExt.type < 2){
		stream->write(raster->texels, sz);
	}else{
		stream->write(raster->texels-0x50, ras->texelSize);
		stream->write(raster->palette-0x50, ras->paletteSize);
	}
	tex->streamWritePlugins(stream);
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 8;
	size += 12 + strlen(tex->name)+4 & ~3;
	size += 12 + strlen(tex->mask)+4 & ~3;
	size += 12 + 12 + 64 + 12;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, tex->raster, nativeRasterOffset);
	size += ras->texelSize + ras->paletteSize;
	size += 12 + tex->streamGetPluginSize();
	return size;
}

}
}
