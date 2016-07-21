#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "ps2/rwps2.h"
#include "d3d/rwd3d.h"
#include "d3d/rwxbox.h"
#include "d3d/rwd3d8.h"
#include "d3d/rwd3d9.h"

#ifdef _WIN32
/* srsly? */
#define strdup _strdup
#endif

#define PLUGIN_ID 0

namespace rw {

//
// TexDictionary
//

TexDictionary*
TexDictionary::create(void)
{
	TexDictionary *dict = (TexDictionary*)malloc(s_plglist.size);
	if(dict == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	dict->object.init(TexDictionary::ID, 0);
	dict->textures.init();
	s_plglist.construct(dict);
	return dict;
}

void
TexDictionary::destroy(void)
{
	FORLIST(lnk, this->textures)
		Texture::fromDict(lnk)->destroy();
	s_plglist.destruct(this);
	free(this);
}

void
TexDictionary::add(Texture *t)
{
	if(engine->currentTexDictionary == this)
		engine->currentTexDictionary = nil;
	if(t->dict)
		t->inDict.remove();
	t->dict = this;
	this->textures.append(&t->inDict);
}
Texture*
TexDictionary::find(const char *name)
{
	FORLIST(lnk, this->textures){
		Texture *tex = Texture::fromDict(lnk);
		if(strncmp_ci(tex->name, name, 32) == 0)
			return tex;
	}
	return nil;
}

TexDictionary*
TexDictionary::streamRead(Stream *stream)
{
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	int32 numTex = stream->readI16();
	stream->readI16(); // some platform id (1 = d3d8, 2 = d3d9, 5 = opengl,
	                   //                   6 = ps2, 8 = xbox)
	TexDictionary *txd = TexDictionary::create();
	if(txd == nil)
		return nil;
	Texture *tex;
	for(int32 i = 0; i < numTex; i++){
		if(!findChunk(stream, ID_TEXTURENATIVE, nil, nil)){
			RWERROR((ERR_CHUNK, "TEXTURENATIVE"));
			goto fail;
		}
		tex = Texture::streamReadNative(stream);
		if(tex == nil)
			goto fail;
		Texture::s_plglist.streamRead(stream, tex);
		txd->add(tex);
	}
	if(s_plglist.streamRead(stream, txd))
		return txd;
fail:
	txd->destroy();
	return nil;
}

void
TexDictionary::streamWrite(Stream *stream)
{
	writeChunkHeader(stream, ID_TEXDICTIONARY, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, 4);
	int32 numTex = this->count();
	stream->writeI16(numTex);
	stream->writeI16(0);
	FORLIST(lnk, this->textures){
		Texture *tex = Texture::fromDict(lnk);
		uint32 sz = tex->streamGetSizeNative();
		sz += 12 + Texture::s_plglist.streamGetSize(tex);
		writeChunkHeader(stream, ID_TEXTURENATIVE, sz);
		tex->streamWriteNative(stream);
		Texture::s_plglist.streamWrite(stream, tex);
	}
	s_plglist.streamWrite(stream, this);
}

uint32
TexDictionary::streamGetSize(void)
{
	uint32 size = 12 + 4;
	FORLIST(lnk, this->textures){
		Texture *tex = Texture::fromDict(lnk);
		size += 12 + tex->streamGetSizeNative();
		size += 12 + Texture::s_plglist.streamGetSize(tex);
	}
	size += 12 + s_plglist.streamGetSize(this);
	return size;
}

void
TexDictionary::setCurrent(TexDictionary *txd)
{
	engine->currentTexDictionary = txd;
}

TexDictionary*
TexDictionary::getCurrent(void)
{
	return engine->currentTexDictionary;
}

//
// Texture
//

bool32 loadTextures;

static Texture *defaultFindCB(const char *name);
static Texture *defaultReadCB(const char *name, const char *mask);

Texture *(*Texture::findCB)(const char *name) = defaultFindCB;
Texture *(*Texture::readCB)(const char *name, const char *mask) = defaultReadCB;

Texture*
Texture::create(Raster *raster)
{
	Texture *tex = (Texture*)malloc(s_plglist.size);
	if(tex == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	tex->dict = nil;
	tex->inDict.init();
	memset(tex->name, 0, 32);
	memset(tex->mask, 0, 32);
	tex->filterAddressing = (WRAP << 12) | (WRAP << 8) | NEAREST;
	tex->raster = raster;
	tex->refCount = 1;
	s_plglist.construct(tex);
	return tex;
}

void
Texture::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
		s_plglist.destruct(this);
		if(this->dict)
			this->inDict.remove();
		if(this->raster)
			this->raster->destroy();
		free(this);
	}
}

static Texture*
defaultFindCB(const char *name)
{
	if(engine->currentTexDictionary)
		return engine->currentTexDictionary->find(name);
	// TODO: RW searches *all* TXDs otherwise
	return nil;
}

static Texture*
defaultReadCB(const char *name, const char *mask)
{
	Texture *tex;
	Image *img;
	char *n = (char*)malloc(strlen(name) + 5);
	strcpy(n, name);
	strcat(n, ".tga");
	img = readTGA(n);
	free(n);
	if(img){
		tex = Texture::create(Raster::createFromImage(img));
		strncpy(tex->name, name, 32);
		if(mask)
			strncpy(tex->mask, mask, 32);
		img->destroy();
		return tex;
	}else
		return nil;
}

Texture*
Texture::read(const char *name, const char *mask)
{
	(void)mask;
	Raster *raster = nil;
	Texture *tex;

	if(tex = Texture::findCB(name)){
		tex->refCount++;
		return tex;
	}
	if(loadTextures){
		tex = Texture::readCB(name, mask);
		if(tex == nil)
			goto dummytex;
	}else{
	dummytex:
		tex = Texture::create(nil);
		if(tex == nil)
			return nil;
		strncpy(tex->name, name, 32);
		if(mask)
			strncpy(tex->mask, mask, 32);
		raster = Raster::create(0, 0, 0, Raster::DONTALLOCATE);
		tex->raster = raster;
	}
	if(engine->currentTexDictionary){
		if(tex->dict)
			tex->inDict.remove();
		engine->currentTexDictionary->add(tex);
	}
	return tex;
}

Texture*
Texture::streamRead(Stream *stream)
{
	uint32 length;
	char name[128], mask[128];
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	uint32 filterAddressing = stream->readU32();
	// TODO: if V addressing is 0, copy U
	// if using mipmap filter mode, set automipmapping,
	// if 0x10000 is set, set mipmapping

	if(!findChunk(stream, ID_STRING, &length, nil)){
		RWERROR((ERR_CHUNK, "STRING"));
		return nil;
	}
	stream->read(name, length);

	if(!findChunk(stream, ID_STRING, &length, nil)){
		RWERROR((ERR_CHUNK, "STRING"));
		return nil;
	}
	stream->read(mask, length);

	Texture *tex = Texture::read(name, mask);
	if(tex == nil)
		return nil;
	if(tex->refCount == 1)
		tex->filterAddressing = filterAddressing;

	if(s_plglist.streamRead(stream, tex))
		return tex;

	tex->destroy();
	return nil;
}

bool
Texture::streamWrite(Stream *stream)
{
	int size;
	char buf[36];
	writeChunkHeader(stream, ID_TEXTURE, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, 4);
	stream->writeU32(this->filterAddressing);

	memset(buf, 0, 36);
	strncpy(buf, this->name, 32);
	size = strlen(buf)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, size);
	stream->write(buf, size);

	memset(buf, 0, 36);
	strncpy(buf, this->mask, 32);
	size = strlen(buf)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, size);
	stream->write(buf, size);

	s_plglist.streamWrite(stream, this);
	return true;
}

uint32
Texture::streamGetSize(void)
{
	uint32 size = 0;
	size += 12 + 4;
	size += 12 + 12;
	size += strlen(this->name)+4 & ~3;
	size += strlen(this->mask)+4 & ~3;
	size += 12 + s_plglist.streamGetSize(this);
	return size;
}

Texture*
Texture::streamReadNative(Stream *stream)
{
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	uint32 platform = stream->readU32();
	stream->seek(-16);
	if(platform == FOURCC_PS2)
		return ps2::readNativeTexture(stream);
	if(platform == PLATFORM_D3D8)
		return d3d8::readNativeTexture(stream);
	if(platform == PLATFORM_D3D9)
		return d3d9::readNativeTexture(stream);
	if(platform == PLATFORM_XBOX)
		return xbox::readNativeTexture(stream);
	assert(0 && "unsupported platform");
	return nil;
}

void
Texture::streamWriteNative(Stream *stream)
{
	if(this->raster->platform == PLATFORM_PS2)
		ps2::writeNativeTexture(this, stream);
	else if(this->raster->platform == PLATFORM_D3D8)
		d3d8::writeNativeTexture(this, stream);
	else if(this->raster->platform == PLATFORM_D3D9)
		d3d9::writeNativeTexture(this, stream);
	else if(this->raster->platform == PLATFORM_XBOX)
		xbox::writeNativeTexture(this, stream);
	else
		assert(0 && "unsupported platform");
}

uint32
Texture::streamGetSizeNative(void)
{
	if(this->raster->platform == PLATFORM_PS2)
		return ps2::getSizeNativeTexture(this);
	if(this->raster->platform == PLATFORM_D3D8)
		return d3d8::getSizeNativeTexture(this);
	if(this->raster->platform == PLATFORM_D3D9)
		return d3d9::getSizeNativeTexture(this);
	if(this->raster->platform == PLATFORM_XBOX)
		return xbox::getSizeNativeTexture(this);
	assert(0 && "unsupported platform");
	return 0;
}

//
// Image
//
// TODO: full 16 bit support

Image*
Image::create(int32 width, int32 height, int32 depth)
{
	Image *img = (Image*)malloc(sizeof(Image));
	if(img == nil){
		RWERROR((ERR_ALLOC, sizeof(Image)));
		return nil;
	}
	img->flags = 0;
	img->width = width;
	img->height = height;
	img->depth = depth;
	img->stride = 0;
	img->pixels = nil;
	img->palette = nil;
	return img;
}

void
Image::destroy(void)
{
	this->free();
	::free(this);
}

void
Image::allocate(void)
{
	if(this->pixels == nil){
		this->stride = this->width*(this->depth==4 ? 1 : this->depth/8);
		this->pixels = new uint8[this->stride*this->height];
		this->flags |= 1;
	}
	if(this->palette == nil){
		if(this->depth == 4 || this->depth == 8)
			this->palette = new uint8[(this->depth==4? 16 : 256)*4];
		this->flags |= 2;
	}
}

void
Image::free(void)
{
	if(this->flags&1){
		delete[] this->pixels;
		this->pixels = nil;
	}
	if(this->flags&2){
		delete[] this->palette;
		this->palette = nil;
	}
}

void
Image::setPixels(uint8 *pixels)
{
	this->pixels = pixels;
	this->flags |= 1;
}

void
decompressDXT1(uint8 *adst, int32 w, int32 h, uint8 *src)
{
	/* j loops through old texels
	 * x and y loop through new texels */
	uint32 x = 0, y = 0;
	uint32 c[4][4];
	uint8 idx[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(uint32 j = 0; j < w*h/2; j += 8){
		/* calculate colors */
		uint32 col0 = *((uint16*)&src[j+0]);
		uint32 col1 = *((uint16*)&src[j+2]);
		c[0][0] = ((col0>>11) & 0x1F)*0xFF/0x1F;
		c[0][1] = ((col0>> 5) & 0x3F)*0xFF/0x3F;
		c[0][2] = ( col0      & 0x1F)*0xFF/0x1F;
		c[0][3] = 0xFF;

		c[1][0] = ((col1>>11) & 0x1F)*0xFF/0x1F;
		c[1][1] = ((col1>> 5) & 0x3F)*0xFF/0x3F;
		c[1][2] = ( col1      & 0x1F)*0xFF/0x1F;
		c[1][3] = 0xFF;
		if(col0 > col1){
			c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
			c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
			c[2][2] = (2*c[0][2] + 1*c[1][2])/3;
			c[2][3] = 0xFF;

			c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
			c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
			c[3][2] = (1*c[0][2] + 2*c[1][2])/3;
			c[3][3] = 0xFF;
		}else{
			c[2][0] = (c[0][0] + c[1][0])/2;
			c[2][1] = (c[0][1] + c[1][1])/2;
			c[2][2] = (c[0][2] + c[1][2])/2;
			c[2][3] = 0xFF;

			c[3][0] = 0x00;
			c[3][1] = 0x00;
			c[3][2] = 0x00;
			c[3][3] = 0x00;
		}

		/* make index list */
		uint32 indices = *((uint32*)&src[j+4]);
		for(int32 k = 0; k < 16; k++){
			idx[k] = indices & 0x3;
			indices >>= 2;
		}

		/* write bytes */
		for(uint32 k = 0; k < 4; k++)
			for(uint32 l = 0; l < 4; l++){
				dst[(y+l)*w + x+k][0] = c[idx[l*4+k]][0];
				dst[(y+l)*w + x+k][1] = c[idx[l*4+k]][1];
				dst[(y+l)*w + x+k][2] = c[idx[l*4+k]][2];
				dst[(y+l)*w + x+k][3] = c[idx[l*4+k]][3];
			}
		x += 4;
		if(x >= w){
			y += 4;
			x = 0;
		}
	}
}

void
decompressDXT3(uint8 *adst, int32 w, int32 h, uint8 *src)
{
	/* j loops through old texels
	 * x and y loop through new texels */
	uint32 x = 0, y = 0;
	uint32 c[4][4];
	uint8 idx[16];
	uint8 a[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(uint32 j = 0; j < w*h; j += 16){
		/* calculate colors */
		uint32 col0 = *((uint16*)&src[j+8]);
		uint32 col1 = *((uint16*)&src[j+10]);
		c[0][0] = ((col0>>11) & 0x1F)*0xFF/0x1F;
		c[0][1] = ((col0>> 5) & 0x3F)*0xFF/0x3F;
		c[0][2] = ( col0      & 0x1F)*0xFF/0x1F;

		c[1][0] = ((col1>>11) & 0x1F)*0xFF/0x1F;
		c[1][1] = ((col1>> 5) & 0x3F)*0xFF/0x3F;
		c[1][2] = ( col1      & 0x1F)*0xFF/0x1F;

		c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
		c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
		c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

		c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
		c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
		c[3][2] = (1*c[0][2] + 2*c[1][2])/3;

		/* make index list */
		uint32 indices = *((uint32*)&src[j+12]);
		for(int32 k = 0; k < 16; k++){
			idx[k] = indices & 0x3;
			indices >>= 2;
		}
		uint64 alphas = *((uint64*)&src[j+0]);
		for(int32 k = 0; k < 16; k++){
			a[k] = (alphas & 0xF)*17;
			alphas >>= 4;
		}

		/* write bytes */
		for(uint32 k = 0; k < 4; k++)
			for(uint32 l = 0; l < 4; l++){
				dst[(y+l)*w + x+k][0] = c[idx[l*4+k]][0];
				dst[(y+l)*w + x+k][1] = c[idx[l*4+k]][1];
				dst[(y+l)*w + x+k][2] = c[idx[l*4+k]][2];
				dst[(y+l)*w + x+k][3] = a[l*4+k];
			}
		x += 4;
		if(x >= w){
			y += 4;
			x = 0;
		}
	}
}

void
decompressDXT5(uint8 *adst, int32 w, int32 h, uint8 *src)
{
	/* j loops through old texels
	 * x and y loop through new texels */
	uint32 x = 0, y = 0;
	uint32 c[4][4];
	uint32 a[8];
	uint8 idx[16];
	uint8 aidx[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(uint32 j = 0; j < w*h; j += 16){
		/* calculate colors */
		uint32 col0 = *((uint16*)&src[j+8]);
		uint32 col1 = *((uint16*)&src[j+10]);
		c[0][0] = ((col0>>11) & 0x1F)*0xFF/0x1F;
		c[0][1] = ((col0>> 5) & 0x3F)*0xFF/0x3F;
		c[0][2] = ( col0      & 0x1F)*0xFF/0x1F;

		c[1][0] = ((col1>>11) & 0x1F)*0xFF/0x1F;
		c[1][1] = ((col1>> 5) & 0x3F)*0xFF/0x3F;
		c[1][2] = ( col1      & 0x1F)*0xFF/0x1F;
		if(col0 > col1){
			c[2][0] = (2*c[0][0] + 1*c[1][0])/3;
			c[2][1] = (2*c[0][1] + 1*c[1][1])/3;
			c[2][2] = (2*c[0][2] + 1*c[1][2])/3;

			c[3][0] = (1*c[0][0] + 2*c[1][0])/3;
			c[3][1] = (1*c[0][1] + 2*c[1][1])/3;
			c[3][2] = (1*c[0][2] + 2*c[1][2])/3;
		}else{
			c[2][0] = (c[0][0] + c[1][0])/2;
			c[2][1] = (c[0][1] + c[1][1])/2;
			c[2][2] = (c[0][2] + c[1][2])/2;

			c[3][0] = 0x00;
			c[3][1] = 0x00;
			c[3][2] = 0x00;
		}

		a[0] = src[j+0];
		a[1] = src[j+1];
		if(a[0] > a[1]){
			a[2] = (6*a[0] + 1*a[1])/7;
			a[3] = (5*a[0] + 2*a[1])/7;
			a[4] = (4*a[0] + 3*a[1])/7;
			a[5] = (3*a[0] + 4*a[1])/7;
			a[6] = (2*a[0] + 5*a[1])/7;
			a[7] = (1*a[0] + 6*a[1])/7;
		}else{
			a[2] = (4*a[0] + 1*a[1])/5;
			a[3] = (3*a[0] + 2*a[1])/5;
			a[4] = (2*a[0] + 3*a[1])/5;
			a[5] = (1*a[0] + 4*a[1])/5;
			a[6] = 0;
			a[7] = 0xFF;
		}

		/* make index list */
		uint32 indices = *((uint32*)&src[j+12]);
		for(int32 k = 0; k < 16; k++){
			idx[k] = indices & 0x3;
			indices >>= 2;
		}
		// only 6 indices
		uint64 alphas = *((uint64*)&src[j+2]);
		for(int32 k = 0; k < 16; k++){
			aidx[k] = alphas & 0x7;
			alphas >>= 3;
		}

		/* write bytes */
		for(uint32 k = 0; k < 4; k++)
			for(uint32 l = 0; l < 4; l++){
				dst[(y+l)*w + x+k][0] = c[idx[l*4+k]][0];
				dst[(y+l)*w + x+k][1] = c[idx[l*4+k]][1];
				dst[(y+l)*w + x+k][2] = c[idx[l*4+k]][2];
				dst[(y+l)*w + x+k][3] = a[aidx[l*4+k]];
			}
		x += 4;
		if(x >= w){
			y += 4;
			x = 0;
		}
	}
}

void
Image::setPixelsDXT(int32 type, uint8 *pixels)
{
	switch(type){
	case 1:
		decompressDXT1(this->pixels, this->width, this->height, pixels);
		break;
	case 3:
		decompressDXT3(this->pixels, this->width, this->height, pixels);
		break;
	case 5:
		decompressDXT5(this->pixels, this->width, this->height, pixels);
		break;
	}
}

void
Image::setPalette(uint8 *palette)
{
	this->palette = palette;
	this->flags |= 2;
}

bool32
Image::hasAlpha(void)
{
	uint8 ret = 0xFF;
	uint8 *pixels = this->pixels;
	if(this->depth == 24)
		return 0;
	if(this->depth == 32){
		for(int y = 0; y < this->height; y++){
			uint8 *line = pixels;
			for(int x = 0; x < this->width; x++){
				ret &= line[3];
				line += 4;
			}
			pixels += this->stride;
		}
	}else if(this->depth <= 8){
		for(int y = 0; y < this->height; y++){
			uint8 *line = pixels;
			for(int x = 0; x < this->width; x++){
				ret &= this->palette[*line*4+3];
				line++;
			}
			pixels += this->stride;
		}
	}
	return ret != 0xFF;
}

void
Image::unindex(void)
{
	if(this->depth > 8)
		return;
	assert(this->pixels);
	assert(this->palette);

	int32 ndepth = this->hasAlpha() ? 32 : 24;
	int32 nstride = this->width*ndepth/8;
	uint8 *npixels = new uint8[nstride*this->height];

	uint8 *line = this->pixels;
	uint8 *nline = npixels;
	uint8 *p, *np;
	for(int32 y = 0; y < this->height; y++){
		p = line;
		np = nline;
		for(int32 x = 0; x < this->width; x++){
			np[0] = this->palette[*p*4+0];
			np[1] = this->palette[*p*4+1];
			np[2] = this->palette[*p*4+2];
			np += 3;
			if(ndepth == 32)
				*np++ = this->palette[*p*4+3];
			p++;
		}
		line += this->stride;
		nline += nstride;
	}
	this->free();
	this->depth = ndepth;
	this->stride = nstride;
	this->setPixels(npixels);
}

void
Image::removeMask(void)
{
	if(this->depth <= 8){
		assert(this->palette);
		int32 pallen = 4*(this->depth == 4 ? 16 : 256);
		for(int32 i = 0; i < pallen; i += 4)
			this->palette[i] = 0xFF;
		return;
	}
	if(this->depth == 24)
		return;
	assert(this->pixels);
	uint8 *line = this->pixels;
	uint8 *p;
	for(int32 y = 0; y < this->height; y++){
		p = line;
		for(int32 x = 0; x < this->width; x++){
			switch(this->depth){
			case 16:
				p[1] |= 0x80;
				p += 2;
				break;
			case 32:
				p[3] = 0xFF;
				p += 4;
				break;
			}
		}
		line += this->stride;
	}
}

Image*
Image::extractMask(void)
{
	Image *img = Image::create(this->width, this->height, 8);
	img->allocate();

	// use an 8bit palette to store all shades of grey
	for(int32 i = 0; i < 256; i++){
		img->palette[i*4+0] = i;
		img->palette[i*4+1] = i;
		img->palette[i*4+2] = i;
		img->palette[i*4+3] = 0xFF;
	}

	// Then use the alpha value as palette index
	uint8 *line = this->pixels;
	uint8 *nline = img->pixels;
	uint8 *p, *np;
	for(int32 y = 0; y < this->height; y++){
		p = line;
		np = nline;
		for(int32 x = 0; x < this->width; x++){
			switch(this->depth){
			case 4:
			case 8:
				*np++ = this->palette[*p*4+3];
				p++;
				break;
			case 16:
				*np++ = 0xFF*!!(p[1]&0x80);
				p += 2;
				break;
			case 24:
				*np++ = 0xFF;
				p += 3;
				break;
			case 32:
				*np++ = p[3];
				p += 4;
				break;
			}
		}
		line += this->stride;
		nline += img->stride;
	}
	return img;
}

static char *searchPaths = nil;
int numSearchPaths = 0;

void
Image::setSearchPath(const char *path)
{
	char *p, *end;
	::free(searchPaths);
	numSearchPaths = 0;
	if(path)
		searchPaths = p = strdup(path);
	else{
		searchPaths = nil;
		return;
	}
	while(p && *p){
		end = strchr(p, ';');
		if(end)
			*end++ = '\0';
		numSearchPaths++;
		p = end;
	}
}

void
Image::printSearchPath(void)
{
	char *p = searchPaths;
	for(int i = 0; i < numSearchPaths; i++){
		printf("%s\n", p);
		p += strlen(p) + 1;
	}
}

char*
Image::getFilename(const char *name)
{
	FILE *f;
	char *s, *p = searchPaths;
	size_t len = strlen(name)+1;
	if(numSearchPaths == 0){
		f = fopen(name, "rb");
		if(f){
			fclose(f);
			printf("found %s\n", name);
			return strdup(name);
		}
		return nil;
	}else
		for(int i = 0; i < numSearchPaths; i++){
			s = (char*)malloc(strlen(p)+len);
			if(s == nil){
				RWERROR((ERR_ALLOC, strlen(p)+len));
				return nil;
			}
			strcpy(s, p);
			strcat(s, name);
			f = fopen(s, "r");
			if(f){
				fclose(f);
				printf("found %s\n", name);
				return s;
			}
			::free(s);
			p += strlen(p) + 1;
		}
	return nil;
}

//
// Raster
//

Raster*
Raster::create(int32 width, int32 height, int32 depth, int32 format, int32 platform)
{
	Raster *raster = (Raster*)malloc(s_plglist.size);
	assert(raster != nil);
	raster->platform = platform ? platform : rw::platform;
	raster->type = format & 0x7;
	raster->flags = format & 0xF8;
	raster->format = format & 0xFF00;
	raster->width = width;
	raster->height = height;
	raster->depth = depth;
	raster->texels = raster->palette = nil;
	s_plglist.construct(raster);

	driver[raster->platform]->rasterCreate(raster);
	return raster;
}

void
Raster::destroy(void)
{
	s_plglist.destruct(this);
//	delete[] this->texels;
//	delete[] this->palette;
	free(this);
}

uint8*
Raster::lock(int32 level)
{
	return driver[this->platform]->rasterLock(this, level);
}

void
Raster::unlock(int32 level)
{
	driver[this->platform]->rasterUnlock(this, level);
}

int32
Raster::getNumLevels(void)
{
	return driver[this->platform]->rasterNumLevels(this);
}

int32
Raster::calculateNumLevels(int32 width, int32 height)
{
	int32 size = width >= height ? width : height;
	int32 n;
	for(n = 0; size != 0; n++)
		size /= 2;
	return n;
}

Raster*
Raster::createFromImage(Image *image, int32 platform)
{
	Raster *raster = Raster::create(image->width, image->height,
	                                image->depth, TEXTURE | DONTALLOCATE,
	                                platform);
	driver[raster->platform]->rasterFromImage(raster, image);
	return raster;
}

Image*
Raster::toImage(void)
{
	return driver[this->platform]->rasterToImage(this);
}

}
