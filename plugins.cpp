#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rw.h"
#include "rwps2.h"

using namespace std;
using namespace Rw;

//
// Frame
//

// Node Name

static void*
createNodeName(void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	name[0] = '\0';
	return object;
}

static void*
copyNodeName(void *dst, void *src, int32 offset, int32)
{
	char *dstname = PLUGINOFFSET(char, dst, offset);
	char *srcname = PLUGINOFFSET(char, src, offset);
	strncpy(dstname, srcname, 17);
	return dst;
}

static void*
destroyNodeName(void *object, int32, int32)
{
	return object;
}

static void
readNodeName(istream &stream, Rw::int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream.read(name, len);
	name[len] = '\0';
}

static void
writeNodeName(ostream &stream, Rw::int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream.write(name, len);
}

static int32
getSizeNodeName(void *object, int32 offset)
{
	char *name = PLUGINOFFSET(char, object, offset);
	int32 len = strlen(name);
	return len > 0 ? len : -1;
}


void
registerNodeNamePlugin(void)
{
	Rw::Frame::registerPlugin(18, 0x253f2fe, (Constructor)createNodeName,
	                          (Destructor)destroyNodeName,
	                          (CopyConstructor)copyNodeName);
	Rw::Frame::registerPluginStream(0x253f2fe, (StreamRead)readNodeName,
	                                (StreamWrite)writeNodeName,
	                                (StreamGetSize)getSizeNodeName);
}

//
// Geometry
//

// Mesh

static void
readMesh(istream &stream, Rw::int32, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	stream.read((char*)buf, 12);
	geo->meshHeader = new MeshHeader;
	geo->meshHeader->flags = buf[0];
	geo->meshHeader->numMeshes = buf[1];
	geo->meshHeader->totalIndices = buf[2];
	geo->meshHeader->mesh = new Mesh[geo->meshHeader->numMeshes];
	Mesh *mesh = geo->meshHeader->mesh;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		stream.read((char*)buf, 8);
		mesh->numIndices = buf[0];
		mesh->material = geo->materialList[buf[1]];
		mesh->indices = NULL;
		if(geo->geoflags & Geometry::NATIVE)
			// TODO: compressed indices in OpenGL
			;
		else{
			mesh->indices = new uint16[mesh->numIndices];
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				stream.read((char*)indbuf, n*4);
				for(int32 j = 0; j < n; j++)
					ind[j] = indbuf[j];
				ind += n;
			}
		}
		mesh++;
	}
}

static void
writeMesh(ostream &stream, Rw::int32, void *object, int32, int32)
{
	Geometry *geo = (Geometry*)object;
	int32 indbuf[256];
	uint32 buf[3];
	buf[0] = geo->meshHeader->flags;
	buf[1] = geo->meshHeader->numMeshes;
	buf[2] = geo->meshHeader->totalIndices;
	stream.write((char*)buf, 12);
	Mesh *mesh = geo->meshHeader->mesh;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		buf[0] = mesh->numIndices;
		buf[1] = findPointer((void*)mesh->material,
		                     (void**)geo->materialList,
		                     geo->numMaterials);
		stream.write((char*)buf, 8);
		if(geo->geoflags & Geometry::NATIVE)
			// TODO: compressed indices in OpenGL
			;
		else{
			uint16 *ind = mesh->indices;
			int32 numIndices = mesh->numIndices;
			for(; numIndices > 0; numIndices -= 256){
				int32 n = numIndices < 256 ? numIndices : 256;
				for(int32 j = 0; j < n; j++)
					indbuf[j] = ind[j];
				stream.write((char*)indbuf, n*4);
				ind += n;
			}
		}
		mesh++;
	}
}

static int32
getSizeMesh(void *object, int32)
{
	Geometry *geo = (Geometry*)object;
	if(geo->meshHeader == NULL)
		return -1;
	int32 size = 12 + geo->meshHeader->numMeshes*8;
	if(geo->geoflags & Geometry::NATIVE)
		// TODO: compressed indices in OpenGL
		;
	else{
		size += geo->meshHeader->totalIndices*4;
	}
	return size;
}


void
registerMeshPlugin(void)
{
	Rw::Geometry::registerPlugin(0, 0x50E, NULL, NULL, NULL);
	Rw::Geometry::registerPluginStream(0x50E, (StreamRead)readMesh,
	                                   (StreamWrite)writeMesh,
	                                   (StreamGetSize)getSizeMesh);
}

// Native Data

static void
readNativeData(istream &stream, int32 len, void *object, int32 o, int32 s)
{
	ChunkHeaderInfo header;
	uint32 libid;
	uint32 platform;
	// ugly hack to find out platform
	stream.seekg(-4, ios::cur);
	libid = readUInt32(stream);
	ReadChunkHeaderInfo(stream, &header);
	if(header.type == ID_STRUCT && 
	   LibraryIDPack(header.version, header.build) == libid){
		// must be PS2 or Xbox
		platform = readUInt32(stream);
		stream.seekg(-16, ios::cur);
		if(platform == PLATFORM_PS2)
			ReadNativeDataPS2(stream, len, object, o, s);
		else if(platform == PLATFORM_XBOX)
			stream.seekg(len, ios::cur);
	}else
		// OpenGL
		// doesn't work always, some headers have wrong size
		stream.seekg(len-12, ios::cur);
}

static void
writeNativeData(ostream &stream, int32 len, void *object, int32 o, int32 s)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return;
	if(geometry->instData->platform == PLATFORM_PS2)
		WriteNativeDataPS2(stream, len, object, o, s);
}

static int32
getSizeNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return -1;
	if(geometry->instData->platform == PLATFORM_PS2)
		return GetSizeNativeDataPS2(object, offset, size);
	else if(geometry->instData->platform == PLATFORM_XBOX)
		return -1;
	else if(geometry->instData->platform == PLATFORM_OGL)
		return -1;
	return -1;
}

void
registerNativeDataPlugin(void)
{
	Rw::Geometry::registerPlugin(0, 0x510, NULL, NULL, NULL);
	Rw::Geometry::registerPluginStream(0x510, (StreamRead)readNativeData,
	                                   (StreamWrite)writeNativeData,
	                                   (StreamGetSize)getSizeNativeData);
}
