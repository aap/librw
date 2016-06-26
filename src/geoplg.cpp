#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwplugins.h"
#include "ps2/rwps2.h"
#include "ps2/rwps2plg.h"
#include "d3d/rwxbox.h"
#include "d3d/rwd3d8.h"
#include "d3d/rwd3d9.h"
#include "gl/rwwdgl.h"
#include "gl/rwgl3.h"

#define PLUGIN_ID 2

namespace rw {

// Mesh

static Stream*
readMesh(Stream *stream, int32 len, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	stream->read(buf, 12);
	geo->meshHeader = new MeshHeader;
	geo->meshHeader->flags = buf[0];
	geo->meshHeader->numMeshes = buf[1];
	geo->meshHeader->totalIndices = buf[2];
	geo->meshHeader->mesh = new Mesh[geo->meshHeader->numMeshes];
	Mesh *mesh = geo->meshHeader->mesh;
	bool hasData = len > 12+geo->meshHeader->numMeshes*8;
	uint16 *p = nil;
	if(!(geo->geoflags & Geometry::NATIVE) || hasData)
		p = new uint16[geo->meshHeader->totalIndices];
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		stream->read(buf, 8);
		mesh->numIndices = buf[0];
		mesh->material = geo->materialList[buf[1]];
		mesh->indices = nil;
		if(geo->geoflags & Geometry::NATIVE){
			// OpenGL stores uint16 indices here
			if(hasData){
				mesh->indices = p;
				p += mesh->numIndices;
				stream->read(mesh->indices,
				            mesh->numIndices*2);
			}
		}else{
			mesh->indices = p;
			p += mesh->numIndices;
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				stream->read(indbuf, n*4);
				for(int32 j = 0; j < n; j++)
					ind[j] = indbuf[j];
				ind += n;
			}
		}
		mesh++;
	}
	return stream;
}

static Stream*
writeMesh(Stream *stream, int32, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	buf[0] = geo->meshHeader->flags;
	buf[1] = geo->meshHeader->numMeshes;
	buf[2] = geo->meshHeader->totalIndices;
	stream->write(buf, 12);
	Mesh *mesh = geo->meshHeader->mesh;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		buf[0] = mesh->numIndices;
		buf[1] = findPointer((void*)mesh->material,
		                     (void**)geo->materialList,
		                     geo->numMaterials);
		stream->write(buf, 8);
		if(geo->geoflags & Geometry::NATIVE){
			assert(geo->instData != nil);
			if(geo->instData->platform == PLATFORM_WDGL)
				stream->write(mesh->indices,
				            mesh->numIndices*2);
		}else{
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				for(int32 j = 0; j < n; j++)
					indbuf[j] = ind[j];
				stream->write(indbuf, n*4);
				ind += n;
			}
		}
		mesh++;
	}
	return stream;
}

static int32
getSizeMesh(void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	if(geo->meshHeader == nil)
		return -1;
	int32 size = 12 + geo->meshHeader->numMeshes*8;
	if(geo->geoflags & Geometry::NATIVE){
		assert(geo->instData != nil);
		if(geo->instData->platform == PLATFORM_WDGL)
			size += geo->meshHeader->totalIndices*2;
	}else{
		size += geo->meshHeader->totalIndices*4;
	}
	return size;
}

void
registerMeshPlugin(void)
{
	Geometry::registerPlugin(0, 0x50E, nil, nil, nil);
	Geometry::registerPluginStream(0x50E, readMesh, writeMesh, getSizeMesh);
}

MeshHeader::~MeshHeader(void)
{
	// first mesh holds pointer to all indices
	delete[] this->mesh[0].indices;
	delete[] this->mesh;
}

void
MeshHeader::allocateIndices(void)
{
	uint16 *p = new uint16[this->totalIndices];
	Mesh *mesh = this->mesh;
	for(uint32 i = 0; i < this->numMeshes; i++){
		mesh->indices = p;
		p += mesh->numIndices;
		mesh++;
	}
}

// Native Data

static void*
destroyNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return object;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::destroyNativeData(object, offset, size);
	if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::destroyNativeData(object, offset, size);
	return object;
}

static Stream*
readNativeData(Stream *stream, int32 len, void *object, int32 o, int32 s)
{
	ChunkHeaderInfo header;
	uint32 libid;
	uint32 platform;
	// ugly hack to find out platform
	stream->seek(-4);
	libid = stream->readU32();
	readChunkHeaderInfo(stream, &header);
	if(header.type == ID_STRUCT && 
	   libraryIDPack(header.version, header.build) == libid){
		platform = stream->readU32();
		stream->seek(-16);
		if(platform == PLATFORM_PS2)
			return ps2::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_XBOX)
			return xbox::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_D3D8)
			return d3d8::readNativeData(stream, len, object, o, s);
		else if(platform == PLATFORM_D3D9)
			return d3d9::readNativeData(stream, len, object, o, s);
		else{
			fprintf(stderr, "unknown platform %d\n", platform);
			stream->seek(len);
		}
	}else{
		stream->seek(-12);
		wdgl::readNativeData(stream, len, object, o, s);
	}
	return stream;
}

static Stream*
writeNativeData(Stream *stream, int32 len, void *object, int32 o, int32 s)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return stream;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::writeNativeData(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::writeNativeData(stream, len, object, o, s);
	return stream;
}

static int32
getSizeNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == nil)
		return 0;
	if(geometry->instData->platform == PLATFORM_PS2)
		return ps2::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_WDGL)
		return wdgl::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return xbox::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_D3D8)
		return d3d8::getSizeNativeData(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_D3D9)
		return d3d9::getSizeNativeData(object, offset, size);
	return 0;
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

}
