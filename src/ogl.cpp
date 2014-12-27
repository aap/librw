#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "rwogl.h"

#ifdef RW_OPENGL
#include <GL/glew.h>
#endif

using namespace std;

namespace Rw {
namespace Gl {

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
//  20303 0 0 0 3	vertices:  3 float
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
DestroyNativeData(void *object, int32, int32)
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
ReadNativeData(Stream *stream, int32, void *object, int32, int32)
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
WriteNativeData(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	stream->writeU32(header->numAttribs);
	stream->write(header->attribs, header->numAttribs*sizeof(AttribDesc));
	stream->write(header->data, header->dataSize);
}

int32
GetSizeNativeData(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	return 4 + header->numAttribs*sizeof(AttribDesc) + header->dataSize;
}

static void
packattrib(uint8 *dst, float32 *src, AttribDesc *a)
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
				i8dst[i] = src[i];
			else if(src[i] > 0.0f)
				i8dst[i] = src[i]*127.0f;
			else
				i8dst[i] = src[i]*128.0f;
		}
		break;

	case 2: // ubyte
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				dst[i] = src[i];
			else
				dst[i] = src[i]*255.0f;
		}
		break;

	case 3: // short
		i16dst = (int16*)dst;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				i16dst[i] = src[i];
			else if(src[i] > 0.0f)
				i16dst[i] = src[i]*32767.0f;
			else
				i16dst[i] = src[i]*32768.0f;
		}
		break;

	case 4: // ushort
		u16dst = (uint16*)dst;
		for(int i = 0; i < a->size; i++){
			if(!a->normalized)
				u16dst[i] = src[i];
			else
				u16dst[i] = src[i]*65535.0f;
		}
		break;
	}
}

// TODO: make pipeline dependent (skin data, night colors)
void
Instance(Atomic *atomic)
{
	Geometry *geo = atomic->geometry;
	InstanceDataHeader *header = new InstanceDataHeader;
	geo->instData = header;
	header->platform = PLATFORM_OGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs = 1 + (geo->numTexCoordSets > 0);
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

	// texCoords, only one set here
	if(geo->numTexCoordSets){
		a->index = 1;
		a->type = 3;
		a->normalized = 0;
		a->size = 2;
		a->offset = offset;
		offset += 4;
		a++;
	}

	if(geo->geoflags & Geometry::NORMALS){
		a->index = 2;
		a->type = 1;
		a->normalized = 1;
		a->size = 3;
		a->offset = offset;
		offset += 4;
		a++;
	}

	if(geo->geoflags & Geometry::PRELIT){
		a->index = 3;
		a->type = 2;
		a->normalized = 1;
		a->size = 4;
		a->offset = offset;
		offset += 4;
		a++;
	}
	// TODO: skin, extra colors; what to do with multiple coords?

	a = header->attribs;
	for(int32 i = 0; i < header->numAttribs; i++)
		a[i].stride = offset;

	header->dataSize = offset*geo->numVertices;
	header->data = new uint8[header->dataSize];
	memset(header->data, 0xFF, header->dataSize);

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
			packattrib(p, texcoord, a);
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
			f[0] = color[0];
			f[1] = color[1];
			f[2] = color[2];
			f[3] = color[3];
			packattrib(p, f, a);
			color += 4;
			p += a->stride;
		}
		a++;
	}
	geo->geoflags |= Geometry::NATIVE;
}

#ifdef RW_OPENGL
void
UploadGeo(Geometry *geo)
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
SetAttribPointers(InstanceDataHeader *inst)
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

}
}
