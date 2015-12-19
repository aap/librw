#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"

using namespace std;

namespace rw {

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
	if(!(this->geoflags & NATIVE) && this->numVertices){
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
	if(!(this->geoflags & NATIVE) && this->numVertices){
		m->vertices = new float32[3*this->numVertices];
		if(this->geoflags & NORMALS)
			m->normals = new float32[3*this->numVertices];
	}
	this->numMaterials = 0;
	this->materialList = NULL;
	this->meshHeader = NULL;
	this->instData = NULL;
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
		// first mesh holds pointer to all indices
		delete[] this->meshHeader->mesh[0].indices;
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
	if(this->refCount == 0)
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
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
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

	assert(findChunk(stream, ID_MATLIST, NULL, NULL));
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	geo->numMaterials = stream->readI32();
	geo->materialList = new Material*[geo->numMaterials];
	stream->seek(geo->numMaterials*4);	// unused (-1)
	for(int32 i = 0; i < geo->numMaterials; i++){
		assert(findChunk(stream, ID_MATERIAL, NULL, NULL));
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
	if(version < 0x34000)
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

	writeChunkHeader(stream, ID_GEOMETRY, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, geoStructSize(this));

	buf.flags = this->geoflags | this->numTexCoordSets << 16;
	buf.numTriangles = this->numTriangles;
	buf.numVertices = this->numVertices;
	buf.numMorphTargets = this->numMorphTargets;
	stream->write(&buf, sizeof(buf));
	if(version < 0x34000)
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
	writeChunkHeader(stream, ID_MATLIST, size);
	writeChunkHeader(stream, ID_STRUCT, 4 + this->numMaterials*4);
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

void
Geometry::calculateBoundingSphere(void)
{
	for(int32 i = 0; i < this->numMorphTargets; i++){
		MorphTarget *m = &this->morphTargets[i];
		float32 min[3] = {  1000000.0f,  1000000.0f,  1000000.0f };
		float32 max[3] = { -1000000.0f, -1000000.0f, -1000000.0f };
		float32 *v = m->vertices;
		for(int32 j = 0; j < this->numVertices; j++){
			if(v[0] > max[0]) max[0] = v[0];
			if(v[0] < min[0]) min[0] = v[0];
			if(v[1] > max[1]) max[1] = v[1];
			if(v[1] < min[1]) min[1] = v[1];
			if(v[2] > max[2]) max[2] = v[2];
			if(v[2] < min[2]) min[2] = v[2];
			v += 3;
		}
		m->boundingSphere[0] = (min[0] + max[0])/2.0f;
		m->boundingSphere[1] = (min[1] + max[1])/2.0f;
		m->boundingSphere[2] = (min[2] + max[2])/2.0f;
		max[0] -= m->boundingSphere[0];
		max[1] -= m->boundingSphere[1];
		max[2] -= m->boundingSphere[2];
		m->boundingSphere[3] = sqrt(max[0]*max[0] + max[1]*max[1] + max[2]*max[2]);
	}
}

bool32
Geometry::hasColoredMaterial(void)
{
	for(int32 i = 0; i < this->numMaterials; i++)
		if(this->materialList[i]->color[0] != 255 ||
		   this->materialList[i]->color[1] != 255 ||
		   this->materialList[i]->color[2] != 255 ||
		   this->materialList[i]->color[3] != 255)
			return 1;
	return 0;
}

void
Geometry::allocateData(void)
{
	if(this->geoflags & PRELIT)
		this->colors = new uint8[4*this->numVertices];
	if((this->geoflags & TEXTURED) || (this->geoflags & TEXTURED2))
		for(int32 i = 0; i < this->numTexCoordSets; i++)
			this->texCoords[i] =
				new float32[2*this->numVertices];
	MorphTarget *m = this->morphTargets;
	m->vertices = new float32[3*this->numVertices];
	if(this->geoflags & NORMALS)
		m->normals = new float32[3*this->numVertices];
	// TODO: morph targets (who cares anyway?)
}

static int
isDegenerate(uint16 *idx)
{
	return idx[0] == idx[1] ||
	       idx[0] == idx[2] ||
	       idx[1] == idx[2];
}

void
Geometry::generateTriangles(int8 *adc)
{
	MeshHeader *header = this->meshHeader;
	assert(header != NULL);

	this->numTriangles = 0;
	Mesh *m = header->mesh;
	int8 *adcbits = adc;
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(m->numIndices < 3){
			// shouldn't happen but it does
			adcbits += m->numIndices;
			m++;
			continue;
		}
		if(header->flags == 1){	// tristrip
			for(uint32 j = 0; j < m->numIndices-2; j++){
				if(!(adc && adcbits[j+2]) &&
				   !isDegenerate(&m->indices[j]))
					this->numTriangles++;
			}
		}else
			this->numTriangles += m->numIndices/3;
		adcbits += m->numIndices;
		m++;
	}

	delete[] this->triangles;
	this->triangles = new uint16[4*this->numTriangles];

	uint16 *f = this->triangles;
	m = header->mesh;
	adcbits = adc;
	for(uint32 i = 0; i < header->numMeshes; i++){
		if(m->numIndices < 3){
			adcbits += m->numIndices;
			m++;
			continue;
		}
		int32 matid = findPointer((void*)m->material,
		                          (void**)this->materialList,
		                          this->numMaterials);
		if(header->flags == 1)	// tristrip
			for(uint32 j = 0; j < m->numIndices-2; j++){
				if(adc && adcbits[j+2] ||
				   isDegenerate(&m->indices[j]))
					continue;
				*f++ = m->indices[j+1 + (j%2)];
				*f++ = m->indices[j+0];
				*f++ = matid;
				*f++ = m->indices[j+2 - (j%2)];
			}
		else
			for(uint32 j = 0; j < m->numIndices-2; j+=3){
				*f++ = m->indices[j+1];
				*f++ = m->indices[j+0];
				*f++ = matid;
				*f++ = m->indices[j+2];
			}
		adcbits += m->numIndices;
		m++;
	}
}

//
// Material
//

Material::Material(void)
{
	this->texture = NULL;
	memset(this->color, 0xFF, 4);
	surfaceProps[0] = surfaceProps[1] = surfaceProps[2] = 1.0f;
	this->pipeline = NULL;
	this->refCount = 1;
	this->constructPlugins();
}

Material::Material(Material *m)
{
	this->color[0] = m->color[0];
	this->color[1] = m->color[1];
	this->color[2] = m->color[2];
	this->color[3] = m->color[3];
	this->surfaceProps[0] = m->surfaceProps[0];
	this->surfaceProps[1] = m->surfaceProps[1];
	this->surfaceProps[2] = m->surfaceProps[2];
	this->texture = m->texture;
	if(this->texture)
		this->texture->refCount++;
	this->pipeline = m->pipeline;
	this->refCount = 1;
	this->constructPlugins();
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
	if(this->refCount == 0)
		delete this;
}

struct MatStreamData
{
	int32 flags;	// unused according to RW
	uint8 color[4];
	int32 unused;
	int32 textured;
};

static uint32 materialRights[2];

Material*
Material::streamRead(Stream *stream)
{
	uint32 length, version;
	MatStreamData buf;

	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(&buf, sizeof(buf));
	Material *mat = new Material;
	mat->color[0] = buf.color[0];
	mat->color[1] = buf.color[1];
	mat->color[2] = buf.color[2];
	mat->color[3] = buf.color[3];
	if(version < 0x30400){
		mat->surfaceProps[0] = 1.0f;
		mat->surfaceProps[1] = 1.0f;
		mat->surfaceProps[2] = 1.0f;
	}else{
		float32 surfaceProps[3];
		stream->read(surfaceProps, sizeof(surfaceProps));
		mat->surfaceProps[0] = surfaceProps[0];
		mat->surfaceProps[1] = surfaceProps[1];
		mat->surfaceProps[2] = surfaceProps[2];
	}
	if(buf.textured){
		assert(findChunk(stream, ID_TEXTURE, &length, NULL));
		mat->texture = Texture::streamRead(stream);
	}

	materialRights[0] = 0;
	mat->streamReadPlugins(stream);
	if(materialRights[0])
		mat->assertRights(materialRights[0], materialRights[1]);
	return mat;
}

bool
Material::streamWrite(Stream *stream)
{
	MatStreamData buf;

	writeChunkHeader(stream, ID_MATERIAL, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, sizeof(MatStreamData)
		+ (rw::version >= 0x30400 ? 12 : 0));

	buf.color[0] = this->color[0];
	buf.color[1] = this->color[1];
	buf.color[2] = this->color[2];
	buf.color[3] = this->color[3];
	buf.flags = 0;
	buf.unused = 0;
	buf.textured = this->texture != NULL;
	stream->write(&buf, sizeof(buf));

	if(rw::version >= 0x30400){
		float32 surfaceProps[3];
		surfaceProps[0] = this->surfaceProps[0];
		surfaceProps[1] = this->surfaceProps[1];
		surfaceProps[2] = this->surfaceProps[2];
		stream->write(surfaceProps, sizeof(surfaceProps));
	}

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
	if(rw::version >= 0x30400)
		size += 12;
	if(this->texture)
		size += 12 + this->texture->streamGetSize();
	size += 12 + this->streamGetPluginSize();
	return size;
}

// Material Rights plugin

static void
readMaterialRights(Stream *stream, int32, void *, int32, int32)
{
	stream->read(materialRights, 8);
//	printf("materialrights: %X %X\n", materialRights[0], materialRights[1]);
}

static void
writeMaterialRights(Stream *stream, int32, void *object, int32, int32)
{
	Material *material = (Material*)object;
	uint32 buffer[2];
	buffer[0] = material->pipeline->pluginID;
	buffer[1] = material->pipeline->pluginData;
	stream->write(buffer, 8);
}

static int32
getSizeMaterialRights(void *object, int32, int32)
{
	Material *material = (Material*)object;
	if(material->pipeline == NULL || material->pipeline->pluginID == 0)
		return -1;
	return 8;
}

void
registerMaterialRightsPlugin(void)
{
	Material::registerPlugin(0, ID_RIGHTTORENDER, NULL, NULL, NULL);
	Material::registerPluginStream(ID_RIGHTTORENDER,
	                               readMaterialRights,
	                               writeMaterialRights,
	                               getSizeMaterialRights);
}


}
