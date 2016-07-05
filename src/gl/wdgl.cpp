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
#include "rwwdgl.h"

#ifdef RW_OPENGL
#include <GL/glew.h>
#endif

#define PLUGIN_ID 2

namespace rw {
namespace wdgl {

void*
driverOpen(void *o, int32, int32)
{
	driver[PLATFORM_WDGL]->defaultPipeline = makeDefaultPipeline();
	return o;
}

void*
driverClose(void *o, int32, int32)
{
	return o;
}

void
initializePlatform(void)
{
	Driver::registerPlugin(PLATFORM_WDGL, 0, PLATFORM_WDGL,
	                       driverOpen, driverClose);
}


// VC
//   8733 0 0 0 3
//     45 1 0 0 2
//   8657 1 3 0 2
//   4610 2 1 1 3
//   4185 3 2 1 4
//    256 4 2 1 4
//    201 4 4 1 4
//    457 5 2 0 4

// SA
//  20303 0 0 0 3	vertices:  3 floats
//     53 1 0 0 2	texCoords: 2 floats
//  20043 1 3 0 2	texCoords: 2 shorts
//   6954 2 1 1 3	normal:    3 bytes normalized
//  13527 3 2 1 4	color:     4 ubytes normalized
//    196 4 2 1 4	weight:    4 ubytes normalized
//    225 4 4 1 4	weight:    4 ushorts normalized
//    421 5 2 0 4	indices:   4 ubytes
//  12887 6 2 1 4	extracolor:4 ubytes normalized

/*
static void
printAttribInfo(AttribDesc *attribs, int n)
{
	for(int i = 0; i < n; i++)
		printf("%x %x %x %x\n",
			attribs[i].index,
			attribs[i].type,
			attribs[i].normalized,
			attribs[i].size);
}
*/

#ifdef RW_OPENGL
void
uploadGeo(Geometry *geo)
{
	InstanceDataHeader *inst = (InstanceDataHeader*)geo->instData;
	MeshHeader *meshHeader = geo->meshHeader;

	glGenBuffers(1, &inst->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, inst->vbo);
	glBufferData(GL_ARRAY_BUFFER, inst->dataSize,
	             inst->data, GL_STATIC_DRAW);

	glGenBuffers(1, &inst->ibo);
	glBindBuffer(GL_ARRAY_BUFFER, inst->ibo);
	glBufferData(GL_ARRAY_BUFFER, meshHeader->totalIndices*2,
	             0, GL_STATIC_DRAW);
	GLintptr offset = 0;
	for(uint32 i = 0; i < meshHeader->numMeshes; i++){
		Mesh *mesh = &meshHeader->mesh[i];
		glBufferSubData(GL_ARRAY_BUFFER, offset, mesh->numIndices*2,
		                mesh->indices);
		offset += mesh->numIndices*2;
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
setAttribPointers(InstanceDataHeader *inst)
{
	static GLenum attribType[] = {
		GL_FLOAT,
		GL_BYTE, GL_UNSIGNED_BYTE,
		GL_SHORT, GL_UNSIGNED_SHORT
	};
	for(int32 i = 0; i < inst->numAttribs; i++){
		AttribDesc *a = &inst->attribs[i];
		glEnableVertexAttribArray(a->index);
		glVertexAttribPointer(a->index, a->size, attribType[a->type],
		                      a->normalized, a->stride,
		                      (void*)(uint64)a->offset);
	}
}
#endif

void
packattrib(uint8 *dst, float32 *src, AttribDesc *a, float32 scale=1.0f)
{
	int8 *i8dst;
	uint16 *u16dst;
	int16 *i16dst;

	switch(a->type){
	case 0:	// float
		memcpy(dst, src, a->size*4);
		break;

	// TODO: maybe have loop inside if?
	case 1: // byte
		i8dst = (int8*)dst;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				i8dst[i] = src[i]*scale;
			else
				i8dst[i] = src[i]*127.0f;
		}
		break;

	case 2: // ubyte
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = src[i]*scale;
			else
				dst[i] = src[i]*255.0f;
		}
		break;

	case 3: // short
		i16dst = (int16*)dst;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				i16dst[i] = src[i]*scale;
			else
				i16dst[i] = src[i]*32767.0f;
		}
		break;

	case 4: // ushort
		u16dst = (uint16*)dst;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				u16dst[i] = src[i]*scale;
			else
				u16dst[i] = src[i]*65535.0f;
		}
		break;
	}
}

void
unpackattrib(float *dst, uint8 *src, AttribDesc *a, float32 scale=1.0f)
{
	int8 *i8src;
	uint16 *u16src;
	int16 *i16src;

	switch(a->type){
	case 0:	// float
		memcpy(dst, src, a->size*4);
		break;

	// TODO: maybe have loop inside if?
	case 1: // byte
		i8src = (int8*)src;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = i8src[i]/scale;
			else
				dst[i] = i8src[i]/127.0f;
		}
		break;

	case 2: // ubyte
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = src[i]/scale;
			else
				dst[i] = src[i]/255.0f;
		}
		break;

	case 3: // short
		i16src = (int16*)src;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = i16src[i]/scale;
			else
				dst[i] = i16src[i]/32767.0f;
		}
		break;

	case 4: // ushort
		u16src = (uint16*)src;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = u16src[i]/scale;
			else
				dst[i] = u16src[i]/65435.0f;
		}
		break;
	}
}

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_WDGL)
		return object;
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	geometry->instData = nil;
	// TODO: delete ibo and vbo
	delete[] header->attribs;
	delete[] header->data;
	delete header;
	return object;
}

Stream*
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_WDGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs = stream->readU32();
	header->attribs = new AttribDesc[header->numAttribs];
	stream->read(header->attribs,
	            header->numAttribs*sizeof(AttribDesc));
	header->dataSize = header->attribs[0].stride*geometry->numVertices;
	header->data = new uint8[header->dataSize];
	stream->read(header->data, header->dataSize);
	return stream;
}

Stream*
writeNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_WDGL)
		return stream;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	stream->writeU32(header->numAttribs);
	stream->write(header->attribs, header->numAttribs*sizeof(AttribDesc));
	stream->write(header->data, header->dataSize);
	return stream;
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil ||
	   geometry->instData->platform != PLATFORM_WDGL)
		return 0;
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	return 4 + header->numAttribs*sizeof(AttribDesc) + header->dataSize;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         nil, destroyNativeData, nil);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               readNativeData,
	                               writeNativeData,
	                               getSizeNativeData);
}

void
printPipeinfo(Atomic *a)
{
	Geometry *g = a->geometry;
	if(g->instData == nil || g->instData->platform != PLATFORM_WDGL)
		return;
	int32 plgid = 0;
	if(a->pipeline)
		plgid = a->pipeline->pluginID;
	printf("%s %x: ", debugFile, plgid);
	InstanceDataHeader *h = (InstanceDataHeader*)g->instData;
	for(int i = 0; i < h->numAttribs; i++)
		printf("%x(%x) ", h->attribs[i].index, h->attribs[i].type);
	printf("\n");
}

static void
instance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if(geo->geoflags & Geometry::NATIVE)
		return;
	InstanceDataHeader *header = new InstanceDataHeader;
	geo->instData = header;
	header->platform = PLATFORM_WDGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs =
		pipe->numCustomAttribs + 1 + (geo->numTexCoordSets > 0);
	if(geo->geoflags & Geometry::PRELIT)
		header->numAttribs++;
	if(geo->geoflags & Geometry::NORMALS)
		header->numAttribs++;
	int32 offset = 0;
	header->attribs = new AttribDesc[header->numAttribs];

	AttribDesc *a = header->attribs;
	// Vertices
	a->index = 0;
	a->type = 0;
	a->normalized = 0;
	a->size = 3;
	a->offset = offset;
	offset += 12;
	a++;
	int32 firstCustom = 1;

	// texCoords, only one set here
	if(geo->numTexCoordSets){
		a->index = 1;
		a->type = 3;
		a->normalized = 0;
		a->size = 2;
		a->offset = offset;
		offset += 4;
		a++;
		firstCustom++;
	}

	if(geo->geoflags & Geometry::NORMALS){
		a->index = 2;
		a->type = 1;
		a->normalized = 1;
		a->size = 3;
		a->offset = offset;
		offset += 4;
		a++;
		firstCustom++;
	}

	if(geo->geoflags & Geometry::PRELIT){
		a->index = 3;
		a->type = 2;
		a->normalized = 1;
		a->size = 4;
		a->offset = offset;
		offset += 4;
		a++;
		firstCustom++;
	}

	if(pipe->instanceCB)
		offset += pipe->instanceCB(geo, firstCustom, offset);
	else{
		header->dataSize = offset*geo->numVertices;
		header->data = new uint8[header->dataSize];
	}

	a = header->attribs;
	for(int32 i = 0; i < header->numAttribs; i++)
		a[i].stride = offset;

	uint8 *p = header->data + a->offset;
	float32 *vert = geo->morphTargets->vertices;
	for(int32 i = 0; i < geo->numVertices; i++){
		packattrib(p, vert, a);
		vert += 3;
		p += a->stride;
	}
	a++;

	if(geo->numTexCoordSets){
		p = header->data + a->offset;
		float32 *texcoord = geo->texCoords[0];
		for(int32 i = 0; i < geo->numVertices; i++){
			packattrib(p, texcoord, a, 512.0f);
			texcoord += 2;
			p += a->stride;
		}
		a++;
	}

	if(geo->geoflags & Geometry::NORMALS){
		p = header->data + a->offset;
		float32 *norm = geo->morphTargets->normals;
		for(int32 i = 0; i < geo->numVertices; i++){
			packattrib(p, norm, a);
			norm += 3;
			p += a->stride;
		}
		a++;
	}

	if(geo->geoflags & Geometry::PRELIT){
		// TODO: this seems too complicated
		p = header->data + a->offset;
		uint8 *color = geo->colors;
		float32 f[4];
		for(int32 i = 0; i < geo->numVertices; i++){
			f[0] = color[0]/255.0f;
			f[1] = color[1]/255.0f;
			f[2] = color[2]/255.0f;
			f[3] = color[3]/255.0f;
			packattrib(p, f, a);
			color += 4;
			p += a->stride;
		}
		a++;
	}
	geo->geoflags |= Geometry::NATIVE;
}

static void
uninstance(rw::ObjPipeline *rwpipe, Atomic *atomic)
{
	ObjPipeline *pipe = (ObjPipeline*)rwpipe;
	Geometry *geo = atomic->geometry;
	if((geo->geoflags & Geometry::NATIVE) == 0)
		return;
	assert(geo->instData != nil);
	assert(geo->instData->platform == PLATFORM_WDGL);
	geo->geoflags &= ~Geometry::NATIVE;
	geo->allocateData();

	uint8 *p;
	float32 *texcoord = geo->texCoords[0];
	uint8 *color = geo->colors;
	float32 *vert = geo->morphTargets->vertices;
	float32 *norm = geo->morphTargets->normals;
	float32 f[4];

	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;
	for(int i = 0; i < header->numAttribs; i++){
		AttribDesc *a = &header->attribs[i];
		p = header->data + a->offset;

		switch(a->index){
		case 0:		// Vertices
			for(int32 i = 0; i < geo->numVertices; i++){
				unpackattrib(vert, p, a);
				vert += 3;
				p += a->stride;
			}
			break;

		case 1:		// texCoords
			for(int32 i = 0; i < geo->numVertices; i++){
				unpackattrib(texcoord, p, a, 512.0f);
				texcoord += 2;
				p += a->stride;
			}
			break;

		case 2:		// normals
			for(int32 i = 0; i < geo->numVertices; i++){
				unpackattrib(norm, p, a);
				norm += 3;
				p += a->stride;
			}
			break;

		case 3:		// colors
			for(int32 i = 0; i < geo->numVertices; i++){
				// TODO: this seems too complicated
				unpackattrib(f, p, a);
				color[0] = f[0]*255.0f;
				color[1] = f[1]*255.0f;
				color[2] = f[2]*255.0f;
				color[3] = f[3]*255.0f;
				color += 4;
				p += a->stride;
			}
			break;
		}
	}

	if(pipe->uninstanceCB)
		pipe->uninstanceCB(geo);

	geo->generateTriangles();

	destroyNativeData(geo, 0, 0);
}

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform)
{
	this->numCustomAttribs = 0;
	this->impl.instance = wdgl::instance;
	this->impl.uninstance = wdgl::uninstance;
	this->instanceCB = nil;
	this->uninstanceCB = nil;
}

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_WDGL);
	return pipe;
}

// Skin

Stream*
readNativeSkin(Stream *stream, int32, void *object, int32 offset)
{
	Geometry *geometry = (Geometry*)object;
	uint32 platform;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	platform = stream->readU32();
	if(platform != PLATFORM_GL){
		RWERROR((ERR_PLATFORM, platform));
		return nil;
	}
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;

	int32 numBones = stream->readI32();
	skin->init(numBones, 0, 0);
	stream->read(skin->inverseMatrices, skin->numBones*64);
	return stream;
}

Stream*
writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset)
{
	writeChunkHeader(stream, ID_STRUCT, len-12);
	stream->writeU32(PLATFORM_WDGL);
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	stream->writeI32(skin->numBones);
	stream->write(skin->inverseMatrices, skin->numBones*64);
	return stream;
}

int32
getSizeNativeSkin(void *object, int32 offset)
{
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == nil)
		return -1;
	int32 size = 12 + 4 + 4 + skin->numBones*64;
	return size;
}

uint32
skinInstanceCB(Geometry *g, int32 i, uint32 offset)
{
	InstanceDataHeader *header = (InstanceDataHeader*)g->instData;
	AttribDesc *a = &header->attribs[i];
	// weights
	a->index = 4;
	a->type = 2;	/* but also short o_O */
	a->normalized = 1;
	a->size = 4;
	a->offset = offset;
	offset += 4;
	a++;

	// indices
	a->index = 5;
	a->type = 2;
	a->normalized = 0;
	a->size = 4;
	a->offset = offset;
	offset += 4;

	header->dataSize = offset*g->numVertices;
	header->data = new uint8[header->dataSize];

	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(skin == nil)
		return 8;

	a = &header->attribs[i];
	uint8 *wgt = header->data + a[0].offset;
	uint8 *idx = header->data + a[1].offset;
	uint8 *indices = skin->indices;
	float32 *weights = skin->weights;
	for(int32 i = 0; i < g->numVertices; i++){
		packattrib(wgt, weights, a);
		weights += 4;
		wgt += offset;
		idx[0] = *indices++;
		idx[1] = *indices++;
		idx[2] = *indices++;
		idx[3] = *indices++;
		idx += offset;
	}

	return 8;
}

void
skinUninstanceCB(Geometry *geo)
{
	InstanceDataHeader *header = (InstanceDataHeader*)geo->instData;

	Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
	if(skin == nil)
		return;

	uint8 *data = skin->data;
	float *invMats = skin->inverseMatrices;
	skin->init(skin->numBones, skin->numBones, geo->numVertices);
	memcpy(skin->inverseMatrices, invMats, skin->numBones*64);
	delete[] data;

	uint8 *p;
	float *weights = skin->weights;
	uint8 *indices = skin->indices;
	for(int i = 0; i < header->numAttribs; i++){
		AttribDesc *a = &header->attribs[i];
		p = header->data + a->offset;

		switch(a->index){
		case 4:		// weights
			for(int32 i = 0; i < geo->numVertices; i++){
				unpackattrib(weights, p, a);
				weights += 4;
				p += a->stride;
			}
			break;

		case 5:		// indices
			for(int32 i = 0; i < geo->numVertices; i++){
				*indices++ = p[0];
				*indices++ = p[1];
				*indices++ = p[2];
				*indices++ = p[3];
				p += a->stride;
			}
			break;
		}
	}

	skin->findNumWeights(geo->numVertices);
	skin->findUsedBones(geo->numVertices);
}

// Skin

static void*
skinOpen(void *o, int32, int32)
{
	skinGlobals.pipelines[PLATFORM_WDGL] = makeSkinPipeline();
	return o;
}

static void*
skinClose(void *o, int32, int32)
{
	return o;
}

void
initSkin(void)
{
	Driver::registerPlugin(PLATFORM_WDGL, 0, ID_SKIN,
	                       skinOpen, skinClose);
}

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_WDGL);
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	pipe->numCustomAttribs = 2;
	pipe->instanceCB = skinInstanceCB;
	pipe->uninstanceCB = skinUninstanceCB;
	return pipe;
}

// MatFX

static void*
matfxOpen(void *o, int32, int32)
{
	matFXGlobals.pipelines[PLATFORM_WDGL] = makeMatFXPipeline();
	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_WDGL, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_WDGL);
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

// Raster

int32 nativeRasterOffset;

#ifdef RW_OPENGL
struct GlRaster {
	GLuint id;
};

static void*
createNativeRaster(void *object, int32 offset, int32)
{
	GlRaster *raster = PLUGINOFFSET(GlRaster, object, offset);
	raster->id = 0;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	// TODO:
	return object;
}

static void*
copyNativeRaster(void *dst, void *, int32 offset, int32)
{
	GlRaster *raster = PLUGINOFFSET(GlRaster, dst, offset);
	raster->id = 0;
	return dst;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(GlRaster),
	                                            ID_RASTERWDGL,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);
}

void
Texture::upload(void)
{
	GLuint id;
	glGenTextures(1, &id);
	glBindTexture(GL_TEXTURE_2D, id);
	Raster *r = this->raster;
	if(r->palette){
		printf("can't upload paletted raster\n");
		return;
	}

	static GLenum filter[] = {
		0, GL_NEAREST, GL_LINEAR,
		   GL_NEAREST_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_NEAREST,
		   GL_NEAREST_MIPMAP_LINEAR,  GL_LINEAR_MIPMAP_LINEAR
	};
	static GLenum filternomip[] = {
		0, GL_NEAREST, GL_LINEAR,
		   GL_NEAREST, GL_LINEAR,
		   GL_NEAREST, GL_LINEAR
	};
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		filternomip[this->filterAddressing & 0xFF]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		filternomip[this->filterAddressing & 0xFF]);

	static GLenum wrap[] = {
		0, GL_REPEAT, GL_MIRRORED_REPEAT,
		GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER
	};
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
		wrap[(this->filterAddressing >> 8) & 0xF]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
		wrap[(this->filterAddressing >> 12) & 0xF]);

	switch(r->format & 0xF00){
	case Raster::C8888:
		glTexImage2D(GL_TEXTURE_2D, 0, 4, r->width, r->height,
		             0, GL_RGBA, GL_UNSIGNED_BYTE, r->texels);
		break;
	default:
		printf("unsupported raster format: %x\n", r->format);
		break;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	GlRaster *glr = PLUGINOFFSET(GlRaster, r, nativeRasterOffset);
	glr->id = id;
}

void
Texture::bind(int n)
{
	Raster *r = this->raster;
	GlRaster *glr = PLUGINOFFSET(GlRaster, r, nativeRasterOffset);
	glActiveTexture(GL_TEXTURE0+n);
	if(r){
		if(glr->id == 0)
			this->upload();
		glBindTexture(GL_TEXTURE_2D, glr->id);
	}else
		glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

}
#endif

}
}
