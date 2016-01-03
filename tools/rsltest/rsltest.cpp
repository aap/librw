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
	uint32 off = (uint32)this->data;
	off -= 0x20;
	*(uint32*)&this->reloc += off;
	*(uint32*)&this->hashTab += off;

	uint8 **rel = (uint8**)this->reloc;
	for(uint32 i = 0; i < this->relocSize; i++){
		rel[i] += off;
		*this->reloc[i] += off;
	}
}

RslFrame*
RslFrameForAllChildren(RslFrame *frame, RslFrameCallBack callBack, void *data)
{
	for(RslFrame *child = frame->child;
	    child;
	    child = child->next)
		if(callBack(child, data) == NULL)
			break;
	return frame;
}

RslClump*
RslClumpForAllAtomics(RslClump *clump, RslAtomicCallBack callback, void *pData)
{
	RslAtomic *a;
	RslLLLink *link;
	for(link = rslLLLinkGetNext(&clump->atomicList.link);
	    link != &clump->atomicList.link;
	    link = link->next){
		a = rslLLLinkGetData(link, RslAtomic, inClumpLink);
		if(callback(a, pData) == NULL)
			break;
	}
	return clump;
}

RslGeometry*
RslGeometryForAllMaterials(RslGeometry *geometry, RslMaterialCallBack fpCallBack, void *pData)
{
	for(int32 i = 0; i < geometry->matList.numMaterials; i++)
		if(fpCallBack(geometry->matList.materials[i], pData) == NULL)
			break;
	return geometry;
}

RslTexDictionary*
RslTexDictionaryCreate(void)
{
	RslTexDictionary *dict = new RslTexDictionary;
	memset(dict, 0, sizeof(RslTexDictionary));
	dict->object.type = 6;
	dict->texturesInDict.link.prev = &dict->texturesInDict.link;
	dict->texturesInDict.link.next = &dict->texturesInDict.link;
	return dict;
}

RslTexture*
RslTexDictionaryAddTexture(RslTexDictionary *dict, RslTexture *tex)
{
	if(tex->dict){
		tex->lInDictionary.prev->next = tex->lInDictionary.next;
		tex->lInDictionary.next->prev = tex->lInDictionary.prev;
	}
	tex->dict = dict;
	tex->lInDictionary.prev = &dict->texturesInDict.link;
	tex->lInDictionary.next = dict->texturesInDict.link.next;
	dict->texturesInDict.link.next->prev = &tex->lInDictionary;
	dict->texturesInDict.link.next = &tex->lInDictionary;
	return tex;
}

RslTexDictionary*
RslTexDictionaryForAllTextures(RslTexDictionary *dict, RslTextureCallBack fpCallBack, void *pData)
{
	RslTexture *t;
	RslLLLink *link;
	for(link = rslLLLinkGetNext(&dict->texturesInDict.link);
	    link != &dict->texturesInDict.link;
	    link = link->next){
		t = rslLLLinkGetData(link, RslTexture, lInDictionary);
		if(fpCallBack(t, pData) == NULL)
			break;
	}
	return dict;
}

uint32
guessSwizzling(uint32 w, uint32 h, uint32 d, uint32 mipmaps)
{
	uint32 swiz = 0;
	for(uint32 i = 0; i < mipmaps; i++){
		switch(d){
		case 4:
			if(w >= 32 && h >= 16)
				swiz |= 1<<i;
			break;
		case 8:
			if(w >= 16 && h >= 4)
				swiz |= 1<<i;
			break;
		}
		w /= 2;
		h /= 2;
	}
	return swiz;
}

RslRaster*
RslCreateRasterPS2(uint32 w, uint32 h, uint32 d, uint32 mipmaps)
{
	RslRasterPS2 *r;
	r = new RslRasterPS2;
	uint32 tmp, logw = 0, logh = 0;
	for(tmp = 1; tmp < w; tmp <<= 1)
		logw++;
	for(tmp = 1; tmp < h; tmp <<= 1)
		logh++;
	r->flags = 0;
	r->flags |= logw&0x3F;
	r->flags |= (logh&0x3F)<<6;
	r->flags |= d << 12;
	r->flags |= mipmaps << 20;
	uint32 swiz = guessSwizzling(w, h, d, mipmaps);
	r->flags |= swiz << 24;
	return (RslRaster*)r;
}

RslTexture*
RslReadNativeTexturePS2(Stream *stream)
{
	RslPs2StreamRaster rasterInfo;
	uint32 len;
	uint32 buf[2];
	RslTexture *tex = RslTextureCreate(NULL);
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	stream->read(buf, sizeof(buf));
	assert(buf[0] == 0x00505350); /* "PSP\0" */
	assert(findChunk(stream, ID_STRING, &len, NULL));
	stream->read(tex->name, len);
	assert(findChunk(stream, ID_STRING, &len, NULL));
	stream->read(tex->mask, len);
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	assert(findChunk(stream, ID_STRUCT, &len, NULL));
	stream->read(&rasterInfo, sizeof(rasterInfo));
	assert(findChunk(stream, ID_STRUCT, &len, NULL));
	tex->raster = RslCreateRasterPS2(rasterInfo.width,
		rasterInfo.height, rasterInfo.depth, rasterInfo.mipmaps);
	tex->raster->ps2.data = new uint8[len];
	stream->read(tex->raster->ps2.data, len);
	assert(findChunk(stream, ID_EXTENSION, &len, NULL));
	stream->seek(len);
	return tex;
}

RslTexDictionary*
RslTexDictionaryStreamRead(Stream *stream)
{
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	int32 numTex = stream->readI32();
	RslTexDictionary *txd = RslTexDictionaryCreate();
	for(int32 i = 0; i < numTex; i++){
		assert(findChunk(stream, ID_TEXTURENATIVE, NULL, NULL));
		RslTexture *tex = RslReadNativeTexturePS2(stream);
		RslTexDictionaryAddTexture(txd, tex);
	}
	return txd;
}

RslTexture*
RslTextureCreate(RslRaster *raster)
{
	RslTexture *tex = new RslTexture;
	memset(tex, 0, sizeof(RslTexture));
	tex->raster = raster;
	return tex;
}




RslFrame *dumpFrameCB(RslFrame *frame, void *data)
{
	printf(" frm: %x %s %x\n", frame->nodeId, frame->name, frame->hierId);
	RslFrameForAllChildren(frame, dumpFrameCB, data);
	return frame;
}

RslMaterial *dumpMaterialCB(RslMaterial *material, void*)
{
	printf("  mat: %s %x\n", material->texname, material->refCount);
	if(material->matfx){
		RslMatFX *fx = material->matfx;
		printf("   matfx: ", fx->effectType);
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

RslTexture *dumpTextureCB(RslTexture *texture, void *pData)
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
	Image *img = new Image(w, h, 32);
	img->allocate();
	convertTo32(img->pixels, palette, texels, w, h, d, swizmask&1);
	char *name = new char[strlen(texture->name)+5];
	strcpy(name, texture->name);
	strcat(name, ".tga");
	writeTGA(img, name);
	delete img;
	delete[] name;
	return texture;
}

RslTexture*
convertTexturePS2(RslTexture *texture, void *pData)
{
	TexDictionary *rwtxd = (TexDictionary*)pData;
	Texture *rwtex = new Texture;
	RslRasterPS2 *ras = &texture->raster->ps2;

	strncpy(rwtex->name, texture->name, 32);
	strncpy(rwtex->mask, texture->mask, 32);
	rwtex->filterAddressing = 0x1102;

	uint32 f = ras->flags;
	uint32 w = 1 << (f & 0x3F);
	uint32 h = 1 << (f>>6 & 0x3F);
	uint32 d = f>>12 & 0xFF;
	uint32 mip = f>>20 & 0xF;
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
	Raster *rwras = new Raster(w, h, d == 4 ? 8 : d, format | 4, PLATFORM_D3D8);
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
	TexDictionary *rwtxd = new TexDictionary;
	RslTexDictionaryForAllTextures(txd, convertTexturePS2, rwtxd);
	return rwtxd;
}

void
usage(void)
{
	fprintf(stderr, "%s [-v version] [-x] [-s] input [output.txd]\n", argv0);
	fprintf(stderr, "\t-v RW version, e.g. 33004 for 3.3.0.4\n");
	fprintf(stderr, "\t-x extract to tga\n");
	fprintf(stderr, "\t-s don't unswizzle\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();
	rw::version = 0x34003;

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

	World *world;
	Sector *sector;
	RslClump *clump;
	RslTexDictionary *txd;


	StreamFile stream;
	assert(stream.open(argv[0], "rb"));

	uint32 ident = stream.readU32();
	stream.seek(0, 0);

	if(ident == 0x16){
		findChunk(&stream, ID_TEXDICTIONARY, NULL, NULL);
		txd = RslTexDictionaryStreamRead(&stream);
		stream.close();
		assert(txd);
		goto writeTxd;
	}

	RslStream *rslstr = new RslStream;
	stream.read(rslstr, 0x20);
	rslstr->data = new uint8[rslstr->fileSize-0x20];
	stream.read(rslstr->data, rslstr->fileSize-0x20);
	stream.close();
	rslstr->relocate();

	int largefile = rslstr->dataSize > 0x100000;

	if(rslstr->ident == WRLD_IDENT && largefile){	// hack
		world = (World*)rslstr->data;

		int len = strlen(argv[1])+1;
		char filename[1024];
		strncpy(filename, argv[1], len);
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
		printf("resources\n");
		for(uint32 i = 0; i < sector->numResources; i++){
			OverlayResource *r = &sector->resources[i];
			printf(" %d %p\n", r->id, r->raw);
		}
		printf("placement\n");
		Placement *p;
		for(p = sector->sectionA; p < sector->sectionEnd; p++){
			printf(" %d, %d, %f %f %f\n", p->id &0x7FFF, p->resId, p->matrix[12], p->matrix[13], p->matrix[14]);
		}
	}else if(rslstr->ident == MDL_IDENT){
		uint8 *p = *rslstr->hashTab;
		p -= 0x24;
		RslAtomic *a = (RslAtomic*)p;
		clump = a->clump;
		if(clump)
			//RslClumpForAllAtomics(clump, dumpAtomicCB, NULL);
			RslFrameForAllChildren(RslClumpGetFrame(clump), dumpFrameCB, NULL);
		else
			//dumpAtomicCB(a, NULL);
			RslFrameForAllChildren(RslAtomicGetFrame(a), dumpFrameCB, NULL);
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
