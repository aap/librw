#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwps2.h"

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

PipeAttribute attribXYZW = {
	"XYZW",
	AT_V4_32
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
	if(vertCount == 0)
		return 0;
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
instanceXYZW(uint32 *p, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	uint32 *d = (uint32*)g->morphTargets[0].vertices;
	int8 *adcbits = getADCbitsForMesh(g, m);
	for(uint32 i = idx; i < idx+n; i++){
		j = m->indices[i];
		*p++ = d[j*3+0];
		*p++ = d[j*3+1];
		*p++ = d[j*3+2];
		*p++ = adcbits && adcbits[i] ? 0x8000 : 0;
	}
	// don't need to pad
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
instanceUV2(uint32 *p, Geometry *g, Mesh *m, uint32 idx, uint32 n)
{
	uint16 j;
	uint32 *d0 = (uint32*)g->texCoords[0];
	uint32 *d1 = (uint32*)g->texCoords[1];
	for(uint32 i = idx; i < idx+n; i++){
		j = m->indices[i];
		if(g->numTexCoordSets > 0){
			*p++ = d0[j*2+0];
			*p++ = d0[j*2+1];
		}else{
			*p++ = 0;
			*p++ = 0;
		}
		if(g->numTexCoordSets > 1){
			*p++ = d1[j*2+0];
			*p++ = d1[j*2+1];
		}else{
			*p++ = 0;
			*p++ = 0;
		}
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
	printf("%x %x\n", this->pluginID, this->pluginData);
	for(uint i = 0; i < nelem(this->attribs); i++){
		a = this->attribs[i];
		if(a)
			printf("%d %s: %x\n", i, a->name, a->attrib);
	}
	printf("stride: %x\n", this->inputStride);
	printf("vertcount: %x\n", this->vifOffset/this->inputStride);
	printf("triSCount: %x\n", this->triStripCount);
	printf("triLCount: %x\n", this->triListCount);
	printf("vifOffset: %x\n", this->vifOffset);
	printf("\n");
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

// Instance format:
//  no broken out clusters
//  ======================
//  DMAret [FLUSH; MSKPATH3 || FLUSH; FLUSH] {
//  	foreach batch {
//  		foreach cluster {
//  			MARK/0; STMOD; STCYCL; UNPACK
//  			unpack-data
//  		}
//  		ITOP; MSCALF/MSCNT;                      // if first/not-first
//  		0/FLUSH; 0/MSKPATH3 || 0/FLUSH; 0/FLUSH  // if not-last/last
//  	}
//  }
//
//  broken out clusters
//  ===================
//  foreach batch {
//  	foreach broken out cluster {
//  		DMAref [STCYCL; UNPACK] -> pointer into unpack-data
//  		DMAcnt (empty)
//  	}
//  	DMAcnt/ret {
//  		foreach cluster {
//  			MARK/0; STMOD; STCYCL; UNPACK
//  			unpack-data
//  		}
//  		ITOP; MSCALF/MSCNT;                      // if first/not-first
//  		0/FLUSH; 0/MSKPATH3 || 0/FLUSH; 0/FLUSH  // if not-last/last
//  	}
//  }
//  unpack-data for broken out clusters

uint32 markcnt = 0;

enum {
	DMAcnt       = 0x10000000,
	DMAref       = 0x30000000,
	DMAret       = 0x60000000,

	VIF_NOP      = 0,
	VIF_STCYCL   = 0x01000100,	// WL = 1
	VIF_ITOP     = 0x04000000,
	VIF_STMOD    = 0x05000000,
	VIF_MSKPATH3 = 0x06000000,
	VIF_MARK     = 0x07000000,
	VIF_FLUSH    = 0x11000000,
	VIF_MSCALF   = 0x15000000,
	VIF_MSCNT    = 0x17000000,
};

struct InstMeshInfo
{
	uint32 numAttribs, numBrokenAttribs;
	uint32 batchVertCount, lastBatchVertCount;
	uint32 numBatches;
	uint32 batchSize, lastBatchSize;
	uint32 size;	// size of DMA chain without broken out data
	uint32 size2;	// size of broken out data
	uint32 vertexSize;
	uint32 attribPos[10];
};

InstMeshInfo
getInstMeshInfo(MatPipeline *pipe, Geometry *g, Mesh *m)
{
	PipeAttribute *a;
	InstMeshInfo im;
	im.numAttribs = 0;
	im.numBrokenAttribs = 0;
	im.vertexSize = 0;
	for(uint i = 0; i < nelem(pipe->attribs); i++)
		if(a = pipe->attribs[i])
			if(a->attrib & AT_RW)
				im.numBrokenAttribs++;
			else{
				im.vertexSize += attribSize(a->attrib);
				im.numAttribs++;
			}
	if(g->meshHeader->flags == MeshHeader::TRISTRIP){
		im.numBatches = (m->numIndices-2) / (pipe->triStripCount-2);
		im.batchVertCount = pipe->triStripCount;
		im.lastBatchVertCount = (m->numIndices-2) % (pipe->triStripCount-2);
		if(im.lastBatchVertCount){
			im.numBatches++;
			im.lastBatchVertCount += 2;
		}
	}else{	// TRILIST; nothing else supported yet
		im.numBatches = (m->numIndices+pipe->triListCount-1) /
		                 pipe->triListCount;
		im.batchVertCount = pipe->triListCount;
		im.lastBatchVertCount = m->numIndices % pipe->triListCount;
	}
	if(im.lastBatchVertCount == 0)
		im.lastBatchVertCount = im.batchVertCount;

	im.batchSize = getBatchSize(pipe, im.batchVertCount);
	im.lastBatchSize = getBatchSize(pipe, im.lastBatchVertCount);
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

	// TODO: not sure if this is correct
	uint32 msk_flush = rw::version >= 0x35000 ? VIF_FLUSH : VIF_MSKPATH3;

	uint32 idx = 0;
	uint32 *p = (uint32*)inst->data;
	if(im.numBrokenAttribs == 0){
		*p++ = DMAret | im.size-1;
		*p++ = 0;
		*p++ = VIF_FLUSH;
		*p++ = msk_flush;
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
				*p++ = DMAref | QWC(nverts*atsz);
				*p++ = im.attribPos[i];
				*p++ = VIF_STCYCL | this->inputStride;
				// Round up nverts so UNPACK will fit exactly into the DMA packet
				//  (can't pad with zeroes in broken out sections).
				// TODO: check for clash with vifOffset somewhere
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | (QWC(nverts*atsz)<<4)/atsz << 16 | i; // UNPACK

				*p++ = DMAcnt;
				*p++ = 0x0;
				*p++ = VIF_NOP;
				*p++ = VIF_NOP;

				im.attribPos[i] += g->meshHeader->flags == 1 ?
					QWC((im.batchVertCount-2)*atsz) :
					QWC(im.batchVertCount*atsz);
			}
		if(im.numBrokenAttribs){
			*p++ = (j < im.numBatches-1 ? DMAcnt : DMAret) | bsize;
			*p++ = 0x0;
			*p++ = VIF_NOP;
			*p++ = VIF_NOP;
		}

		for(uint i = 0; i < nelem(this->attribs); i++)
			if((a = this->attribs[i]) && (a->attrib & AT_RW) == 0){
				if(rw::version >= 0x35000)
					*p++ = VIF_NOP;
				else
					*p++ = VIF_MARK | markcnt++;
				*p++ = VIF_STMOD;
				*p++ = VIF_STCYCL | this->inputStride;
				*p++ = (a->attrib&0xFF004000)
					| 0x8000 | nverts << 16 | i; // UNPACK

				if(a == &attribXYZ)
					p = instanceXYZ(p, g, m, idx, nverts);
				else if(a == &attribXYZW)
					p = instanceXYZW(p, g, m, idx, nverts);
				else if(a == &attribUV)
					p = instanceUV(p, g, m, idx, nverts);
				else if(a == &attribUV2)
					p = instanceUV2(p, g, m, idx, nverts);
				else if(a == &attribRGBA)
					p = instanceRGBA(p, g, m, idx, nverts);
				else if(a == &attribNormal)
					p = instanceNormal(p, g, m, idx, nverts);
			}
		idx += g->meshHeader->flags == 1
			? im.batchVertCount-2 : im.batchVertCount;

		*p++ = VIF_ITOP | nverts;
		*p++ = j == 0 ? VIF_MSCALF : VIF_MSCNT;
		if(j < im.numBatches-1){
			*p++ = VIF_NOP;
			*p++ = VIF_NOP;
		}else{
			*p++ = VIF_FLUSH;
			*p++ = msk_flush;
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

	uint8 *raw = im.vertexSize*m->numIndices ?
		new uint8[im.vertexSize*m->numIndices] : NULL;
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
				if((p[-1] & 0xff004000) != a->attrib){
					fprintf(stderr, "unexpected unpack xx: %08x %08x\n", p[-1], a->attrib);
					assert(0 && "unexpected unpack\n");
				}
				memcpy(datap[i], p, asz*nverts);
				datap[i] += asz*(nverts-overlap);
				p += QWC(asz*nverts)*4;
			}
		p += 4;
	}
	return raw;
}

static void
objInstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
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
		m = pipe->groupPipeline ?
		    pipe->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL)
			m = defaultMatPipe;
		m->instance(geo, instance, mesh);
		instance->material = mesh->material;
	}
	geo->geoflags |= Geometry::NATIVE;
}

static void
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

static void
objUninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
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
		m = pipe->groupPipeline ?
		    pipe->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;
		if(m->preUninstCB) m->preUninstCB(m, geo);
	}
	geo->numVertices = 0;
	for(uint32 i = 0; i < header->numMeshes; i++){
		Mesh *mesh = &geo->meshHeader->mesh[i];
		InstanceData *instance = &header->instanceMeshes[i];
		MatPipeline *m;
		m = pipe->groupPipeline ?
		    pipe->groupPipeline :
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
		m = pipe->groupPipeline ?
		    pipe->groupPipeline :
		    (MatPipeline*)mesh->material->pipeline;
		if(m == NULL) m = defaultMatPipe;
		if(m->postUninstCB) m->postUninstCB(m, geo);
	}

	int8 *bits = getADCbits(geo);
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

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform)
{
	this->groupPipeline = NULL;
	this->impl.instance = objInstance;
	this->impl.uninstance = objUninstance;
}

int32
findVertex(Geometry *g, uint32 flags[], uint32 mask, Vertex *v)
{
	float32 *verts = g->morphTargets[0].vertices;
	float32 *tex = g->texCoords[0];
	float32 *tex1 = g->texCoords[1];
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
		if(mask & flags[i] & 0x2000 &&
		   !(tex1[0] == v->t1[0] && tex1[1] == v->t1[1]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex += 2;
		tex1 += 2;
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
	if(mask & 0x2000)
		memcpy(&geo->texCoords[1][i*2], v->t1, 8);
}

void
genericUninstanceCB(MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
{
//extern PipeAttribute attribXYZ;
//extern PipeAttribute attribXYZW;
//extern PipeAttribute attribUV;
//extern PipeAttribute attribUV2;
//extern PipeAttribute attribRGBA;
//extern PipeAttribute attribNormal;
//extern PipeAttribute attribWeights;
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
	for(int32 i = 0; i < geo->numTexCoordSets; i++)
		mask |= 0x1000 << i;
	int numUV = pipe->attribs[AT_UV] == &attribUV2 ? 2 : 1;

	Vertex v;
	for(uint32 i = 0; i < mesh->numIndices; i++){
		if(mask & 0x1)
			memcpy(&v.p, verts, 12);
		if(mask & 0x10){
			v.n[0] = norms[0]/127.0f;
			v.n[1] = norms[1]/127.0f;
			v.n[2] = norms[2]/127.0f;
		}
		if(mask & 0x100){
			memcpy(&v.c, colors, 4);
			//v.c[3] = 0xFF;
		}
		if(mask & 0x1000)
			memcpy(&v.t, texcoords, 8);
		if(mask & 0x2000)
			memcpy(&v.t1, texcoords+2, 8);

		int32 idx = findVertex(geo, flags, mask, &v);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		flags[idx] = mask;
		insertVertex(geo, idx, mask, &v);
		verts += 3;
		texcoords += 2*numUV;
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

void
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

void
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

int8*
getADCbits(Geometry *geo)
{
	int8 *bits = NULL;
	if(adcOffset){
		ADCData *adc = PLUGINOFFSET(ADCData, geo, adcOffset);
		if(adc->adcFormatted)
			bits = adc->adcBits;
	}
	return bits;
}

int8*
getADCbitsForMesh(Geometry *geo, Mesh *mesh)
{
	int8 *bits = getADCbits(geo);
	if(bits == NULL)
		return NULL;
	int32 n = mesh - geo->meshHeader->mesh;
	for(int32 i = 0; i < n; i++)
		bits += geo->meshHeader->mesh[i].numIndices;
	return bits;
}

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

// misc stuff

void
printDMA(InstanceData *inst)
{
	uint32 *tag = (uint32*)inst->data;
	for(;;){
		switch(tag[0]&0x70000000){
		case DMAcnt:
			printf("%08x %08x\n", tag[0], tag[1]);
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		case DMAref:
			printf("%08x %08x\n", tag[0], tag[1]);
			tag += 4;
			break;

		case DMAret:
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
		case DMAcnt:
			tag += (1+(tag[0]&0xFFFF))*4;
			break;

		case DMAref:
			last = base + tag[1]*4 + (tag[0]&0xFFFF)*4;
			tag += 4;
			break;

		case DMAret:
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
