#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"

using namespace std;

namespace rw {

Geometry*
Geometry::create(int32 numVerts, int32 numTris, uint32 flags)
{
	Geometry *geo = (Geometry*)malloc(PluginBase::s_size);
	assert(geo != NULL);
	geo->object.init(Geometry::ID, 0);
	geo->geoflags = flags & 0xFF00FFFF;
	geo->numTexCoordSets = (flags & 0xFF0000) >> 16;
	if(geo->numTexCoordSets == 0)
		geo->numTexCoordSets = (geo->geoflags & TEXTURED)  ? 1 :
		                       (geo->geoflags & TEXTURED2) ? 2 : 0;
	geo->numTriangles = numTris;
	geo->numVertices = numVerts;
	geo->numMorphTargets = 1;

	geo->colors = NULL;
	for(int32 i = 0; i < geo->numTexCoordSets; i++)
		geo->texCoords[i] = NULL;
	geo->triangles = NULL;
	if(!(geo->geoflags & NATIVE) && geo->numVertices){
		if(geo->geoflags & PRELIT)
			geo->colors = new uint8[4*geo->numVertices];
		if((geo->geoflags & TEXTURED) || (geo->geoflags & TEXTURED2))
			for(int32 i = 0; i < geo->numTexCoordSets; i++)
				geo->texCoords[i] =
					new float32[2*geo->numVertices];
		geo->triangles = new Triangle[geo->numTriangles];
	}
	geo->morphTargets = new MorphTarget[1];
	MorphTarget *m = geo->morphTargets;
	m->boundingSphere.center.set(0.0f, 0.0f, 0.0f);
	m->boundingSphere.radius = 0.0f;
	m->vertices = NULL;
	m->normals = NULL;
	if(!(geo->geoflags & NATIVE) && geo->numVertices){
		m->vertices = new float32[3*geo->numVertices];
		if(geo->geoflags & NORMALS)
			m->normals = new float32[3*geo->numVertices];
	}
	geo->numMaterials = 0;
	geo->materialList = NULL;
	geo->meshHeader = NULL;
	geo->instData = NULL;
	geo->refCount = 1;

	geo->constructPlugins();
	return geo;
}


void
Geometry::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
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
		delete this->meshHeader;
		for(int32 i = 0; i < this->numMaterials; i++)
			this->materialList[i]->destroy();
		delete[] this->materialList;
		free(this);
	}
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
	Geometry *geo = Geometry::create(buf.numVertices,
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
		for(int32 i = 0; i < geo->numTriangles; i++){
			uint32 tribuf[2];
			stream->read(tribuf, 8);
			geo->triangles[i].v[0]  = tribuf[0] >> 16;
			geo->triangles[i].v[1]  = tribuf[0];
			geo->triangles[i].v[2]  = tribuf[1] >> 16;
			geo->triangles[i].matId = tribuf[1];
		}
	}

	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		stream->read(&m->boundingSphere, 4*4);
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
		for(int32 i = 0; i < this->numTriangles; i++){
			uint32 tribuf[2];
			tribuf[0] = this->triangles[i].v[0] << 16 |
			            this->triangles[i].v[1];
			tribuf[1] = this->triangles[i].v[2] << 16 |
			            this->triangles[i].matId;
			stream->write(tribuf, 8);
		}
	}

	for(int32 i = 0; i < this->numMorphTargets; i++){
		MorphTarget *m = &this->morphTargets[i];
		stream->write(&m->boundingSphere, 4*4);
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
		V3d min( 1000000.0f,  1000000.0f,  1000000.0f);
		V3d max(-1000000.0f, -1000000.0f, -1000000.0f);
		float32 *v = m->vertices;
		for(int32 j = 0; j < this->numVertices; j++){
			if(v[0] > max.x) max.x = v[0];
			if(v[0] < min.x) min.x = v[0];
			if(v[1] > max.y) max.y = v[1];
			if(v[1] < min.y) min.y = v[1];
			if(v[2] > max.z) max.z = v[2];
			if(v[2] < min.z) min.z = v[2];
			v += 3;
		}
		m->boundingSphere.center = scale(add(min, max), 1/2.0f);
		max = sub(max, m->boundingSphere.center);
		m->boundingSphere.radius = length(max);
	}
}

bool32
Geometry::hasColoredMaterial(void)
{
	for(int32 i = 0; i < this->numMaterials; i++)
		if(this->materialList[i]->color.red != 255 ||
		   this->materialList[i]->color.green != 255 ||
		   this->materialList[i]->color.blue != 255 ||
		   this->materialList[i]->color.alpha != 255)
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
	this->triangles = new Triangle[this->numTriangles];

	Triangle *tri = this->triangles;
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
				tri->v[0] = m->indices[j+0];
				tri->v[1] = m->indices[j+1 + (j%2)];
				tri->v[2] = m->indices[j+2 - (j%2)];
				tri->matId = matid;
				tri++;
			}
		else
			for(uint32 j = 0; j < m->numIndices-2; j+=3){
				tri->v[0] = m->indices[j+0];
				tri->v[1] = m->indices[j+1];
				tri->v[2] = m->indices[j+2];
				tri->matId = matid;
				tri++;
			}
		adcbits += m->numIndices;
		m++;
	}
}

void
Geometry::buildMeshes(void)
{
	delete this->meshHeader;

	Triangle *tri;
	MeshHeader *h = new MeshHeader;
	this->meshHeader = h;
	if((this->geoflags & Geometry::TRISTRIP) == 0){
		h->flags = 0;
		h->totalIndices = this->numTriangles*3;
		h->numMeshes = this->numMaterials;
		h->mesh = new Mesh[h->numMeshes];
		for(uint32 i = 0; i < h->numMeshes; i++){
			h->mesh[i].material = this->materialList[i];
			h->mesh[i].numIndices = 0;
		}
		// count indices per mesh
		tri = this->triangles;
		for(int32 i = 0; i < this->numTriangles; i++){
			h->mesh[tri->matId].numIndices += 3;
			tri++;
		}
		h->allocateIndices();
		for(uint32 i = 0; i < h->numMeshes; i++)
			h->mesh[i].numIndices = 0;
		// same as above but fill with indices
		tri = this->triangles;
		for(int32 i = 0; i < this->numTriangles; i++){
			uint32 idx = h->mesh[tri->matId].numIndices;
			h->mesh[tri->matId].indices[idx++] = tri->v[0];
			h->mesh[tri->matId].indices[idx++] = tri->v[1];
			h->mesh[tri->matId].indices[idx++] = tri->v[2];
			h->mesh[tri->matId].numIndices = idx;
			tri++;
		}
	}else{
		assert(0 && "can't tristrip\n");
	}
}

// HAS to be called with an existing mesh
void
Geometry::removeUnusedMaterials(void)
{
	if(this->meshHeader == NULL)
		return;
	MeshHeader *mh = this->meshHeader;
	int32 *map = new int32[this->numMaterials];
	Material **matlist = new Material*[this->numMaterials];
	int32 numMaterials = 0;
	/* Build new material list and map */
	for(uint32 i = 0; i < mh->numMeshes; i++){
		Mesh *m = &mh->mesh[i];
		if(m->numIndices <= 0)
			continue;
		matlist[numMaterials] = m->material;
		int32 oldid = findPointer((void*)m->material,
		                          (void**)this->materialList,
		                          this->numMaterials);
		map[oldid] = numMaterials;
		numMaterials++;
	}
	delete[] this->materialList;
	this->materialList = matlist;
	this->numMaterials = numMaterials;
	/* Build new meshes */
	MeshHeader *newmh = new MeshHeader;
	newmh->flags = mh->flags;
	newmh->numMeshes = numMaterials;
	newmh->mesh = new Mesh[newmh->numMeshes];
	newmh->totalIndices = mh->totalIndices;
	Mesh *newm = newmh->mesh;
	for(uint32 i = 0; i < mh->numMeshes; i++){
		Mesh *oldm = &mh->mesh[i];
		if(oldm->numIndices <= 0)
			continue;
		newm->numIndices = oldm->numIndices;
		newm->material = oldm->material;
		newm++;
	}
	newmh->allocateIndices();
	/* Copy indices */
	newm = newmh->mesh;
	for(uint32 i = 0; i < mh->numMeshes; i++){
		Mesh *oldm = &mh->mesh[i];
		if(oldm->numIndices <= 0)
			continue;
		memcpy(newm->indices, oldm->indices,
		       oldm->numIndices*sizeof(*oldm->indices));
		newm++;
	}
	delete this->meshHeader;
	this->meshHeader = newmh;
	/* Remap triangle material IDs */
	for(int32 i = 0; i < this->numTriangles; i++)
		this->triangles[i].matId = map[this->triangles[i].matId];
	delete[] map;
}

//
// Material
//

Material*
Material::create(void)
{
	Material *mat = (Material*)malloc(PluginBase::s_size);
	assert(mat != NULL);
	mat->texture = NULL;
	memset(&mat->color, 0xFF, 4);
	mat->surfaceProps.ambient = 1.0f;
	mat->surfaceProps.specular = 1.0f;
	mat->surfaceProps.diffuse = 1.0f;
	mat->pipeline = NULL;
	mat->refCount = 1;
	mat->constructPlugins();
	return mat;
}

Material*
Material::clone(void)
{
	Material *mat = Material::create();
	mat->color = this->color;
	mat->surfaceProps = this->surfaceProps;
	if(this->texture){
		mat->texture = this->texture;
		mat->texture->refCount++;
	}
	mat->pipeline = this->pipeline;
	mat->copyPlugins(this);
	return mat;
}

void
Material::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
		this->destructPlugins();
		if(this->texture)
			this->texture->destroy();
		free(this);
	}
}

struct MatStreamData
{
	int32 flags;	// unused according to RW
	RGBA  color;
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
	Material *mat = Material::create();
	mat->color = buf.color;
	if(version < 0x30400){
		mat->surfaceProps.ambient = 1.0f;
		mat->surfaceProps.specular = 1.0f;
		mat->surfaceProps.diffuse = 1.0f;
	}else{
		float32 surfaceProps[3];
		stream->read(surfaceProps, sizeof(surfaceProps));
		mat->surfaceProps.ambient = surfaceProps[0];
		mat->surfaceProps.specular = surfaceProps[1];
		mat->surfaceProps.diffuse = surfaceProps[2];
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

	buf.color = this->color;
	buf.flags = 0;
	buf.unused = 0;
	buf.textured = this->texture != NULL;
	stream->write(&buf, sizeof(buf));

	if(rw::version >= 0x30400){
		float32 surfaceProps[3];
		surfaceProps[0] = this->surfaceProps.ambient;
		surfaceProps[1] = this->surfaceProps.specular;
		surfaceProps[2] = this->surfaceProps.diffuse;
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
