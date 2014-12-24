#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "rwogl.h"

#include <GL/glew.h>

using namespace std;

namespace Rw {
namespace Gl {

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
ReadNativeData(istream &stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	InstanceDataHeader *header = new InstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_OGL;
	header->vbo = 0;
	header->ibo = 0;
	header->numAttribs = readUInt32(stream);
	header->attribs = new AttribDesc[header->numAttribs];
	stream.read((char*)header->attribs,
	            header->numAttribs*sizeof(AttribDesc));
	// Any better way to find out the size? (header length can be wrong)
	header->dataSize = header->attribs[0].stride*geometry->numVertices;
	header->data = new uint8[header->dataSize];
	stream.read((char*)header->data, header->dataSize);
}

void
WriteNativeData(ostream &stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	InstanceDataHeader *header =
		(InstanceDataHeader*)geometry->instData;
	writeUInt32(header->numAttribs, stream);
	stream.write((char*)header->attribs,
	             header->numAttribs*sizeof(AttribDesc));
	stream.write((char*)header->data, header->dataSize);
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


void
UploadGeo(Geometry *geo)
{
	using namespace Rw;
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
	using namespace Rw;
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

}
}
