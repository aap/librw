#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"

#define PLUGIN_ID 2

namespace rw {

SurfaceProperties defaultSurfaceProps = { 1.0f, 1.0f, 1.0f };

Geometry*
Geometry::create(int32 numVerts, int32 numTris, uint32 flags)
{
	Geometry *geo = (Geometry*)malloc(s_plglist.size);
	if(geo == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	geo->object.init(Geometry::ID, 0);
	geo->flags = flags & 0xFF00FFFF;
	geo->numTexCoordSets = (flags & 0xFF0000) >> 16;
	if(geo->numTexCoordSets == 0)
		geo->numTexCoordSets = (geo->flags & TEXTURED)  ? 1 :
		                       (geo->flags & TEXTURED2) ? 2 : 0;
	geo->numTriangles = numTris;
	geo->numVertices = numVerts;
	geo->numMorphTargets = 1;

	geo->colors = nil;
	for(int32 i = 0; i < geo->numTexCoordSets; i++)
		geo->texCoords[i] = nil;
	geo->triangles = nil;
	if(!(geo->flags & NATIVE) && geo->numVertices){
		if(geo->flags & PRELIT)
			geo->colors = new uint8[4*geo->numVertices];
		if((geo->flags & TEXTURED) || (geo->flags & TEXTURED2))
			for(int32 i = 0; i < geo->numTexCoordSets; i++)
				geo->texCoords[i] =
					new float32[2*geo->numVertices];
		geo->triangles = new Triangle[geo->numTriangles];
	}
	geo->morphTargets = new MorphTarget[1];
	MorphTarget *m = geo->morphTargets;
	m->boundingSphere.center.set(0.0f, 0.0f, 0.0f);
	m->boundingSphere.radius = 0.0f;
	m->vertices = nil;
	m->normals = nil;
	if(!(geo->flags & NATIVE) && geo->numVertices){
		m->vertices = new float32[3*geo->numVertices];
		if(geo->flags & NORMALS)
			m->normals = new float32[3*geo->numVertices];
	}
	geo->matList.init();
	geo->meshHeader = nil;
	geo->instData = nil;
	geo->refCount = 1;

	s_plglist.construct(geo);
	return geo;
}

void
Geometry::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
		s_plglist.destruct(this);
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
		this->matList.deinit();
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
	SurfaceProperties surfProps;
	MaterialList *ret;
	static SurfaceProperties reset = { 1.0f, 1.0f, 1.0f };

	if(!findChunk(stream, ID_STRUCT, nil, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	stream->read(&buf, sizeof(buf));
	Geometry *geo = Geometry::create(buf.numVertices,
	                                 buf.numTriangles, buf.flags);
	if(geo == nil)
		return nil;
	geo->addMorphTargets(buf.numMorphTargets-1);
	if(version < 0x34000)
		stream->read(&surfProps, 12);

	if(!(geo->flags & NATIVE)){
		if(geo->flags & PRELIT)
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

	if(!findChunk(stream, ID_MATLIST, nil, nil)){
		RWERROR((ERR_CHUNK, "MATLIST"));
		goto fail;
	}
	if(version < 0x34000)
		defaultSurfaceProps = surfProps;
	ret = MaterialList::streamRead(stream, &geo->matList);
	if(version < 0x34000)
		defaultSurfaceProps = reset;
	if(ret == nil)
		goto fail;
	if(s_plglist.streamRead(stream, geo))
		return geo;

fail:
	geo->destroy();
	return nil;
}

static uint32
geoStructSize(Geometry *geo)
{
	uint32 size = 0;
	size += sizeof(GeoStreamData);
	if(version < 0x34000)
		size += 12;	// surface properties
	if(!(geo->flags & Geometry::NATIVE)){
		if(geo->flags&geo->PRELIT)
			size += 4*geo->numVertices;
		for(int32 i = 0; i < geo->numTexCoordSets; i++)
			size += 2*geo->numVertices*4;
		size += 4*geo->numTriangles*2;
	}
	for(int32 i = 0; i < geo->numMorphTargets; i++){
		MorphTarget *m = &geo->morphTargets[i];
		size += 4*4 + 2*4; // bounding sphere and bools
		if(!(geo->flags & Geometry::NATIVE)){
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
	static float32 fbuf[3] = { 1.0f, 1.0f, 1.0f };

	writeChunkHeader(stream, ID_GEOMETRY, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, geoStructSize(this));

	buf.flags = this->flags | this->numTexCoordSets << 16;
	buf.numTriangles = this->numTriangles;
	buf.numVertices = this->numVertices;
	buf.numMorphTargets = this->numMorphTargets;
	stream->write(&buf, sizeof(buf));
	if(version < 0x34000)
		stream->write(fbuf, sizeof(fbuf));

	if(!(this->flags & NATIVE)){
		if(this->flags & PRELIT)
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
		if(!(this->flags & NATIVE)){
			stream->writeI32(m->vertices != nil);
			stream->writeI32(m->normals != nil);
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

	this->matList.streamWrite(stream);

	s_plglist.streamWrite(stream, this);
	return true;
}

uint32
Geometry::streamGetSize(void)
{
	uint32 size = 0;
	size += 12 + geoStructSize(this);
	size += 12 + this->matList.streamGetSize();
	size += 12 + s_plglist.streamGetSize(this);
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
		m->vertices = nil;
		m->normals = nil;
		if(!(this->flags & NATIVE)){
			m->vertices = new float32[3*this->numVertices];
			if(this->flags & NORMALS)
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
	for(int32 i = 0; i < this->matList.numMaterials; i++)
		if(this->matList.materials[i]->color.red != 255 ||
		   this->matList.materials[i]->color.green != 255 ||
		   this->matList.materials[i]->color.blue != 255 ||
		   this->matList.materials[i]->color.alpha != 255)
			return 1;
	return 0;
}

void
Geometry::allocateData(void)
{
	if(this->flags & PRELIT)
		this->colors = new uint8[4*this->numVertices];
	if((this->flags & TEXTURED) || (this->flags & TEXTURED2))
		for(int32 i = 0; i < this->numTexCoordSets; i++)
			this->texCoords[i] =
				new float32[2*this->numVertices];
	MorphTarget *m = this->morphTargets;
	m->vertices = new float32[3*this->numVertices];
	if(this->flags & NORMALS)
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
	assert(header != nil);

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
		if(header->flags == MeshHeader::TRISTRIP){
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
		int32 matid = this->matList.findIndex(m->material);
		if(header->flags == MeshHeader::TRISTRIP)
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

static void
dumpMesh(Mesh *m)
{
	for(int32 i = 0; i < m->numIndices-2; i++) 
//		if(i % 2)
//			printf("%3d %3d %3d\n",
//				m->indices[i+1],
//				m->indices[i],
//				m->indices[i+2]);
//		else
			printf("%d %d %d\n",
				m->indices[i],
				m->indices[i+1],
				m->indices[i+2]);
}

void
Geometry::buildMeshes(void)
{
//dumpMesh(this->meshHeader->mesh);
	delete this->meshHeader;

	Triangle *tri;
	MeshHeader *h = new MeshHeader;
	this->meshHeader = h;
	if((this->flags & Geometry::TRISTRIP) == 0){
		h->flags = 0;
		h->totalIndices = this->numTriangles*3;
		h->numMeshes = this->matList.numMaterials;
		h->mesh = new Mesh[h->numMeshes];
		for(uint32 i = 0; i < h->numMeshes; i++){
			h->mesh[i].material = this->matList.materials[i];
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
	}else
		this->buildTristrips();
}

/* The idea is that even in meshes where winding is not preserved
 * every tristrip starts at an even vertex. So find the start of
 * strips and insert duplicate vertices if necessary. */
void
Geometry::correctTristripWinding(void)
{
	MeshHeader *header = this->meshHeader;
	if(header == nil || header->flags != MeshHeader::TRISTRIP ||
	   this->flags & NATIVE)
		return;
	MeshHeader *newhead = new MeshHeader;
	newhead->flags = header->flags;
	newhead->numMeshes = header->numMeshes;
	newhead->totalIndices = 0;
	newhead->mesh = new Mesh[newhead->numMeshes];
	/* get a temporary working buffer */
	uint16 *indices = new uint16[header->totalIndices*2];

	Mesh *mesh = header->mesh;
	Mesh *newmesh = newhead->mesh;
	for(uint16 i = 0; i < header->numMeshes; i++){
		newmesh->numIndices = 0;
		newmesh->indices = &indices[newhead->totalIndices];
		newmesh->material = mesh->material;

		bool inStrip = 0;
		uint32 j;
		for(j = 0; j < mesh->numIndices-2; j++){
			/* Duplicate vertices indicate end of strip */
			if(mesh->indices[j] == mesh->indices[j+1] ||
			   mesh->indices[j+1] == mesh->indices[j+2])
				inStrip = 0;
			else if(!inStrip){
				/* Entering strip now,
				 * make sure winding is correct */
				inStrip = 1;
				if(newmesh->numIndices % 2){
					newmesh->indices[newmesh->numIndices] =
					  newmesh->indices[newmesh->numIndices-1];
					newmesh->numIndices++;
				}
			}
			newmesh->indices[newmesh->numIndices++] = mesh->indices[j];
		}
		for(; j < mesh->numIndices; j++)
			newmesh->indices[newmesh->numIndices++] = mesh->indices[j];
		newhead->totalIndices += newmesh->numIndices;

		mesh++;
		newmesh++;
	}
	newhead->allocateIndices();
	memcpy(newhead->mesh[0].indices, indices, newhead->totalIndices*2);
	delete[] indices;
	this->meshHeader = newhead;
	delete header;
}

void
Geometry::removeUnusedMaterials(void)
{
	if(this->meshHeader == nil)
		return;
	MeshHeader *mh = this->meshHeader;
	for(uint32 i = 0; i < mh->numMeshes; i++)
		if(mh->mesh[i].indices == nil)
			return;

	int32 *map = new int32[this->matList.numMaterials];
	Material **materials = new Material*[this->matList.numMaterials];
	int32 numMaterials = 0;
	/* Build new material list and map */
	for(uint32 i = 0; i < mh->numMeshes; i++){
		Mesh *m = &mh->mesh[i];
		if(m->numIndices <= 0)
			continue;
		materials[numMaterials] = m->material;
		m->material->refCount++;
		int32 oldid = this->matList.findIndex(m->material);
		map[oldid] = numMaterials;
		numMaterials++;
	}
	for(int32 i = 0; i < this->matList.numMaterials; i++)
		this->matList.materials[i]->destroy();
	free(this->matList.materials);
	this->matList.materials = materials;
	this->matList.space = this->matList.numMaterials;
	this->matList.numMaterials = numMaterials;

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
// MaterialList
//

void
MaterialList::init(void)
{
	this->materials = nil;
	this->numMaterials = 0;
	this->space = 0;
}

void
MaterialList::deinit(void)
{
	if(this->materials){
		for(int32 i = 0; i < this->numMaterials; i++)
			this->materials[i]->destroy();
		free(this->materials);
	}
}

int32
MaterialList::appendMaterial(Material *mat)
{
	Material **ml;
	int32 space;
	if(this->numMaterials >= this->space){
		space = this->space + 20;
		if(this->materials)
			ml = (Material**)realloc(this->materials,
						space*sizeof(Material*));
		else
			ml = (Material**)malloc(space*sizeof(Material*));
		if(ml == nil)
			return -1;
		this->space = space;
		this->materials = ml;
	}
	this->materials[this->numMaterials++] = mat;
	mat->refCount++;
	return this->numMaterials-1;
}

int32
MaterialList::findIndex(Material *mat)
{
	for(int32 i = 0; i < this->numMaterials; i++)
		if(this->materials[i] == mat)
			return i;
	return -1;
}

MaterialList*
MaterialList::streamRead(Stream *stream, MaterialList *matlist)
{
	int32 *indices = nil;
	int32 numMat;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		goto fail;
	}
	matlist->init();
	numMat = stream->readI32();
	if(numMat == 0)
		return matlist;
	matlist->materials = (Material**)malloc(numMat*sizeof(Material*));
	if(matlist->materials == nil)
		goto fail;
	matlist->space = numMat;

	indices = (int32*)malloc(numMat*4);
	stream->read(indices, numMat*4);

	Material *m;
	for(int32 i = 0; i < numMat; i++){
		if(indices[i] >= 0){
			m = matlist->materials[indices[i]];
			m->refCount++;
		}else{
			if(!findChunk(stream, ID_MATERIAL, nil, nil)){
				RWERROR((ERR_CHUNK, "MATERIAL"));
				goto fail;
			}
			m = Material::streamRead(stream);
			if(m == nil)
				goto fail;
		}
		matlist->appendMaterial(m);
		m->destroy();
	}
	free(indices);
	return matlist;
fail:
	free(indices);
	matlist->deinit();
	return nil;
}

bool
MaterialList::streamWrite(Stream *stream)
{
	uint32 size = this->streamGetSize();
	writeChunkHeader(stream, ID_MATLIST, size);
	writeChunkHeader(stream, ID_STRUCT, 4 + this->numMaterials*4);
	stream->writeI32(this->numMaterials);

	int32 idx;
	for(int32 i = 0; i < this->numMaterials; i++){
		idx = -1;
		for(int32 j = i-1; j >= 0; j--)
			if(this->materials[i] == this->materials[j]){
				idx = j;
				break;
			}
		stream->writeI32(idx);
	}
	for(int32 i = 0; i < this->numMaterials; i++){
		for(int32 j = i-1; j >= 0; j--)
			if(this->materials[i] == this->materials[j])
				goto found;
		this->materials[i]->streamWrite(stream);
		found:;
	}
	return true;
}

uint32
MaterialList::streamGetSize(void)
{
	uint32 size = 12 + 4 + this->numMaterials*4;
	for(int32 i = 0; i < this->numMaterials; i++){
		for(int32 j = i-1; j >= 0; j--)
			if(this->materials[i] == this->materials[j])
				goto found;
		size += 12 + this->materials[i]->streamGetSize();
		found:;
	}
	return size;
}

//
// Material
//

Material*
Material::create(void)
{
	Material *mat = (Material*)malloc(s_plglist.size);
	if(mat == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	mat->texture = nil;
	memset(&mat->color, 0xFF, 4);
	mat->surfaceProps = defaultSurfaceProps;
	mat->pipeline = nil;
	mat->refCount = 1;
	s_plglist.construct(mat);
	return mat;
}

Material*
Material::clone(void)
{
	Material *mat = Material::create();
	if(mat == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	mat->color = this->color;
	mat->surfaceProps = this->surfaceProps;
	if(this->texture)
		mat->setTexture(this->texture);
	mat->pipeline = this->pipeline;
	s_plglist.copy(mat, this);
	return mat;
}

void
Material::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
		s_plglist.destruct(this);
		if(this->texture)
			this->texture->destroy();
		free(this);
	}
}

void
Material::setTexture(Texture *tex)
{
	if(this->texture)
		this->texture->destroy();
	if(tex)
		tex->refCount++;
	this->texture = tex;
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

	if(!findChunk(stream, ID_STRUCT, nil, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	stream->read(&buf, sizeof(buf));
	Material *mat = Material::create();
	if(mat == nil)
		return nil;
	mat->color = buf.color;
	if(version < 0x30400)
		mat->surfaceProps = defaultSurfaceProps;
	else
		stream->read(&mat->surfaceProps, sizeof(surfaceProps));
	if(buf.textured){
		if(!findChunk(stream, ID_TEXTURE, &length, nil)){
			RWERROR((ERR_CHUNK, "TEXTURE"));
			goto fail;
		}
		Texture *t = Texture::streamRead(stream);
		mat->setTexture(t);
	}

	materialRights[0] = 0;
	if(!s_plglist.streamRead(stream, mat))
		goto fail;
	if(materialRights[0])
		s_plglist.assertRights(mat, materialRights[0], materialRights[1]);
	return mat;

fail:
	mat->destroy();
	return nil;
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
	buf.textured = this->texture != nil;
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

	s_plglist.streamWrite(stream, this);
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
	size += 12 + s_plglist.streamGetSize(this);
	return size;
}

// Material Rights plugin

static Stream*
readMaterialRights(Stream *stream, int32, void *, int32, int32)
{
	stream->read(materialRights, 8);
//	printf("materialrights: %X %X\n", materialRights[0], materialRights[1]);
	return stream;
}

static Stream*
writeMaterialRights(Stream *stream, int32, void *object, int32, int32)
{
	Material *material = (Material*)object;
	uint32 buffer[2];
	buffer[0] = material->pipeline->pluginID;
	buffer[1] = material->pipeline->pluginData;
	stream->write(buffer, 8);
	return stream;
}

static int32
getSizeMaterialRights(void *object, int32, int32)
{
	Material *material = (Material*)object;
	if(material->pipeline == nil || material->pipeline->pluginID == 0)
		return 0;
	return 8;
}

void
registerMaterialRightsPlugin(void)
{
	Material::registerPlugin(0, ID_RIGHTTORENDER, nil, nil, nil);
	Material::registerPluginStream(ID_RIGHTTORENDER,
	                               readMaterialRights,
	                               writeMaterialRights,
	                               getSizeMaterialRights);
}


}
