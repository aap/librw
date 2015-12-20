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
	float32 uvScale[2];
	uint32 unknown;
	uint32 dmaOffset;
	uint16 numTriangles;
	int16 matID;
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

float32
halfFloat(uint16 half)
{
	uint32 f = (uint32)(half & 0x7fff) << 13;
	uint32 sgn = (uint32)(half & 0x8000) << 16;
	f += 0x38000000;
	if((half & 0x7c00) == 0) f = 0;
	f |= sgn;
	return *(float32*)&f;
}

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

struct RslVertex
{
	float32 p[3];
	float32 n[3];
	float32 t[2];
	uint8   c[4];
	float32 w[4];
	uint8   i[4];
};

void
convertRslMesh(Geometry *g, RslGeometry *rg, Mesh *m, RslMesh *rm)
{
	RslVertex v;
	uint32 mask = 0x1001;	// tex coords, vertices
	if(g->geoflags & Geometry::NORMALS)
		mask |= 0x10;
	if(g->geoflags & Geometry::PRELIT)
		mask |= 0x100;
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
		mask |= 0x10000;
	}

	//float uScale = //halfFloat(rm->uvScale[0]);
	//float vScale = //halfFloat(rm->uvScale[1]);

	int16 *vuVerts = NULL;
	int8 *vuNorms = NULL;
	uint8 *vuTex = NULL;
	uint16 *vuCols = NULL;
	uint32 *vuSkin = NULL;

	uint8 *p = rg->data + rm->dmaOffset;
	uint32 *w = (uint32*)p;
	uint32 *end = (uint32*)(p + ((w[0] & 0xFFFF) + 1)*0x10);
	w += 4;
	int flags = 0;
	int32 nvert;
	bool first = 1;
	while(w < end){
		/* Get data pointers */

		// GIFtag probably
		assert(w[0] == 0x6C018000);	// UNPACK
		nvert = w[4] & 0x7FFF;
		if(!first) nvert -=2;
		w += 5;

		// positions
		assert(w[0] == 0x20000000);	// STMASK
		w += 2;
		assert(w[0] == 0x30000000);	// STROW
		w += 5;
		assert((w[0] & 0xFF004000) == 0x79000000);
		vuVerts = (int16*)(w+1);
		if(!first) vuVerts += 2*3;
		w = skipUnpack(w);

		// tex coords
		assert(w[0] == 0x20000000);	// STMASK
		w += 2;
		assert(w[0] == 0x30000000);	// STROW
		w += 5;
		assert((w[0] & 0xFF004000) == 0x76004000);
		vuTex = (uint8*)(w+1);
		if(!first) vuTex += 2*2;
		w = skipUnpack(w);

		if(g->geoflags & Geometry::NORMALS){
			assert((w[0] & 0xFF004000) == 0x6A000000);
			vuNorms = (int8*)(w+1);
			if(!first) vuNorms += 2*3;
			w = skipUnpack(w);
		}

		if(g->geoflags & Geometry::PRELIT){
			assert((w[0] & 0xFF004000) == 0x6F000000);
			vuCols = (uint16*)(w+1);
			if(!first) vuCols += 2;
			w = skipUnpack(w);
		}

		if(skin){
			assert((w[0] & 0xFF004000) == 0x6C000000);
			vuSkin = w+1;
			if(!first) vuSkin += 2*4;
			w = skipUnpack(w);
		}

		assert(w[0] == 0x14000006);	// MSCAL
		w++;
		while(w[0] == 0) w++;

		/* Insert Data */
		for(int32 i = 0; i < nvert; i++){
			v.p[0] = vuVerts[0]/32768.0f*rg->scale[0] + rg->pos[0];
			v.p[1] = vuVerts[1]/32768.0f*rg->scale[1] + rg->pos[1];
			v.p[2] = vuVerts[2]/32768.0f*rg->scale[2] + rg->pos[2];
			v.t[0] = vuTex[0]/128.0f*rm->uvScale[0];
			v.t[1] = vuTex[1]/128.0f*rm->uvScale[1];
			if(mask & 0x10){
				v.n[0] = vuNorms[0]/127.0f;
				v.n[1] = vuNorms[1]/127.0f;
				v.n[2] = vuNorms[2]/127.0f;
			}
			if(mask & 0x100){
				v.c[0] = (vuCols[0] & 0x1f) * 255 / 0x1F;
				v.c[1] = (vuCols[0]>>5 & 0x1f) * 255 / 0x1F;
				v.c[2] = (vuCols[0]>>10 & 0x1f) * 255 / 0x1F;
				v.c[3] = vuCols[0]&0x8000 ? 0xFF : 0;
			}
			if(mask & 0x10000){
				for(int j = 0; j < 4; j++){
					((uint32*)v.w)[j] = vuSkin[j] & ~0x3FF;
					v.i[j] = vuSkin[j] >> 2;
					//if(v.i[j]) v.i[j]--;
					if(v.w[j] == 0.0f) v.i[j] = 0;
				}
			}

			int32 idx = ps2::findVertexSkin(g, NULL, mask, 0, v.p, v.t, NULL, v.c, v.n, v.w, v.i);
			if(idx < 0)
				idx = g->numVertices++;
			/* Insert mesh joining indices when we get the index of the first vertex
			 * in the first VU chunk of a non-first RslMesh. */
			if(i == 0 && first && rm != &rg->meshes[0] && (rm-1)->matID == rm->matID){
				m->indices[m->numIndices] = m->indices[m->numIndices-1];
				m->numIndices++;
				m->indices[m->numIndices++] = idx;
				if((rm-1)->numTriangles % 2)
					m->indices[m->numIndices++] = idx;
			}
			m->indices[m->numIndices++] = idx;
			ps2::insertVertexSkin(g, idx, mask, v.p, v.t, NULL, v.c, v.n, v.w, v.i);

			vuVerts += 3;
			vuTex += 2;
			vuNorms += 3;
			vuCols++;
			vuSkin += 4;
		}
		first = 0;
	}
}

void
convertRslGeometry(Geometry *g)
{
	RslGeometry *rg = *PLUGINOFFSET(RslGeometry*, g, rslPluginOffset);
	assert(rg != NULL);

	g->meshHeader = new MeshHeader;
	g->meshHeader->flags = 1;
	g->meshHeader->numMeshes = g->numMaterials;
	g->meshHeader->mesh = new Mesh[g->meshHeader->numMeshes];
	g->meshHeader->totalIndices = 0;
	Mesh *meshes = g->meshHeader->mesh;
	for(uint32 i = 0; i < g->meshHeader->numMeshes; i++)
		meshes[i].numIndices = 0;
	RslMesh *rm = rg->meshes;
	int32 lastId = -1;
	for(uint32 i = 0; i < rg->numMeshes; i++, rm++){
		Mesh *m = &meshes[rm->matID];
		g->numVertices += rm->numTriangles+2;
		m->numIndices += rm->numTriangles+2;
		// Extra indices since we're merging tristrip
		// meshes with the same material.
		// Be careful with face winding.
		if(lastId == rm->matID)
			m->numIndices += (rm-1)->numTriangles % 2 ? 3 : 2;
		m->material = g->materialList[rm->matID];
		lastId = rm->matID;
	}
	for(uint32 i = 0; i < g->meshHeader->numMeshes; i++)
		g->meshHeader->totalIndices += meshes[i].numIndices;
	g->geoflags = Geometry::TRISTRIP |
	              Geometry::POSITIONS |	/* 0x01 ? */
	              Geometry::TEXTURED |	/* 0x04 ? */
	              Geometry::LIGHT;
	if(g->hasColoredMaterial())
		g->geoflags |= Geometry::MODULATE;
	if(rg->flags & 0x2)
		g->geoflags |= Geometry::NORMALS;
	if(rg->flags & 0x8)
		g->geoflags |= Geometry::PRELIT;
	g->numTexCoordSets = 1;

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

	for(uint32 i = 0; i < g->meshHeader->numMeshes; i++)
		meshes[i].numIndices = 0;
	g->numVertices = 0;
	rm = rg->meshes;
	for(uint32 i = 0; i < rg->numMeshes; i++, rm++)
		convertRslMesh(g, rg, &meshes[rm->matID], rm);

	if(skin){
		skin->findNumWeights(g->numVertices);
		skin->findUsedBones(g->numVertices);
	}
	g->calculateBoundingSphere();
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
	g->numMaterials = stream->readI32();
	g->materialList = new Material*[g->numMaterials];
	stream->seek(g->numMaterials*4);	// unused (-1)
	for(int32 i = 0; i < g->numMaterials; i++){
		assert(findChunk(stream, ID_MATERIAL, NULL, NULL));
		g->materialList[i] = Material::streamRead(stream);
		// fucked somehow
		g->materialList[i]->surfaceProps[0] = 1.0f;
		g->materialList[i]->surfaceProps[1] = 1.0f;
		g->materialList[i]->surfaceProps[2] = 1.0f;
	}

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
