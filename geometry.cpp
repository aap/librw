#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rw.h"

using namespace std;

namespace Rw {

Geometry::Geometry(int32 numVerts, int32 numTris, uint32 flags)
{
	this->geoflags = flags & 0xFF00FFFF;
	this->numTexCoordSets = (flags & 0xFF0000) >> 16;
	if(this->numTexCoordSets == 0 && (this->geoflags & TEXTURED))
		this->numTexCoordSets = 1;
	this->numTriangles = numTris;
	this->numVertices = numVerts;
	this->numMorphTargets = 1;

	this->colors = NULL;
	for(int32 i = 0; i < this->numTexCoordSets; i++)
		this->texCoords[i] = NULL;
	this->triangles = NULL;
	if(!(this->geoflags & 0xFF000000)){
		if(this->geoflags & PRELIT)
			this->colors = new uint8[4*this->numVertices];
		if((this->geoflags & TEXTURED) || (this->geoflags & TEXTURED2))
			for(int32 i = 0; i < this->numTexCoordSets; i++)
				this->texCoords[i] =
					new float32[2*this->numVertices];
		this->triangles = new uint16[4*this->numTriangles];
	}
	this->morphTargets = new MorphTarget[1];
	MorphTarget *m = this->morphTargets;
	m->vertices = NULL;
	m->normals = NULL;
	if(!(this->geoflags & 0xFF000000)){
		m->vertices = new float32[3*this->numVertices];
		if(this->geoflags & NORMALS)
			m->normals = new float32[3*this->numVertices];
	}
	this->numMaterials = 0;
	materialList = NULL;
	meshHeader = NULL;

	this->constructPlugins();
}

Geometry::~Geometry(void)
{
	this->destructPlugins();
	delete[] this->colors;
	for(int32 i = 0; i < this->numTexCoordSets; i++)
		delete[] this->texCoords[i];
	delete[] this->triangles;
	for(int32 i = 0; i < this->numMorphTargets; i++){
		MorphTarget *m = &this->morphTargets[i];
		delete[] m->vertices;
		delete[] m->normals;
	}
	delete[] this->morphTargets;
}

struct GeoStreamData
{
	uint32 flags;
	int32 numTriangles;
	int32 numVertices;
	int32 numMorphTargets;
};

Geometry*
Geometry::streamRead(istream &stream)
{
	uint32 version;
	GeoStreamData buf;
	assert(FindChunk(stream, ID_STRUCT, NULL, &version));
	stream.read((char*)&buf, sizeof(buf));
	Geometry *geo = new Geometry(buf.numVertices,
	                             buf.numTriangles, buf.flags);
	geo->addMorphTargets(buf.numMorphTargets-1);
	// skip surface properties
	if(version < 0x34000)
		stream.seekg(12, ios::cur);

	if(!(geo->geoflags & 0xFF000000)){
		if(geo->geoflags & PRELIT)
			stream.read((char*)geo->colors, 4*geo->numVertices);
		if((geo->geoflags & TEXTURED) || (geo->geoflags & TEXTURED2))
			for(int32 i = 0; i < geo->numTexCoordSets; i++)
				stream.read((char*)geo->texCoords[i],
				            2*geo->numVertices*4);
		stream.read((char*)geo->triangles, 4*geo->numTriangles*2);
	}

	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		stream.read((char*)m->boundingSphere, 4*4);
		int32 hasVertices = readInt32(stream);
		int32 hasNormals = readInt32(stream);
		if(hasVertices)
			stream.read((char*)m->vertices, 3*geo->numVertices*4);
		if(hasNormals)
			stream.read((char*)m->normals, 3*geo->numVertices*4);
	}

	assert(FindChunk(stream, ID_MATLIST, NULL, NULL));
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	geo->numMaterials = readInt32(stream);
	geo->materialList = new Material*[geo->numMaterials];
	stream.seekg(geo->numMaterials*4, ios::cur);	// unused (-1)
	for(int32 i = 0; i < geo->numMaterials; i++){
		assert(FindChunk(stream, ID_MATERIAL, NULL, NULL));
		geo->materialList[i] = Material::streamRead(stream);
	}

	geo->streamReadPlugins(stream);

	return geo;
}

static uint32
geoStructSize(Geometry *geo)
{
	uint32 size = 0;
	size += sizeof(GeoStreamData);
	if(Version < 0x34000)
		size += 12;	// surface properties
	if(!(geo->geoflags & 0xFF000000)){
		if(geo->geoflags&geo->PRELIT)
			size += 4*geo->numVertices;
		if((geo->geoflags & geo->TEXTURED) ||
		   (geo->geoflags & geo->TEXTURED2))
			for(int32 i = 0; i < geo->numTexCoordSets; i++)
				size += 2*geo->numVertices*4;
		size += 4*geo->numTriangles*2;
	}
	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		size += 4*4 + 2*4; // bounding sphere and bools
		if(m->vertices)
			size += 3*geo->numVertices*4;
		if(m->normals)
			size += 3*geo->numVertices*4;
	}
	return size;
}

bool
Geometry::streamWrite(ostream &stream)
{
	GeoStreamData buf;
	uint32 size;
	static float32 fbuf[3] = { 1.0f, 1.0f, 1.0f };

	WriteChunkHeader(stream, ID_GEOMETRY, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, geoStructSize(this));

	buf.flags = this->geoflags | this->numTexCoordSets << 16;
	buf.numTriangles = this->numTriangles;
	buf.numVertices = this->numVertices;
	buf.numMorphTargets = this->numMorphTargets;
	stream.write((char*)&buf, sizeof(buf));
	if(Version < 0x34000)
		stream.write((char*)fbuf, sizeof(fbuf));

	if(!(this->geoflags & 0xFF000000)){
		if(this->geoflags & PRELIT)
			stream.write((char*)this->colors, 4*this->numVertices);
		if((this->geoflags & TEXTURED) || (this->geoflags & TEXTURED2))
			for(int32 i = 0; i < this->numTexCoordSets; i++)
				stream.write((char*)this->texCoords[i],
				            2*this->numVertices*4);
		stream.write((char*)this->triangles, 4*this->numTriangles*2);
	}

	for(int32 i = 0; i < this->numMorphTargets; i++){
		MorphTarget *m = &this->morphTargets[i];
		stream.write((char*)m->boundingSphere, 4*4);
		writeInt32(m->vertices != NULL, stream);
		writeInt32(m->normals != NULL, stream);
		if(m->vertices)
			stream.write((char*)m->vertices, 3*this->numVertices*4);
		if(m->normals)
			stream.write((char*)m->normals, 3*this->numVertices*4);
	}

	size = 12 + 4;
	for(int32 i = 0; i < this->numMaterials; i++)
		size += 4 + 12 + this->materialList[i]->streamGetSize();
	WriteChunkHeader(stream, ID_MATLIST, size);
	WriteChunkHeader(stream, ID_STRUCT, 4 + this->numMaterials*4);
	writeInt32(this->numMaterials, stream);
	for(int32 i = 0; i < this->numMaterials; i++)
		writeInt32(-1, stream);
	for(int32 i = 0; i < this->numMaterials; i++)
		this->materialList[i]->streamWrite(stream);

	this->streamWritePlugins(stream);
	return true;
}

uint32
Geometry::streamGetSize(void)
{
	uint32 size = 0;
	size += 12 + geoStructSize(this);
	size += 12 + 12 + 4;
	for(int32 i = 0; i < this->numMaterials; i++)
		size += 4 + 12 + this->materialList[i]->streamGetSize();
	size += 12 + this->streamGetPluginSize();
	return size;
}

void
Geometry::addMorphTargets(int32 n)
{
	if(n == 0)
		return;
	MorphTarget *morphTargets = new MorphTarget[this->numMorphTargets+n];
	memcpy(morphTargets, this->morphTargets,
	       this->numMorphTargets*sizeof(MorphTarget));
	delete[] this->morphTargets;
	this->morphTargets = morphTargets;
	for(int32 i = this->numMorphTargets; i < n; i++){
		MorphTarget *m = &morphTargets[i];
		m->vertices = NULL;
		m->normals = NULL;
		if(!(this->geoflags & 0xFF000000)){
			m->vertices = new float32[3*this->numVertices];
			if(this->geoflags & NORMALS)
				m->normals = new float32[3*this->numVertices];
		}
	}
	this->numMorphTargets += n;
}




Material::Material(void)
{
	this->texture = NULL;
	color[0] = color[1] = color[2] = color[3] = 0xFF;
	surfaceProps[0] = surfaceProps[1] = surfaceProps[2] = 1.0f;
	constructPlugins();
}

Material::Material(Material *m)
{
	m->color[0] = this->color[0];
	m->color[1] = this->color[1];
	m->color[2] = this->color[2];
	m->color[3] = this->color[3];
	m->surfaceProps[0] = this->surfaceProps[0];
	m->surfaceProps[1] = this->surfaceProps[1];
	m->surfaceProps[2] = this->surfaceProps[2];
	copyPlugins(m);
}

Material::~Material(void)
{
	destructPlugins();
}

struct MatStreamData
{
	int32 flags;	// unused according to RW
	uint8 color[4];
	int32 unused;
	int32 textured;
	float32 surfaceProps[3];
};

Material*
Material::streamRead(istream &stream)
{
	uint32 length;
	MatStreamData buf;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	stream.read((char*)&buf, sizeof(buf));
	Material *mat = new Material;
	mat->color[0] = buf.color[0];
	mat->color[1] = buf.color[1];
	mat->color[2] = buf.color[2];
	mat->color[3] = buf.color[3];
	mat->surfaceProps[0] = buf.surfaceProps[0];
	mat->surfaceProps[1] = buf.surfaceProps[1];
	mat->surfaceProps[2] = buf.surfaceProps[2];

	if(buf.textured){
		assert(FindChunk(stream, ID_TEXTURE, &length, NULL));
		mat->texture = Texture::streamRead(stream);
	}

	mat->streamReadPlugins(stream);

	return mat;
}

bool
Material::streamWrite(ostream &stream)
{
	MatStreamData buf;

	WriteChunkHeader(stream, ID_MATERIAL, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, sizeof(MatStreamData));

	buf.color[0] = this->color[0];
	buf.color[1] = this->color[1];
	buf.color[2] = this->color[2];
	buf.color[3] = this->color[3];
	buf.surfaceProps[0] = this->surfaceProps[0];
	buf.surfaceProps[1] = this->surfaceProps[1];
	buf.surfaceProps[2] = this->surfaceProps[2];
	buf.flags = 0;
	buf.unused = 0;
	buf.textured = this->texture != NULL;
	stream.write((char*)&buf, sizeof(buf));

	if(this->texture)
		this->texture->streamWrite(stream);

	this->streamWritePlugins(stream);
	return true;
}

uint32
Material::streamGetSize(void)
{
	uint32 size = 0;
	size += 12 + sizeof(MatStreamData);
	if(this->texture)
		size += 12 + this->texture->streamGetSize();
	size += 12 + this->streamGetPluginSize();
	return size;
}



Texture::Texture(void)
{
	memset(this->name, 0, 32);
	memset(this->mask, 0, 32);
	this->filterAddressing = 0;
	constructPlugins();
}

Texture::Texture(Texture *t)
{
	memcpy(this->name, t->name, 32);
	memcpy(this->mask, t->name, 32);
	this->filterAddressing = t->filterAddressing;
	copyPlugins(t);
}

Texture::~Texture(void)
{
	destructPlugins();
}

Texture*
Texture::streamRead(istream &stream)
{
	uint32 length;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	Texture *tex = new Texture;
	tex->filterAddressing = readUInt16(stream);
	stream.seekg(2, ios::cur);

	assert(FindChunk(stream, ID_STRING, &length, NULL));
	stream.read(tex->name, length);

	assert(FindChunk(stream, ID_STRING, &length, NULL));
	stream.read(tex->mask, length);

	tex->streamReadPlugins(stream);

	return tex;
}

bool
Texture::streamWrite(ostream &stream)
{
	int size;
	WriteChunkHeader(stream, ID_TEXTURE, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, 4);
	writeUInt32(this->filterAddressing, stream);

	size = strnlen(this->name, 32)+3 & ~3;
	if(size < 4)
		size = 4;
	WriteChunkHeader(stream, ID_STRING, size);
	stream.write(this->name, size);

	size = strnlen(this->mask, 32)+3 & ~3;
	if(size < 4)
		size = 4;
	WriteChunkHeader(stream, ID_STRING, size);
	stream.write(this->mask, size);

	this->streamWritePlugins(stream);
	return true;
}

uint32
Texture::streamGetSize(void)
{
	uint32 size = 0;
	int strsize;
	size += 12 + 4;
	size += 12 + 12;
	strsize = strnlen(this->name, 32)+3 & ~3;
	size += strsize < 4 ? 4 : strsize;
	strsize = strnlen(this->mask, 32)+3 & ~3;
	size += strsize < 4 ? 4 : strsize;
	size += 12 + this->streamGetPluginSize();
	return size;
}

}
