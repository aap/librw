#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwps2.h"
#include "rwogl.h"
#include "rwxbox.h"
#include "rwd3d8.h"
#include "rwd3d9.h"
#include "mdl.h"

using namespace std;
using namespace rw;

enum {
	ID_RSL = 0xf00d0000
};
int32 rslPluginOffset;

struct RslMesh
{
	float32 bound[4];	// ?
	float32 uvOff[2];	// ?
	uint32 unknown;
	uint32 dmaOffset;
	uint16 numTriangles;
	uint16 matID;
	float32 pos[3];		// ?
};

struct RslGeometry
{
	float32 bound[4];
	uint32 size;
	int32 flags;
	int32 unknown[4];
	float32 scale[3];
	float32 pos[3];

	uint32 numMeshes;
	RslMesh *meshes;
	uint32 dataSize;
	uint8 *data;
};

static uint32
unpackSize(uint32 unpack)
{
	if((unpack&0x6F000000) == 0x6F000000)
		return 2;
	static uint32 size[] = { 32, 16, 8, 16 };
	return ((unpack>>26 & 3)+1)*size[unpack>>24 & 3]/8;
}

int
analyzeDMA(uint8 *p)
{
	uint32 *w = (uint32*)p;
	uint32 *end;
	end = (uint32*)(p + ((w[0] & 0xFFFF) + 1)*0x10);
	w += 4;
	int flags = 0;
	while(w < end){
		if((w[0] & 0x60000000) == 0x60000000){
//			printf("UNPACK %x %x\n", w[0], unpackSize(w[0]));
			printf("UNPACK %x %x %x\n", w[0] & 0x7F004000, w[0], unpackSize(w[0]));
			uint32 type = w[0] & 0x7F004000;
			if(w[0] != 0x6c018000){
				if(type == 0x79000000)
					flags |= 0x1;
				if(type == 0x76004000)
					flags |= 0x10;
				if(type == 0x6a000000)
					flags |= 0x100;
				if(type == 0x6f000000)
					flags |= 0x1000;
				if(type == 0x6c000000)
					flags |= 0x10000;
			}
			int32 n = (w[0] >> 16) & 0xFF;
			p = (uint8*)(w+1);
			p += (n*unpackSize(w[0])+3)&~3;
			w = (uint32*)p;
			continue;
		}
		switch(w[0] & 0x7F000000){
		case 0x20000000:	// STMASK
			printf("STMASK %x\n", w[1]);
			w++;
			break;
		case 0x30000000:
			printf("STROW %x %x %x %x\n", w[1], w[2], w[3], w[4]);
			w+=4;
			break;
		case 0x31000000:
			printf("STCOL %x %x %x %x\n", w[1], w[2], w[3], w[4]);
			w+=4;
			break;
		case 0x14000000:
			printf("MSCAL %x\n", w[0]&0xFFFF);
			return flags;
			break;
		case 0:
			break;
		default:
			printf("vif: %x\n", w[0]);
			break;
		}
		w++;
	}
	return flags;
}

uint32*
skipUnpack(uint32 *p)
{
	int32 n = (p[0] >> 16) & 0xFF;
	return p + (n*unpackSize(p[0])+3 >> 2) + 1;
}

void
unpackVertices(RslGeometry *rg, int16 *data, float32 *verts, int32 n)
{
	while(n--){
		verts[0] = data[0]/32768.0f*rg->scale[0] + rg->pos[0];
		verts[1] = data[1]/32768.0f*rg->scale[1] + rg->pos[1];
		verts[2] = data[2]/32768.0f*rg->scale[2] + rg->pos[2];
		verts += 3;
		data += 3;
	}
}

void
unpackTex(RslMesh *rm, uint8 *data, float32 *tex, int32 n)
{
	while(n--){
		tex[0] = data[0]/255.0f*rm->uvOff[0];
		tex[1] = data[1]/255.0f*rm->uvOff[1];
		tex += 2;
		data += 2;
	}
}

void
unpackNormals(int8 *data, float32 *norms, int32 n)
{
	while(n--){
		norms[0] = data[0]/127.0f;
		norms[1] = data[1]/127.0f;
		norms[2] = data[2]/127.0f;
		norms += 3;
		data += 3;
	}
}

void
unpackColors(uint16 *data, uint8 *cols, int32 n)
{
	while(n--){
		cols[0] = (data[0] & 0x1f) * 255 / 0x1F;
		cols[1] = (data[0]>>5 & 0x1f) * 255 / 0x1F;
		cols[2] = (data[0]>>10 & 0x1f) * 255 / 0x1F;
		cols[3] = data[0]&0x80 ? 0xFF : 0;
		cols += 4;
		data++;
	}
}

void
unpackSkinData(uint32 *data, float32 *weights, uint8 *indices, int32 n)
{
	while(n--){
		for(int j = 0; j < 4; j++){
			((uint32*)weights)[j] = data[j] & ~0x3FF;
			indices[j] = data[j] >> 2;
			if(indices[j]) indices[j]--;
			if(weights[j] == 0.0f) indices[j] = 0;
		}
		weights += 4;
		indices += 4;
		data += 4;
	}
}

void
insertVertices(Geometry *g, RslGeometry *rg, RslMesh *rm)
{
	float32 *verts = &g->morphTargets[0].vertices[g->numVertices*3];
	float32 *norms = &g->morphTargets[0].normals[g->numVertices*3];
	uint8 *cols = &g->colors[g->numVertices*4];
	float32 *texCoords = &g->texCoords[0][g->numVertices*2];
	uint8 *indices = NULL;
	float32 *weights = NULL;
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(skin){
		indices = &skin->indices[g->numVertices*4];
		weights = &skin->weights[g->numVertices*4];
	}

	uint8 *p = rg->data + rm->dmaOffset;
	uint32 *w = (uint32*)p;
	uint32 *end = (uint32*)(p + ((w[0] & 0xFFFF) + 1)*0x10);
	w += 4;
	int flags = 0;
	int32 nvert;
	while(w < end){
		assert(w[0] == 0x6C018000);	// UNPACK
		nvert = w[4] & 0x7FFF;
		w += 5;

		// positions
		assert(w[0] == 0x20000000);	// STMASK
		w += 2;
		assert(w[0] == 0x30000000);	// STROW
		w += 5;
		assert((w[0] & 0xFF004000) == 0x79000000);
		unpackVertices(rg, (int16*)(w+1), verts, nvert);
		w = skipUnpack(w);
		verts += 3*(nvert-2);

		// tex coords
		assert(w[0] == 0x20000000);	// STMASK
		w += 2;
		assert(w[0] == 0x30000000);	// STROW
		w += 5;
		assert((w[0] & 0xFF004000) == 0x76004000);
		unpackTex(rm, (uint8*)(w+1), texCoords, nvert);
		w = skipUnpack(w);
		texCoords += 2*(nvert-2);

		if(g->geoflags & Geometry::NORMALS){
			assert((w[0] & 0xFF004000) == 0x6A000000);
			unpackNormals((int8*)(w+1), norms, nvert);
			w = skipUnpack(w);
			norms += 3*(nvert-2);
		}

		if(g->geoflags & Geometry::PRELIT){
			assert((w[0] & 0xFF004000) == 0x6F000000);
			unpackColors((uint16*)(w+1), cols, nvert);
			w = skipUnpack(w);
			cols += 4*(nvert-2);
		}

		if(skin){
			assert((w[0] & 0xFF004000) == 0x6C000000);
			unpackSkinData((w+1), weights, indices, nvert);
			w = skipUnpack(w);
			indices += 4*(nvert-2);
			weights += 4*(nvert-2);
		}
		g->numVertices += nvert-2;

		assert(w[0] == 0x14000006);
		w++;
		while(w[0] == 0) w++;
	}
	g->numVertices += 2;
}

void
convertRslGeometry(Geometry *g)
{
	RslGeometry *rg = *PLUGINOFFSET(RslGeometry*, g, rslPluginOffset);
	assert(rg != NULL);

	g->meshHeader = new MeshHeader;
	g->meshHeader->flags = 1;
	g->meshHeader->numMeshes = rg->numMeshes;
	g->meshHeader->mesh = new Mesh[rg->numMeshes];
	g->meshHeader->totalIndices = 0;
	int32 nverts = 0;
	RslMesh *rm = rg->meshes;
	Mesh *m = g->meshHeader->mesh;
	for(uint32 i = 0; i < rg->numMeshes; i++, rm++, m++){
		nverts += m->numIndices = rm->numTriangles+2;
		m->material = g->materialList[i];
	}
	g->geoflags = Geometry::TRISTRIP |
	              Geometry::POSITIONS |	/* 0x01 ? */
	              Geometry::TEXTURED;	/* 0x04 ? */
	if(rg->flags & 0x2)
		g->geoflags |= Geometry::NORMALS;
	if(rg->flags & 0x8)
		g->geoflags |= Geometry::PRELIT;
	g->numTexCoordSets = 1;

	g->numVertices = nverts;
	g->meshHeader->totalIndices = nverts;
	g->allocateData();
	g->meshHeader->allocateIndices();

	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	if(rg->flags & 0x10)
		assert(skin);
	if(skin){
		uint8 *data = skin->data;
		float *invMats = skin->inverseMatrices;
		skin->init(skin->numBones, skin->numBones, g->numVertices);
		memcpy(skin->inverseMatrices, invMats, skin->numBones*64);
		delete[] data;
	}

	uint16 idx = 0;
	rm = rg->meshes;
	m = g->meshHeader->mesh;
	g->numVertices = 0;
	for(uint32 i = 0; i < rg->numMeshes; i++, rm++, m++){
		for(uint32 j = 0; j < m->numIndices; j++)
			m->indices[j] = idx++;
		insertVertices(g, rg, rm);
	}

	if(skin){
		skin->findNumWeights(g->numVertices);
		skin->findUsedBones(g->numVertices);
	}
	g->generateTriangles();
}

Geometry*
geometryStreamReadRsl(Stream *stream)
{
	RslGeometry *rg = new RslGeometry;
	stream->read(rg, 0x40);
	rg->numMeshes = rg->size >> 20;
	rg->size &= 0xFFFFF;
	rg->meshes = new RslMesh[rg->numMeshes];
	rg->dataSize = rg->size - 0x40;
	stream->read(rg->meshes, rg->numMeshes*sizeof(RslMesh));
	rg->dataSize -= rg->numMeshes*sizeof(RslMesh);
	rg->data = new uint8[rg->dataSize];
	stream->read(rg->data, rg->dataSize);

	Geometry *g = new Geometry(0, 0, 0);
	*PLUGINOFFSET(RslGeometry*, g, rslPluginOffset) = rg;

	assert(findChunk(stream, ID_MATLIST, NULL, NULL));
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	int32 numMaterials = stream->readI32();
	Material **materialList = new Material*[numMaterials];
	stream->seek(numMaterials*4);	// unused (-1)
	for(int32 i = 0; i < numMaterials; i++){
		assert(findChunk(stream, ID_MATERIAL, NULL, NULL));
		materialList[i] = Material::streamRead(stream);
	}

	g->numMaterials = rg->numMeshes;
	g->materialList = new Material*[g->numMaterials];
	for(int32 i = 0; i < rg->numMeshes; i++){
		g->materialList[i] = materialList[rg->meshes[i].matID];
		g->materialList[i]->refCount++;
		//g->materialList[i] = new Material(materialList[rg->meshes[i].matID]);
	}

	for(int32 i = 0; i < numMaterials; i++)
		materialList[i]->decRef();

	g->streamReadPlugins(stream);

	return g;
}

bool
geometryStreamWriteRsl(Stream *stream, Geometry *g)
{
	uint32 buf[3] = { ID_GEOMETRY, geometryStreamGetSizeRsl(g), 0xf00d };
	stream->write(&buf, 12);

	RslGeometry *rg = *PLUGINOFFSET(RslGeometry*, g, rslPluginOffset);
	assert(rg != NULL);
	rg->size |= rg->numMeshes << 20;
	stream->write(rg, 0x40);
	rg->numMeshes = rg->size >> 20;
	rg->size &= 0xFFFFF;
	stream->write(rg->meshes, rg->numMeshes*sizeof(RslMesh));
	stream->write(rg->data, rg->dataSize);

	uint32 size = 12 + 4;
	for(int32 i = 0; i < g->numMaterials; i++)
		size += 4 + 12 + g->materialList[i]->streamGetSize();
	writeChunkHeader(stream, ID_MATLIST, size);
	writeChunkHeader(stream, ID_STRUCT, 4 + g->numMaterials*4);
	stream->writeI32(g->numMaterials);
	for(int32 i = 0; i < g->numMaterials; i++)
		stream->writeI32(-1);
	for(int32 i = 0; i < g->numMaterials; i++)
		g->materialList[i]->streamWrite(stream);

	g->streamWritePlugins(stream);
	return true;
}

uint32
geometryStreamGetSizeRsl(Geometry *g)
{
	RslGeometry *rg = *PLUGINOFFSET(RslGeometry*, g, rslPluginOffset);
	assert(rg != NULL);

	uint32 size = rg->size;
	size += 12 + 12 + 4;
	for(int32 i = 0; i < g->numMaterials; i++)
		size += 4 + 12 + g->materialList[i]->streamGetSize();
	size += 12 + g->streamGetPluginSize();
	return size;
}

static uint32 atomicRights[2];

Atomic*
atomicStreamReadRsl(Stream *stream, Frame **frameList)
{
	int32 buf[4];
	uint32 version;
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(buf, 16);
	Atomic *atomic = new Atomic;
	atomic->frame = frameList[buf[0]];
	assert(findChunk(stream, ID_GEOMETRY, NULL, &version));
	assert(version == 0x00f00d00);
	atomic->geometry = geometryStreamReadRsl(stream);

	atomicRights[0] = 0;
	atomic->streamReadPlugins(stream);
	if(atomicRights[0])
		atomic->assertRights(atomicRights[0], atomicRights[1]);
	return atomic;
}

bool
atomicStreamWriteRsl(Stream *stream, Atomic *a, Frame **frameList, int32 numFrames)
{
	int32 buf[4] = { 0, 0, 5, 0 };
	Clump *c = a->clump;
	if(c == NULL)
		return false;
	writeChunkHeader(stream, ID_ATOMIC, atomicStreamGetSizeRsl(a));
	buf[0] = findPointer((void*)a->frame, (void**)frameList, numFrames);
	writeChunkHeader(stream, ID_STRUCT, 16);
	stream->write(buf, sizeof(buf));

	geometryStreamWriteRsl(stream, a->geometry);

	a->streamWritePlugins(stream);
	return true;
}

uint32
atomicStreamGetSizeRsl(Atomic *a)
{
	uint32 size = 12 + 12 + 12 + a->streamGetPluginSize();
	size += 16 + geometryStreamGetSizeRsl(a->geometry);
	return size;
}

Clump*
clumpStreamReadRsl(Stream *stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;
	assert(findChunk(stream, ID_STRUCT, &length, &version));
	clump = new Clump;
	stream->read(buf, length);
	clump->numAtomics = buf[0];
	clump->numLights = 0;
	clump->numCameras = 0;

	// Frame list
	Frame **frameList;
	int32 numFrames;
	clump->frameListStreamRead(stream, &frameList, &numFrames);
	clump->parent = (void*)frameList[0];

	Geometry **geometryList = 0;

	// Atomics
	if(clump->numAtomics)
		clump->atomicList = new Atomic*[clump->numAtomics];
	for(int32 i = 0; i < clump->numAtomics; i++){
		assert(findChunk(stream, ID_ATOMIC, NULL, NULL));
		clump->atomicList[i] = atomicStreamReadRsl(stream, frameList);
		clump->atomicList[i]->clump = clump;
	}

	delete[] frameList;

	clump->streamReadPlugins(stream);
	return clump;
}

bool
clumpStreamWriteRsl(Stream *stream, Clump *c)
{
	int size = clumpStreamGetSizeRsl(c);
	writeChunkHeader(stream, ID_CLUMP, size);
	int buf[3] = { c->numAtomics, 0, 0 };
	writeChunkHeader(stream, ID_STRUCT, 4);
	stream->write(buf, 4);

	int32 numFrames = ((Frame*)c->parent)->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList((Frame*)c->parent, flist);

	c->frameListStreamWrite(stream, flist, numFrames);

	for(int32 i = 0; i < c->numAtomics; i++)
		atomicStreamWriteRsl(stream, c->atomicList[i], flist, numFrames);

	delete[] flist;

	c->streamWritePlugins(stream);
	return true;
}

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += f->streamGetPluginSize();
	f->forAllChildren(sizeCB, size);
	return f;
}

uint32
clumpStreamGetSizeRsl(Clump *c)
{
	uint32 size = 0;
	size += 12;	// Struct
	size += 4;	// numAtomics

	// frame list
	// TODO: make this into a function
	int32 numFrames = ((Frame*)c->parent)->count();
	size += 12 + 12 + 4 + numFrames*(56+12);
	sizeCB((Frame*)c->parent, (void*)&size);

	// atomics
	for(int32 i = 0; i < c->numAtomics; i++)
		size += 12 + atomicStreamGetSizeRsl(c->atomicList[i]);

	size += 12 + c->streamGetPluginSize();
	return size;
}


static void*
createRslGeo(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(RslGeometry*, object, offset) = NULL;
	return object;
}

static void*
destroyRslGeo(void *object, int32 offset, int32)
{
	RslGeometry *rg = *PLUGINOFFSET(RslGeometry*, object, offset);
	delete rg->data;
	delete rg->meshes;
	delete rg;
	*PLUGINOFFSET(RslGeometry*, object, offset) = NULL;
	return object;
}

static void*
copyRslGeo(void *dst, void *src, int32 offset, int32)
{
	// TODO
	return dst;
}

void
registerRslPlugin(void)
{
	rslPluginOffset = Geometry::registerPlugin(sizeof(RslGeometry*), ID_RSL,
	                                            createRslGeo,
                                                    destroyRslGeo,
                                                    copyRslGeo);
}
