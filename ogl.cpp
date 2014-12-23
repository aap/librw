#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rw.h"
#include "rwogl.h"

using namespace std;

namespace Rw {

void*
DestroyNativeDataOGL(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	OGLInstanceDataHeader *header =
		(OGLInstanceDataHeader*)geometry->instData;
	delete[] header->attribs;
	delete[] header->data;
	delete header;
	return object;
}

void
ReadNativeDataOGL(istream &stream, int32, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	OGLInstanceDataHeader *header = new OGLInstanceDataHeader;
	geometry->instData = header;
	header->platform = PLATFORM_OGL;
	header->numAttribs = readUInt32(stream);
	header->attribs = new OGLAttrib[header->numAttribs];
	stream.read((char*)header->attribs,
	            header->numAttribs*sizeof(OGLAttrib));
	// Any better way to find out the size? (header length can be wrong)
	header->dataSize = header->attribs[0].stride*geometry->numVertices;
	header->data = new uint8[header->dataSize];
	stream.read((char*)header->data, header->dataSize);
}

void
WriteNativeDataOGL(ostream &stream, int32 len, void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	OGLInstanceDataHeader *header =
		(OGLInstanceDataHeader*)geometry->instData;
	writeUInt32(header->numAttribs, stream);
	stream.write((char*)header->attribs,
	             header->numAttribs*sizeof(OGLAttrib));
	stream.write((char*)header->data, header->dataSize);
}

int32
GetSizeNativeDataOGL(void *object, int32, int32)
{
	Geometry *geometry = (Geometry*)object;
	assert(geometry->instData->platform == PLATFORM_OGL);
	OGLInstanceDataHeader *header =
		(OGLInstanceDataHeader*)geometry->instData;
	return 4 + header->numAttribs*sizeof(OGLAttrib) + header->dataSize;
}

}
