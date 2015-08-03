#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwogl.h"

#ifdef RW_OPENGL
#include <GL/glew.h>
#endif

using namespace std;

namespace rw {
namespace gl {

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

void*
destroyNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	delete[] header->attribs;
	delete[] header->data;
	delete header;
	return object;
}

void
readNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_OGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs = stream->readU32();
	header->attribs = new AttribDesc[header->numAttribs];
	stream->read(header->attribs,
	            header->numAttribs*sizeof(AttribDesc));
	header->dataSize = header->attribs[0].stride*geometry->numVertices;
	header->data = new uint8[header->dataSize];
	stream->read(header->data, header->dataSize);
}

void
printPipeinfo(Atomic *a)
{
	Geometry *g = a->geometry;
	if(g->instData == NULL || g->instData->platform != PLATFORM_OGL)
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

void
writeNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	stream->writeU32(header->numAttribs);
	stream->write(header->attribs, header->numAttribs*sizeof(AttribDesc));
	stream->write(header->data, header->dataSize);
}

int32
getSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header = (InstanceDataHeader*)geometry->instData;
	return 4 + header->numAttribs*sizeof(AttribDesc) + header->dataSize;
}

static void
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

ObjPipeline::ObjPipeline(uint32 platform)
 : rw::ObjPipeline(platform), numCustomAttribs(0), instanceCB(NULL) { }

void
ObjPipeline::instance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	if(geo->geoflags & Geometry::NATIVE)
		return;
	InstanceDataHeader *header = new InstanceDataHeader;
	geo->instData = header;
	header->platform = PLATFORM_OGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs =
		this->numCustomAttribs + 1 + (geo->numTexCoordSets > 0);
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

	if(this->instanceCB)
		offset += this->instanceCB(geo, firstCustom, offset);
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

ObjPipeline*
makeDefaultPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_OGL);
	return pipe;
}

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

// Skin

void
readNativeSkin(Stream *stream, int32, void *object, int32 offset)
{
	uint8 header[4];
	uint32 vers;
	Geometry *geometry = (Geometry*)object;
	assert(findChunk(stream, ID_STRUCT, NULL, &vers));
	assert(stream->readU32() == PLATFORM_OGL);
	stream->read(header, 4);
	Skin *skin = new Skin;
	*PLUGINOFFSET(Skin*, geometry, offset) = skin;
	skin->numBones = header[0];

	// should be 0
	skin->numUsedBones = header[1];
	skin->maxIndex = header[2];
	assert(skin->numUsedBones == 0);
	assert(skin->maxIndex == 0);

	int32 size = skin->numBones*64 + 15;
	uint8 *data = new uint8[size];
	skin->data = data;
	skin->indices = NULL;
	skin->weights = NULL;
	skin->usedBones = NULL;

	uintptr ptr = (uintptr)data + 15;
	ptr &= ~0xF;
	data = (uint8*)ptr;
	skin->inverseMatrices = NULL;
	if(skin->numBones){
		skin->inverseMatrices = (float*)data;
		stream->read(skin->inverseMatrices, skin->numBones*64);
	}
}

void
writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset)
{
	uint8 header[4];

	writeChunkHeader(stream, ID_STRUCT, len-12);
	stream->writeU32(PLATFORM_OGL);
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	header[0] = skin->numBones;
	header[1] = 0;
	header[2] = 0;
	header[3] = 0;
	stream->write(header, 4);
	stream->write(skin->inverseMatrices, skin->numBones*64);
}

int32
getSizeNativeSkin(void *object, int32 offset)
{
	Skin *skin = *PLUGINOFFSET(Skin*, object, offset);
	if(skin == NULL)
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
        if(skin == NULL)
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

ObjPipeline*
makeSkinPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_OGL);
	pipe->pluginID = ID_SKIN;
	pipe->pluginData = 1;
	pipe->numCustomAttribs = 2;
	pipe->instanceCB = skinInstanceCB;
	return pipe;
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_OGL);
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
	                                            0x12340001, 
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
