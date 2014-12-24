#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "rwps2.h"
#include "rwogl.h"

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
readNodeName(istream &stream, int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream.read(name, len);
	name[len] = '\0';
}

static void
writeNodeName(ostream &stream, int32 len, void *object, int32 offset, int32)
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
	Frame::registerPlugin(18, 0x253f2fe, (Constructor)createNodeName,
	                      (Destructor)destroyNodeName,
	                      (CopyConstructor)copyNodeName);
	Frame::registerPluginStream(0x253f2fe, (StreamRead)readNodeName,
	                            (StreamWrite)writeNodeName,
	                            (StreamGetSize)getSizeNodeName);
}

//
// Geometry
//

// Mesh

static void
readMesh(istream &stream, int32 len, void *object, int32, int32)
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
	bool hasData = len > 12+geo->meshHeader->numMeshes*8;
	for(uint32 i = 0; i < geo->meshHeader->numMeshes; i++){
		stream.read((char*)buf, 8);
		mesh->numIndices = buf[0];
		mesh->material = geo->materialList[buf[1]];
		mesh->indices = NULL;
		if(geo->geoflags & Geometry::NATIVE){
			// OpenGL stores uint16 indices here
			if(hasData){
				mesh->indices = new uint16[mesh->numIndices];
				stream.read((char*)mesh->indices,
				            mesh->numIndices*2);
			}
		}else{
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
writeMesh(ostream &stream, int32, void *object, int32, int32)
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
		if(geo->geoflags & Geometry::NATIVE){
			if(mesh->indices)
				stream.write((char*)mesh->indices,
				            mesh->numIndices*2);
		}else{
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
	if(geo->geoflags & Geometry::NATIVE){
		if(geo->meshHeader[0].mesh->indices)
			size += geo->meshHeader->totalIndices*2;
	}else{
		size += geo->meshHeader->totalIndices*4;
	}
	return size;
}


void
registerMeshPlugin(void)
{
	Geometry::registerPlugin(0, 0x50E, NULL, NULL, NULL);
	Geometry::registerPluginStream(0x50E, (StreamRead)readMesh,
	                               (StreamWrite)writeMesh,
	                               (StreamGetSize)getSizeMesh);
}

// Native Data

static void*
destroyNativeData(void *object, int32 offset, int32 size)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return object;
	if(geometry->instData->platform == PLATFORM_PS2)
		return DestroyNativeDataPS2(object, offset, size);
	if(geometry->instData->platform == PLATFORM_OGL)
		return Gl::DestroyNativeData(object, offset, size);
	return object;
}

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
	}else{
		stream.seekg(-12, ios::cur);
		Gl::ReadNativeData(stream, len, object, o, s);
	}
}

static void
writeNativeData(ostream &stream, int32 len, void *object, int32 o, int32 s)
{
	Geometry *geometry = (Geometry*)object;
	if(geometry->instData == NULL)
		return;
	if(geometry->instData->platform == PLATFORM_PS2)
		WriteNativeDataPS2(stream, len, object, o, s);
	else if(geometry->instData->platform == PLATFORM_OGL)
		Gl::WriteNativeData(stream, len, object, o, s);
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
		return Gl::GetSizeNativeData(object, offset, size);
	return -1;
}

void
registerNativeDataPlugin(void)
{
	Geometry::registerPlugin(0, ID_NATIVEDATA,
	                         NULL, destroyNativeData, NULL);
	Geometry::registerPluginStream(ID_NATIVEDATA,
	                               (StreamRead)readNativeData,
	                               (StreamWrite)writeNativeData,
	                               (StreamGetSize)getSizeNativeData);
}

// Breakable Model

// TODO: put this in a header
struct Breakable
{
	uint32 position;
	uint32 numVertices;
	uint32 numFaces;
	uint32 numMaterials;

	float32 *vertices;
	float32 *texCoords;
	uint8   *colors;
	uint16  *faces;
	uint16  *matIDs;
	char    (*texNames)[32];
	char    (*maskNames)[32];
	float32 (*surfaceProps)[3];
};

static void*
destroyBreakableModel(void *object, int32 offset, int32 size)
{
	uint8 *p = *PLUGINOFFSET(uint8*, object, offset);
	delete[] p;
	return object;
}

static void
readBreakableModel(istream &stream, int32 len, void *object, int32 o, int32 s)
{
	uint32 header[13];
	uint32 hasBreakable = readUInt32(stream);
	if(hasBreakable == 0)
		return;
	stream.read((char*)header, 13*4);
	uint32 size = header[1]*(12+8+4) + header[5]*(6+2) +
	              header[8]*(32+32+12);
	uint8 *p = new uint8[sizeof(Breakable)+size];
	Breakable *breakable = (Breakable*)p;
	*PLUGINOFFSET(Breakable*, object, o) = breakable;
	breakable->position     = header[0];
	breakable->numVertices  = header[1];
	breakable->numFaces     = header[5];
	breakable->numMaterials = header[8];
	p += sizeof(Breakable);
	stream.read((char*)p, size);
	breakable->vertices = (float*)p;
	p += breakable->numVertices*12;
	breakable->texCoords = (float*)p;
	p += breakable->numVertices*8;
	breakable->colors = (uint8*)p;
	p += breakable->numVertices*4;
	breakable->faces = (uint16*)p;
	p += breakable->numFaces*6;
	breakable->matIDs = (uint16*)p;
	p += breakable->numFaces*2;
	breakable->texNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->maskNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->surfaceProps = (float32(*)[3])p;
}

static void
writeBreakableModel(ostream &stream, int32 len, void *object, int32 o, int32 s)
{
	uint32 header[13];
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, o);
	uint8 *p = (uint8*)breakable;
	if(breakable == NULL){
		writeUInt32(0, stream);
		return;
	}
	writeUInt32(1, stream);
	memset((char*)header, 0, 13*4);
	header[0] = breakable->position;
	header[1] = breakable->numVertices;
	header[5] = breakable->numFaces;
	header[8] = breakable->numMaterials;
	stream.write((char*)header, 13*4);
	p += sizeof(Breakable);
	stream.write((char*)p, breakable->numVertices*(12+8+4) +
	                       breakable->numFaces*(6+2) +
	                       breakable->numMaterials*(32+32+12));
}

static int32
getSizeBreakableModel(void *object, int32 offset, int32 size)
{
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, offset);
	if(breakable == NULL)
		return 4;
	return 56 + breakable->numVertices*(12+8+4) +
	            breakable->numFaces*(6+2) +
	            breakable->numMaterials*(32+32+12);
}

void
registerBreakableModelPlugin(void)
{
	Geometry::registerPlugin(sizeof(Breakable*), ID_BREAKABLE, NULL,
	                         destroyBreakableModel, NULL);
	Geometry::registerPluginStream(ID_BREAKABLE,
	                               (StreamRead)readBreakableModel,
	                               (StreamWrite)writeBreakableModel,
	                               (StreamGetSize)getSizeBreakableModel);
}
