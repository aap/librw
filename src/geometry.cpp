#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"

using namespace std;

namespace Rw {

Geometry::Geometry(int32 numVerts, int32 numTris, uint32 flags)
{
	this->geoflags = flags & 0xFF00FFFF;
	this->numTexCoordSets = (flags & 0xFF0000) >> 16;
	if(this->numTexCoordSets == 0)
		this->numTexCoordSets = (this->geoflags & TEXTURED)  ? 1 :
		                        (this->geoflags & TEXTURED2) ? 2 : 0;
	this->numTriangles = numTris;
	this->numVertices = numVerts;
	this->numMorphTargets = 1;

	this->colors = NULL;
	for(int32 i = 0; i < this->numTexCoordSets; i++)
		this->texCoords[i] = NULL;
	this->triangles = NULL;
	if(!(this->geoflags & NATIVE)){
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
	if(!(this->geoflags & NATIVE)){
		m->vertices = new float32[3*this->numVertices];
		if(this->geoflags & NORMALS)
			m->normals = new float32[3*this->numVertices];
	}
	this->numMaterials = 0;
	this->materialList = NULL;
	this->meshHeader = NULL;
	this->refCount = 1;

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

	if(this->meshHeader){
		for(uint32 i = 0; i < this->meshHeader->numMeshes; i++)
			delete[] this->meshHeader->mesh[i].indices;
		delete[] this->meshHeader->mesh;
		delete this->meshHeader;
	}

	for(int32 i = 0; i < this->numMaterials; i++)
		this->materialList[i]->decRef();
	delete[] this->materialList;
}

void
Geometry::decRef(void)
{
	this->refCount--;
	if(this->refCount)
		delete this;
}

struct GeoStreamData
{
	uint32 flags;
	int32 numTriangles;
	int32 numVertices;
	int32 numMorphTargets;
};

Geometry*
Geometry::streamRead(Stream *stream)
{
	uint32 version;
	GeoStreamData buf;
	assert(FindChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(&buf, sizeof(buf));
	Geometry *geo = new Geometry(buf.numVertices,
	                             buf.numTriangles, buf.flags);
	geo->addMorphTargets(buf.numMorphTargets-1);
	// skip surface properties
	if(version < 0x34000)
		stream->seek(12);

	if(!(geo->geoflags & NATIVE)){
		if(geo->geoflags & PRELIT)
			stream->read(geo->colors, 4*geo->numVertices);
		for(int32 i = 0; i < geo->numTexCoordSets; i++)
			stream->read(geo->texCoords[i],
				    2*geo->numVertices*4);
		stream->read(geo->triangles, 4*geo->numTriangles*2);
	}

	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		stream->read(m->boundingSphere, 4*4);
		int32 hasVertices = stream->readI32();
		int32 hasNormals = stream->readI32();
		if(hasVertices)
			stream->read(m->vertices, 3*geo->numVertices*4);
		if(hasNormals)
			stream->read(m->normals, 3*geo->numVertices*4);
	}

	assert(FindChunk(stream, ID_MATLIST, NULL, NULL));
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	geo->numMaterials = stream->readI32();
	geo->materialList = new Material*[geo->numMaterials];
	stream->seek(geo->numMaterials*4);	// unused (-1)
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
	if(!(geo->geoflags & Geometry::NATIVE)){
		if(geo->geoflags&geo->PRELIT)
			size += 4*geo->numVertices;
		for(int32 i = 0; i < geo->numTexCoordSets; i++)
			size += 2*geo->numVertices*4;
		size += 4*geo->numTriangles*2;
	}
	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		size += 4*4 + 2*4; // bounding sphere and bools
		if(!(geo->geoflags & Geometry::NATIVE)){
			if(m->vertices)
				size += 3*geo->numVertices*4;
			if(m->normals)
				size += 3*geo->numVertices*4;
		}
	}
	return size;
}

bool
Geometry::streamWrite(Stream *stream)
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
	stream->write(&buf, sizeof(buf));
	if(Version < 0x34000)
		stream->write(fbuf, sizeof(fbuf));

	if(!(this->geoflags & NATIVE)){
		if(this->geoflags & PRELIT)
			stream->write(this->colors, 4*this->numVertices);
		for(int32 i = 0; i < this->numTexCoordSets; i++)
			stream->write(this->texCoords[i],
				    2*this->numVertices*4);
		stream->write(this->triangles, 4*this->numTriangles*2);
	}

	for(int32 i = 0; i < this->numMorphTargets; i++){
		MorphTarget *m = &this->morphTargets[i];
		stream->write(m->boundingSphere, 4*4);
		if(!(this->geoflags & NATIVE)){
			stream->writeI32(m->vertices != NULL);
			stream->writeI32(m->normals != NULL);
			if(m->vertices)
				stream->write(m->vertices,
				             3*this->numVertices*4);
			if(m->normals)
				stream->write(m->normals,
				             3*this->numVertices*4);
		}else{
			stream->writeI32(0);
			stream->writeI32(0);
		}
	}

	size = 12 + 4;
	for(int32 i = 0; i < this->numMaterials; i++)
		size += 4 + 12 + this->materialList[i]->streamGetSize();
	WriteChunkHeader(stream, ID_MATLIST, size);
	WriteChunkHeader(stream, ID_STRUCT, 4 + this->numMaterials*4);
	stream->writeI32(this->numMaterials);
	for(int32 i = 0; i < this->numMaterials; i++)
		stream->writeI32(-1);
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
		if(!(this->geoflags & NATIVE)){
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
	memset(this->color, 0xFF, 4);
	surfaceProps[0] = surfaceProps[1] = surfaceProps[2] = 1.0f;
	this->refCount = 1;
	this->constructPlugins();
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
	m->texture = this->texture;
	if(m->texture)
		m->texture->refCount++;
	this->copyPlugins(m);
}

Material::~Material(void)
{
	this->destructPlugins();
	if(this->texture)
		this->texture->decRef();
}

void
Material::decRef(void)
{
	this->refCount--;
	if(this->refCount)
		delete this;
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
Material::streamRead(Stream *stream)
{
	uint32 length;
	MatStreamData buf;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	stream->read(&buf, sizeof(buf));
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
Material::streamWrite(Stream *stream)
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
	stream->write(&buf, sizeof(buf));

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
	this->filterAddressing = (WRAP << 12) | (WRAP << 8) | NEAREST;
	this->refCount = 1;
	this->constructPlugins();
}

Texture::~Texture(void)
{
	this->destructPlugins();
}

void
Texture::decRef(void)
{
	this->refCount--;
	if(this->refCount)
		delete this;
}

Texture*
Texture::streamRead(Stream *stream)
{
	uint32 length;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	Texture *tex = new Texture;
	tex->filterAddressing = stream->readU16();
	stream->seek(2);

	assert(FindChunk(stream, ID_STRING, &length, NULL));
	stream->read(tex->name, length);

	assert(FindChunk(stream, ID_STRING, &length, NULL));
	stream->read(tex->mask, length);

	tex->streamReadPlugins(stream);

	return tex;
}

bool
Texture::streamWrite(Stream *stream)
{
	int size;
	WriteChunkHeader(stream, ID_TEXTURE, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, 4);
	stream->writeU32(this->filterAddressing);

	// TODO: length can't be > 32
	size = strlen(this->name)+3 & ~3;
	if(size < 4)
		size = 4;
	WriteChunkHeader(stream, ID_STRING, size);
	stream->write(this->name, size);

	size = strlen(this->mask)+3 & ~3;
	if(size < 4)
		size = 4;
	WriteChunkHeader(stream, ID_STRING, size);
	stream->write(this->mask, size);

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
	// TODO: see above
	strsize = strlen(this->name)+3 & ~3;
	size += strsize < 4 ? 4 : strsize;
	strsize = strlen(this->mask)+3 & ~3;
	size += strsize < 4 ? 4 : strsize;
	size += 12 + this->streamGetPluginSize();
	return size;
}

}
