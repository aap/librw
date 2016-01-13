#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <args.h>
#include <rw.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;
#include "rsl.h"

char *argv0;

void
RslStream::relocate(void)
{
	uint32 off = (uint32)(uintptr)this->data;
	off -= 0x20;
	*(uint32*)&this->reloc += off;
	*(uint32*)&this->hashTab += off;

	uint8 **rel = (uint8**)this->reloc;
	for(uint32 i = 0; i < this->relocSize; i++){
		rel[i] += off;
		*this->reloc[i] += off;
	}
}



RslFrame *dumpFrameCB(RslFrame *frame, void *data)
{
	printf(" frm: %x %s %x\n", frame->nodeId, frame->name, frame->hierId);
	RslFrameForAllChildren(frame, dumpFrameCB, data);
	return frame;
}

RslMaterial *dumpMaterialCB(RslMaterial *material, void*)
{
	printf("  mat: %d %d %d %d %s %x\n", material->color.red, material->color.green, material->color.blue, material->color.alpha,
		material->texname, material->refCount);
	if(material->matfx){
		RslMatFX *fx = material->matfx;
		printf("   matfx: %d", fx->effectType);
		if(fx->effectType == 2)
			printf("env[%s %f] ", fx->env.texname, fx->env.intensity);
		printf("\n");
	}
	return material;
}

RslAtomic *dumpAtomicCB(RslAtomic *atomic, void*)
{
	printf(" atm: %x %x %x %p\n", atomic->unk1, atomic->unk2, atomic->unk3, atomic->hier);
	RslGeometry *g = atomic->geometry;
	RslGeometryForAllMaterials(g, dumpMaterialCB, NULL);
	return atomic;
}

int32
mapID(int32 id)
{
	if(id == 255) return -1;
	if(id > 0x80) id |= 0x1300;
	return id;
}

Frame*
convertFrame(RslFrame *f)
{
	Frame *rwf = Frame::create();
	rwf->matrix[0] =  f->modelling.right.x;
	rwf->matrix[1] =  f->modelling.right.y;
	rwf->matrix[2] =  f->modelling.right.z;
	rwf->matrix[4] =  f->modelling.up.x;
	rwf->matrix[5] =  f->modelling.up.y;
	rwf->matrix[6] =  f->modelling.up.z;
	rwf->matrix[8] =  f->modelling.at.x;
	rwf->matrix[9] =  f->modelling.at.y;
	rwf->matrix[10] = f->modelling.at.z;
	rwf->matrix[12] = f->modelling.pos.x;
	rwf->matrix[13] = f->modelling.pos.y;
	rwf->matrix[14] = f->modelling.pos.z;

	if(f->name)
		strncpy(gta::getNodeName(rwf), f->name, 24);

	HAnimData *hanim = PLUGINOFFSET(HAnimData, rwf, hAnimOffset);
	hanim->id = f->nodeId;
	if(f->hier){
		HAnimHierarchy *hier;
		hanim->hierarchy = hier = new HAnimHierarchy;
		hier->numNodes = f->hier->numNodes;
		hier->flags = f->hier->flags;
		hier->maxInterpKeyFrameSize = f->hier->maxKeyFrameSize;
		hier->parentFrame = rwf;
		hier->parentHierarchy = hier;
		hier->nodeInfo = new HAnimNodeInfo[hier->numNodes];
		for(int32 i = 0; i < hier->numNodes; i++){
			hier->nodeInfo[i].id = mapID((uint8)f->hier->pNodeInfo[i].id);
			hier->nodeInfo[i].index = f->hier->pNodeInfo[i].index;
			hier->nodeInfo[i].flags = f->hier->pNodeInfo[i].flags;
			hier->nodeInfo[i].frame = NULL;
		}
	}
	return rwf;
}

Texture*
convertTexture(RslTexture *t)
{
	Texture *tex = Texture::read(t->name, t->mask);
	//tex->refCount++;	// ??
	if(tex->refCount == 1)
		tex->filterAddressing = (Texture::WRAP << 12) | (Texture::WRAP << 8) | Texture::LINEAR;
	return tex;
}

Material*
convertMaterial(RslMaterial *m)
{
	Material *rwm;
	rwm = Material::create();

	rwm->color = m->color;
	if(m->texture)
		rwm->texture = convertTexture(m->texture);

	if(m->matfx){
		MatFX *matfx = new MatFX;
		matfx->setEffects(m->matfx->effectType);
		matfx->setEnvCoefficient(m->matfx->env.intensity);
		if(m->matfx->env.texture)
			matfx->setEnvTexture(convertTexture(m->matfx->env.texture));
		*PLUGINOFFSET(MatFX*, rwm, matFXGlobals.materialOffset) = matfx;
	}
	return rwm;
}

static uint32
unpackSize(uint32 unpack)
{
	if((unpack&0x6F000000) == 0x6F000000)
		return 2;
	static uint32 size[] = { 32, 16, 8, 16 };
	return ((unpack>>26 & 3)+1)*size[unpack>>24 & 3]/8;
}

static uint32*
skipUnpack(uint32 *p)
{
	int32 n = (p[0] >> 16) & 0xFF;
	return p + (n*unpackSize(p[0])+3 >> 2) + 1;
}

void
convertMesh(Geometry *rwg, RslGeometry *g, int32 ii)
{
	RslPS2ResEntryHeader *resHeader = (RslPS2ResEntryHeader*)(g+1);
	RslPS2InstanceData *inst = (RslPS2InstanceData*)(resHeader+1);
	int32 numInst = resHeader->size >> 20;
	uint8 *p = (uint8*)(inst+numInst);
	inst += ii;
	p += inst->dmaPacket;
	Mesh *m = &rwg->meshHeader->mesh[inst->matID];

	ps2::SkinVertex v;
	uint32 mask = 0x1001;	// tex coords, vertices
	if(rwg->geoflags & Geometry::NORMALS)
		mask |= 0x10;
	if(rwg->geoflags & Geometry::PRELIT)
		mask |= 0x100;
	Skin *skin = *PLUGINOFFSET(Skin*, rwg, skinGlobals.offset);
	if(skin)
		mask |= 0x10000;

	int16 *vuVerts = NULL;
	int8 *vuNorms = NULL;
	uint8 *vuTex = NULL;
	uint16 *vuCols = NULL;
	uint32 *vuSkin = NULL;

	uint32 *w = (uint32*)p;
	uint32 *end = (uint32*)(p + ((w[0] & 0xFFFF) + 1)*0x10);
	w += 4;
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

		if(rwg->geoflags & Geometry::NORMALS){
			assert((w[0] & 0xFF004000) == 0x6A000000);
			vuNorms = (int8*)(w+1);
			if(!first) vuNorms += 2*3;
			w = skipUnpack(w);
		}

		if(rwg->geoflags & Geometry::PRELIT){
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
			v.p[0] = vuVerts[0]/32768.0f*resHeader->scale[0] + resHeader->pos[0];
			v.p[1] = vuVerts[1]/32768.0f*resHeader->scale[1] + resHeader->pos[1];
			v.p[2] = vuVerts[2]/32768.0f*resHeader->scale[2] + resHeader->pos[2];
			v.t[0] = vuTex[0]/128.0f*inst->uvScale[0];
			v.t[1] = vuTex[1]/128.0f*inst->uvScale[1];
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

			int32 idx = ps2::findVertexSkin(rwg, NULL, mask, &v);
			if(idx < 0)
				idx = rwg->numVertices++;
			/* Insert mesh joining indices when we get the index of the first vertex
			 * in the first VU chunk of a non-first RslMesh. */
			if(i == 0 && first && ii != 0 && inst[-1].matID == inst->matID){
				m->indices[m->numIndices] = m->indices[m->numIndices-1];
				m->numIndices++;
				m->indices[m->numIndices++] = idx;
				if(inst[-1].numTriangles % 2)
					m->indices[m->numIndices++] = idx;
			}
			m->indices[m->numIndices++] = idx;
			ps2::insertVertexSkin(rwg, idx, mask, &v);

			vuVerts += 3;
			vuTex += 2;
			vuNorms += 3;
			vuCols++;
			vuSkin += 4;
		}
		first = 0;
	}
}

Atomic*
convertAtomic(RslAtomic *atomic)
{
	Atomic *rwa = Atomic::create();
	RslGeometry *g = atomic->geometry;
	Geometry *rwg = Geometry::create(0, 0, 0);
	rwa->geometry = rwg;

	rwg->numMaterials = g->matList.numMaterials;
	rwg->materialList = new Material*[rwg->numMaterials];
	for(int32 i = 0; i < rwg->numMaterials; i++)
		rwg->materialList[i] = convertMaterial(g->matList.materials[i]);

	rwg->meshHeader = new MeshHeader;
	rwg->meshHeader->flags = 1;
	rwg->meshHeader->numMeshes = rwg->numMaterials;
	rwg->meshHeader->mesh = new Mesh[rwg->meshHeader->numMeshes];
	rwg->meshHeader->totalIndices = 0;
	Mesh *meshes = rwg->meshHeader->mesh;
	for(uint32 i = 0; i < rwg->meshHeader->numMeshes; i++)
		meshes[i].numIndices = 0;

	RslPS2ResEntryHeader *resHeader = (RslPS2ResEntryHeader*)(g+1);
	RslPS2InstanceData *inst = (RslPS2InstanceData*)(resHeader+1);
	int32 numInst = resHeader->size >> 20;

	int32 lastId = -1;
	for(int32 i = 0; i < numInst; i++){
		Mesh *m = &meshes[inst[i].matID];
		rwg->numVertices += inst[i].numTriangles+2;
		m->numIndices += inst[i].numTriangles+2;
		// Extra indices since we're merging tristrip
		// meshes with the same material.
		// Be careful with face winding.
		if(lastId == inst[i].matID)
			m->numIndices += inst[i-1].numTriangles % 2 ? 3 : 2;
		lastId = inst[i].matID;
	}
	for(uint32 i = 0; i < rwg->meshHeader->numMeshes; i++){
		rwg->meshHeader->mesh[i].material = rwg->materialList[i];
		rwg->meshHeader->totalIndices += meshes[i].numIndices;
	}
	rwg->geoflags = Geometry::TRISTRIP |
	                Geometry::POSITIONS |	 /* 0x01 ? */
	                Geometry::TEXTURED |	 /* 0x04 ? */
	                Geometry::LIGHT;
	if(rwg->hasColoredMaterial())
		rwg->geoflags |= Geometry::MODULATE;
	if(resHeader->flags & 0x2)
		rwg->geoflags |= Geometry::NORMALS;
	if(resHeader->flags & 0x8)
		rwg->geoflags |= Geometry::PRELIT;
	rwg->numTexCoordSets = 1;

	rwg->allocateData();
	rwg->meshHeader->allocateIndices();

	Skin *skin = NULL;
	if(resHeader->flags & 0x10)
		assert(g->skin);
	if(g->skin){
		skin = new Skin;
		*PLUGINOFFSET(Skin*, rwg, skinGlobals.offset) = skin;
		skin->init(g->skin->numBones, g->skin->numBones, rwg->numVertices);
		memcpy(skin->inverseMatrices, g->skin->invMatrices, skin->numBones*64);
	}

	for(uint32 i = 0; i < rwg->meshHeader->numMeshes; i++)
		meshes[i].numIndices = 0;
	rwg->meshHeader->totalIndices = rwg->numVertices = 0;
	for(int32 i = 0; i < numInst; i++)
		convertMesh(rwg, g, i);
	for(uint32 i = 0; i < rwg->meshHeader->numMeshes; i++)
		rwg->meshHeader->totalIndices += meshes[i].numIndices;
	if(skin){
		skin->findNumWeights(rwg->numVertices);
		skin->findUsedBones(rwg->numVertices);
	}
	rwg->calculateBoundingSphere();
	rwg->generateTriangles();
	return rwa;
}

RslAtomic*
collectAtomics(RslAtomic *atomic, void *data)
{
	RslAtomic ***alist = (RslAtomic***)data;
	*(*alist)++ = atomic;
	return atomic;
}

Clump*
convertClump(RslClump *c)
{
	Clump *rwc;
	Frame *rwf;
	Atomic *rwa;
	rslFrameList frameList;

	rwc = Clump::create();
	rslFrameListInitialize(&frameList, (RslFrame*)c->object.parent);
	Frame **rwframes = new Frame*[frameList.numFrames];
	for(int32 i = 0; i < frameList.numFrames; i++){
		rwf = convertFrame(frameList.frames[i]);
		rwframes[i] = rwf;
		void *par = frameList.frames[i]->object.parent;
		int32 parent = findPointer(par, (void**)frameList.frames, frameList.numFrames);
		if(parent >= 0)
			rwframes[parent]->addChild(rwf);
	}
	rwc->object.parent = rwframes[0];

	int32 numAtomics = RslClumpGetNumAtomics(c);
	RslAtomic **alist = new RslAtomic*[numAtomics];
	RslAtomic **ap = &alist[0];
	RslClumpForAllAtomics(c, collectAtomics, &ap);
	for(int32 i = 0; i < numAtomics; i++){
		rwa = convertAtomic(alist[i]);
		int32 fi = findPointer(alist[i]->object.object.parent, (void**)frameList.frames, frameList.numFrames);
		rwa->setFrame(rwframes[fi]);
		rwc->addAtomic(rwa);
	}

	delete[] alist;
	delete[] rwframes;
	delete[] frameList.frames;
	return rwc;
}

RslAtomic*
makeTextures(RslAtomic *atomic, void*)
{
	RslGeometry *g = atomic->geometry;
	RslMaterial *m;
	for(int32 i = 0; i < g->matList.numMaterials; i++){
		m = g->matList.materials[i];
		if(m->texname){
			RslTexture *tex = RslTextureCreate(NULL);
			strncpy(tex->name, m->texname, 32);
			strncpy(tex->mask, m->texname, 32);
			m->texture = tex;
		}
		if(m->matfx && m->matfx->effectType == MatFX::ENVMAP &&
		   m->matfx->env.texname){
			RslTexture *tex = RslTextureCreate(NULL);
			strncpy(tex->name, m->matfx->env.texname, 32);
			strncpy(tex->mask, m->matfx->env.texname, 32);
			m->matfx->env.texture = tex;
		}
	}
	return atomic;
}




uint8*
getPalettePS2(RslRaster *raster)
{
	uint32 f = raster->ps2.flags;
	uint32 w = 1 << (f & 0x3F);
	uint32 h = 1 << (f>>6 & 0x3F);
	uint32 d = f>>12 & 0xFF;
	uint32 mip = f>>20 & 0xF;
	uint8 *data = raster->ps2.data;
	if(d > 8)
		return NULL;
	while(mip--){
		data += w*h*d/8;
		w /= 2;
		h /= 2;
	}
	return data;
}

uint8*
getTexelPS2(RslRaster *raster, int32 n)
{
	uint32 f = raster->ps2.flags;
	uint32 w = 1 << (f & 0x3F);
	uint32 h = 1 << (f>>6 & 0x3F);
	uint32 d = f>>12 & 0xFF;
	uint8 *data = raster->ps2.data;
	for(int32 i = 0; i < n; i++){
		data += w*h*d/8;
		w /= 2;
		h /= 2;
	}
	return data;
}

void
convertCLUT(uint8 *texels, uint32 w, uint32 h)
{
	static uint8 map[4] = { 0x00, 0x10, 0x08, 0x18 };
	for (uint32 i = 0; i < w*h; i++)
		texels[i] = (texels[i] & ~0x18) | map[(texels[i] & 0x18) >> 3];
}

void
unswizzle8(uint8 *dst, uint8 *src, uint32 w, uint32 h)
{
	for (uint32 y = 0; y < h; y++)
		for (uint32 x = 0; x < w; x++) {
			int32 block_loc = (y&(~0xF))*w + (x&(~0xF))*2;
			uint32 swap_sel = (((y+2)>>2)&0x1)*4;
			int32 ypos = (((y&(~3))>>1) + (y&1))&0x7;
			int32 column_loc = ypos*w*2 + ((x+swap_sel)&0x7)*4;
			int32 byte_sum = ((y>>1)&1) + ((x>>2)&2);
			uint32 swizzled = block_loc + column_loc + byte_sum;
			dst[y*w+x] = src[swizzled];
		}
}

void
unswizzle16(uint16 *dst, uint16 *src, int32 w, int32 h)
{
	for(int y = 0; y < h; y++)
		for(int x = 0; x < w; x++){
			int32 pageX = x & (~0x3f);
			int32 pageY = y & (~0x3f);
			int32 pages_horz = (w+63)/64;
			int32 pages_vert = (h+63)/64;
			int32 page_number = (pageY/64)*pages_horz + (pageX/64);
			int32 page32Y = (page_number/pages_vert)*32;
			int32 page32X = (page_number%pages_vert)*64;
			int32 page_location = (page32Y*h + page32X)*2;
			int32 locX = x & 0x3f;
			int32 locY = y & 0x3f;
			int32 block_location = (locX&(~0xf))*h + (locY&(~0x7))*2;
			int32 column_location = ((y&0x7)*h + (x&0x7))*2;
			int32 short_num = (x>>3)&1;       // 0,1
			uint32 swizzled = page_location + block_location +
			                  column_location + short_num;
			dst[y*w+x] = src[swizzled];
		}
}

bool32 unswizzle = 1;

void
convertTo32(uint8 *out, uint8 *pal, uint8 *tex,
            uint32 w, uint32 h, uint32 d, bool32 swiz)
{
	uint32 x;
	if(d == 32){
		//uint32 *dat = new uint32[w*h];
		//if(swiz && unswizzle)
		//	unswizzle8_hack(dat, (uint32*)tex, w, h);
		//else
		//	memcpy(dat, tex, w*h*4);
		//tex = (uint8*)dat;
		for(uint32 i = 0; i < w*h; i++){
			out[i*4+0] = tex[i*4+0];
			out[i*4+1] = tex[i*4+1];
			out[i*4+2] = tex[i*4+2];
			out[i*4+3] = tex[i*4+3]*255/128;
		}
		//delete[] dat;
	}
	if(d == 16) return;	// TODO
	if(d == 8){
		uint8 *dat = new uint8[w*h];
		if(swiz && unswizzle)
			unswizzle8(dat, tex, w, h);
		else
			memcpy(dat, tex, w*h);
		tex = dat;
		convertCLUT(tex, w, h);
		for(uint32 i = 0; i < h; i++)
			for(uint32 j = 0; j < w; j++){
				x = *tex++;
				*out++ = pal[x*4+0];
				*out++ = pal[x*4+1];
				*out++ = pal[x*4+2];
				*out++ = pal[x*4+3]*255/128;
			}
		delete[] dat;
	}
	if(d == 4){
		uint8 *dat = new uint8[w*h];
		for(uint32 i = 0; i < w*h/2; i++){
			dat[i*2+0] = tex[i] & 0xF;
			dat[i*2+1] = tex[i] >> 4;
		}
		if(swiz && unswizzle){
			uint8 *tmp = new uint8[w*h];
			unswizzle8(tmp, dat, w, h);
			delete[] dat;
			dat = tmp;
		}
		tex = dat;
		for(uint32 i = 0; i < h; i++)
			for(uint32 j = 0; j < w; j++){
				x = *tex++;
				*out++ = pal[x*4+0];
				*out++ = pal[x*4+1];
				*out++ = pal[x*4+2];
				*out++ = pal[x*4+3]*255/128;
			}
		delete[] dat;
	}
}

RslTexture *dumpTextureCB(RslTexture *texture, void*)
{
	uint32 f = texture->raster->ps2.flags;
	uint32 w = 1 << (f & 0x3F);
	uint32 h = 1 << (f>>6 & 0x3F);
	uint32 d = f>>12 & 0xFF;
	uint32 mip = f>>20 & 0xF;
	uint32 swizmask = f>>24;
	uint8 *palette = getPalettePS2(texture->raster);
	uint8 *texels = getTexelPS2(texture->raster, 0);
	printf(" %x %x %x %x %x %s\n", w, h, d, mip, swizmask, texture->name);
	Image *img = Image::create(w, h, 32);
	img->allocate();
	convertTo32(img->pixels, palette, texels, w, h, d, swizmask&1);
	char *name = new char[strlen(texture->name)+5];
	strcpy(name, texture->name);
	strcat(name, ".tga");
	writeTGA(img, name);
	img->destroy();
	delete[] name;
	return texture;
}

RslTexture*
convertTexturePS2(RslTexture *texture, void *pData)
{
	TexDictionary *rwtxd = (TexDictionary*)pData;
	Texture *rwtex = Texture::create(NULL);
	RslRasterPS2 *ras = &texture->raster->ps2;

	strncpy(rwtex->name, texture->name, 32);
	strncpy(rwtex->mask, texture->mask, 32);
	rwtex->filterAddressing = 0x1102;

	uint32 f = ras->flags;
	uint32 w = 1 << (f & 0x3F);
	uint32 h = 1 << (f>>6 & 0x3F);
	uint32 d = f>>12 & 0xFF;
	//uint32 mip = f>>20 & 0xF;
	uint32 swizmask = f>>24;
	uint8 *palette = getPalettePS2(texture->raster);
	uint8 *texels = getTexelPS2(texture->raster, 0);

	int32 hasAlpha = 0;
	uint8 *convtex = NULL;
	if(d == 4){
		convtex = new uint8[w*h];
		for(uint32 i = 0; i < w*h/2; i++){
			int32 a = texels[i] & 0xF;
			int32 b = texels[i] >> 4;
			if(palette[a*4+3] != 0x80)
				hasAlpha = 1;
			if(palette[b*4+3] != 0x80)
				hasAlpha = 1;
			convtex[i*2+0] = a;
			convtex[i*2+1] = b;
		}
		if(swizmask & 1 && unswizzle){
			uint8 *tmp = new uint8[w*h];
			unswizzle8(tmp, convtex, w, h);
			delete[] convtex;
			convtex = tmp;
		}
	}else if(d == 8){
		convtex = new uint8[w*h];
		if(swizmask & 1 && unswizzle)
			unswizzle8(convtex, texels, w, h);
		else
			memcpy(convtex, texels, w*h);
		convertCLUT(convtex, w, h);
		for(uint32 i = 0; i < w*h; i++)
			if(palette[convtex[i]*4+3] != 0x80){
				hasAlpha = 1;
				break;
			}
	}

	int32 format = 0;
	switch(d){
	case 4:
	case 8:
		format |= Raster::PAL8;
		goto alpha32;

	case 32:
		for(uint32 i = 0; i < w*h; i++)
			if(texels[i*4+3] != 0x80){
				hasAlpha = 1;
				break;
			}
	alpha32:
		if(hasAlpha)
			format |= Raster::C8888;
		else
			format |= Raster::C888;
		break;
	default:
		fprintf(stderr, "unsupported depth %d\n", d);
		return NULL;
	}
	Raster *rwras = Raster::create(w, h, d == 4 ? 8 : d, format | 4, PLATFORM_D3D8);
	d3d::D3dRaster *d3dras = PLUGINOFFSET(d3d::D3dRaster, rwras, d3d::nativeRasterOffset);

	int32 pallen = d == 4 ? 16 :
	               d == 8 ? 256 : 0;
	if(pallen){
		uint8 *p = new uint8[256*4];
		for(int32 i = 0; i < pallen; i++){
			p[i*4+0] = palette[i*4+0];
			p[i*4+1] = palette[i*4+1];
			p[i*4+2] = palette[i*4+2];
			p[i*4+3] = palette[i*4+3]*255/128;
		}
		memcpy(d3dras->palette, p, 256*4);
		delete[] p;
	}

	uint8 *data = rwras->lock(0);
	if(d == 4 || d == 8)
		memcpy(data, convtex, w*h);
	else if(d == 32){
		// texture is fucked, but pretend it isn't
		for(uint32 i = 0; i < w*h; i++){
			data[i*4+2] = texels[i*4+0];
			data[i*4+1] = texels[i*4+1];
			data[i*4+0] = texels[i*4+2];
			data[i*4+3] = texels[i*4+3]*255/128;
		}
	}else
		memcpy(data, texels, w*h*d/8);
	rwras->unlock(0);
	rwtex->raster = rwras;
	delete[] convtex;

	rwtxd->add(rwtex);
	return texture;
}

TexDictionary*
convertTXD(RslTexDictionary *txd)
{
	TexDictionary *rwtxd = TexDictionary::create();
	RslTexDictionaryForAllTextures(txd, convertTexturePS2, rwtxd);
	return rwtxd;
}

void
usage(void)
{
	fprintf(stderr, "%s [-v version] [-x] [-s] input [output.{txd|dff}]\n", argv0);
	fprintf(stderr, "\t-v RW version, e.g. 33004 for 3.3.0.4\n");
	fprintf(stderr, "\t-x extract textures to tga\n");
	fprintf(stderr, "\t-s don't unswizzle textures\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();
	rw::version = 0x34003;
	rw::platform = PLATFORM_D3D8;

	assert(sizeof(void*) == 4);
	int extract = 0;

	ARGBEGIN{
	case 'v':
		sscanf(EARGF(usage()), "%x", &rw::version);
		break;
	case 's':
		unswizzle = 0;
		break;
	case 'x':
		extract++;
		break;
	default:
		usage();
	}ARGEND;

	if(argc < 1)
		usage();

	World *world = NULL;
	Sector *sector = NULL;
	RslClump *clump = NULL;
	RslAtomic *atomic = NULL;
	RslTexDictionary *txd = NULL;


	StreamFile stream;
	assert(stream.open(argv[0], "rb"));

	uint32 ident = stream.readU32();
	stream.seek(0, 0);

	if(ident == ID_TEXDICTIONARY){
		findChunk(&stream, ID_TEXDICTIONARY, NULL, NULL);
		txd = RslTexDictionaryStreamRead(&stream);
		stream.close();
		assert(txd);
		goto writeTxd;
	}
	if(ident == ID_CLUMP){
		findChunk(&stream, ID_CLUMP, NULL, NULL);
		clump = RslClumpStreamRead(&stream);
		stream.close();
		assert(clump);
		goto writeDff;
	}

	RslStream *rslstr;
	rslstr = new RslStream;
	stream.read(rslstr, 0x20);
	rslstr->data = new uint8[rslstr->fileSize-0x20];
	stream.read(rslstr->data, rslstr->fileSize-0x20);
	stream.close();
	rslstr->relocate();

	bool32 largefile;
	largefile = rslstr->dataSize > 0x1000000;

	if(rslstr->ident == WRLD_IDENT && largefile){	// hack
		world = (World*)rslstr->data;

		int len = strlen(argv[0])+1;
		char filename[1024];
		strncpy(filename, argv[0], len);
		filename[len-3] = 'i';
		filename[len-2] = 'm';
		filename[len-1] = 'g';
		filename[len] = '\0';
		assert(stream.open(filename, "rb"));
		filename[len-4] = '\\';
		filename[len-3] = '\0';

		char name[1024];
		uint8 *data;
		StreamFile outf;
		RslStreamHeader *h;
		uint32 i = 0;
		for(h = world->sectors->sector; h->ident == WRLD_IDENT; h++){
			sprintf(name, "world%04d.wrld", i++);
			strcat(filename, name);
			assert(outf.open(filename, "wb"));
			data = new uint8[h->fileEnd];
			memcpy(data, h, 0x20);
			stream.seek(h->root, 0);
			stream.read(data+0x20, h->fileEnd-0x20);
			outf.write(data, h->fileEnd);
			outf.close();
			filename[len-3] = '\0';
		}
		// radar textures
		h = world->textures;
		for(i = 0; i < world->numTextures; i++){
			sprintf(name, "txd%04d.chk", i);
			strcat(filename, name);
			assert(outf.open(filename, "wb"));
			data = new uint8[h->fileEnd];
			memcpy(data, h, 0x20);
			stream.seek(h->root, 0);
			stream.read(data+0x20, h->fileEnd-0x20);
			outf.write(data, h->fileEnd);
			outf.close();
			filename[len-3] = '\0';
			h++;
		}
		stream.close();
	}else if(rslstr->ident == WRLD_IDENT){	// sector
		sector = (Sector*)rslstr->data;
		fprintf(stderr, "%d\n",sector->unk1);
		//printf("resources\n");
		//for(uint32 i = 0; i < sector->numResources; i++){
		//	OverlayResource *r = &sector->resources[i];
		//	printf(" %d %p\n", r->id, r->raw);
		//}
		//printf("placement\n");
		if(sector->unk1 == 0)
			return 0;
		Placement *p;
		//for(p = sector->sectionA; p < sector->sectionEnd; p++){
		//	printf(" %d, %d, %f %f %f\n", p->id &0x7FFF, p->resId, p->matrix[12], p->matrix[13], p->matrix[14]);
		//}
		for(p = sector->sectionA; p < sector->sectionEnd; p++)
			printf("%f %f %f\n", p->matrix[12], p->matrix[13], p->matrix[14]);
	}else if(rslstr->ident == MDL_IDENT){
		uint8 *p;
		p = *rslstr->hashTab;
		p -= 0x24;
		atomic = (RslAtomic*)p;
		clump = atomic->clump;
		Clump *rwc;
		if(clump){
			RslClumpForAllAtomics(clump, makeTextures, NULL);
			//RslClumpForAllAtomics(clump, dumpAtomicCB, NULL);
			//RslFrameForAllChildren(RslClumpGetFrame(clump), dumpFrameCB, NULL);
		}else{
			makeTextures(atomic, NULL);
			clump = RslClumpCreate();
			RslAtomicSetFrame(atomic, RslFrameCreate());
			RslClumpSetFrame(clump, RslAtomicGetFrame(atomic));
			RslClumpAddAtomic(clump, atomic);
			//dumpAtomicCB(a, NULL);
			//RslFrameForAllChildren(RslAtomicGetFrame(atomic), dumpFrameCB, NULL);
		}
	writeDff:
		rwc = convertClump(clump);
		if(argc > 1)
			assert(stream.open(argv[1], "wb"));
		else
			assert(stream.open("out.dff", "wb"));
		rwc->streamWrite(&stream);
		stream.close();
	}else if(rslstr->ident == TEX_IDENT){
		txd = (RslTexDictionary*)rslstr->data;
	writeTxd:
		if(extract)
			RslTexDictionaryForAllTextures(txd, dumpTextureCB, NULL);
		TexDictionary *rwtxd = convertTXD(txd);
		if(argc > 1)
			assert(stream.open(argv[1], "wb"));
		else
			assert(stream.open("out.txd", "wb"));
		rwtxd->streamWrite(&stream);
		stream.close();
	}

	return 0;
}
