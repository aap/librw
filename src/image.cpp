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

TexDictionary *currentTexDictionary;

TexDictionary*
TexDictionary::create(void)
{
	TexDictionary *dict = (TexDictionary*)malloc(PluginBase::s_size);
	if(dict == nil){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return nil;
	}
	dict->object.init(TexDictionary::ID, 0);
	dict->textures.init();
	dict->constructPlugins();
	return dict;
}

void
TexDictionary::destroy(void)
{
	FORLIST(lnk, this->textures)
		Texture::fromDict(lnk)->destroy();
	this->destructPlugins();
	free(this);
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
	stream->readI16();	// some platform id (1 = d3d8, 2 = d3d9, 5 = opengl,
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
		txd->add(tex);
	}
	if(txd->streamReadPlugins(stream))
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
	FORLIST(lnk, this->textures)
		Texture::fromDict(lnk)->streamWriteNative(stream);
	this->streamWritePlugins(stream);
}

uint32
TexDictionary::streamGetSize(void)
{
	uint32 size = 12 + 4;
	FORLIST(lnk, this->textures)
		size += 12 + Texture::fromDict(lnk)->streamGetSizeNative();
	size += 12 + this->streamGetPluginSize();
	return size;
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
	Texture *tex = (Texture*)malloc(PluginBase::s_size);
	if(tex == nil){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return nil;
	}
	tex->dict = nil;
	tex->inDict.init();
	memset(tex->name, 0, 32);
	memset(tex->mask, 0, 32);
	tex->filterAddressing = (WRAP << 12) | (WRAP << 8) | NEAREST;
	tex->raster = raster;
	tex->refCount = 1;
	tex->constructPlugins();
	return tex;
}

void
Texture::destroy(void)
{
	this->refCount--;
	if(this->refCount <= 0){
		this->destructPlugins();
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
	if(currentTexDictionary)
		return currentTexDictionary->find(name);
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
		raster = Raster::create(0, 0, 0, 0x80);
		tex->raster = raster;
	}
	if(currentTexDictionary){
		if(tex->dict)
			tex->inDict.remove();
		currentTexDictionary->add(tex);
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
	tex->refCount++;	// TODO: RW doesn't do this, why?

	if(tex->streamReadPlugins(stream))
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

	this->streamWritePlugins(stream);
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
	size += 12 + this->streamGetPluginSize();
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
	if(this->flags&1)
		delete[] this->pixels;
	if(this->flags&2)
		delete[] this->palette;
}

void
Image::setPixels(uint8 *pixels)
{
	this->pixels = pixels;
	this->flags |= 1;
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
	// TODO: palettized textures
	if(this->depth == 32){
		for(int y = 0; y < this->height; y++){
			uint8 *line = pixels;
			for(int x = 0; x < this->width; x++){
				ret &= line[3];
				line += 4;
			}
			pixels += this->stride;
		}
	}
	return ret != 0xFF;
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
// TGA I/O
//

// TODO: fuck pakced structs
#ifndef RW_PS2
#pragma pack(push)
#pragma pack(1)
#define PACKED_STRUCT
#else
#define PACKED_STRUCT __attribute__((__packed__))
#endif
struct PACKED_STRUCT TGAHeader
{
	int8  IDlen;
	int8  colorMapType;
	int8  imageType;
	int16 colorMapOrigin;
	int16 colorMapLength;
	int8  colorMapDepth;
	int16 xOrigin, yOrigin;
	int16 width, height;
	uint8 depth;
	uint8 descriptor;
};
#ifndef RW_PS2
#pragma pack(push)
#endif

Image*
readTGA(const char *afilename)
{
	TGAHeader header;
	Image *image;
	char *filename;
	int depth = 0, palDepth = 0;
	filename = Image::getFilename(afilename);
	if(filename == nil)
		return nil;
	uint32 length;
	uint8 *data = getFileContents(filename, &length);
	assert(data != nil);
	free(filename);
	StreamMemory file;
	file.open(data, length);
	file.read(&header, sizeof(header));

	assert(header.imageType == 1 || header.imageType == 2);
	file.seek(header.IDlen);
	if(header.colorMapType){
		assert(header.colorMapOrigin == 0);
		depth = (header.colorMapLength <= 16) ? 4 : 8;
		palDepth = header.colorMapDepth;
		assert(palDepth == 24 || palDepth == 32);
	}else{
		depth = header.depth;
		assert(depth == 24 || depth == 32);
	}

	image = Image::create(header.width, header.height, depth);
	image->allocate();
	uint8 *palette = header.colorMapType ? image->palette : nil;
	uint8 (*color)[4] = nil;
	if(palette){
		int maxlen = depth == 4 ? 16 : 256;
		color = (uint8(*)[4])palette;
		int i;
		for(i = 0; i < header.colorMapLength; i++){
			color[i][2] = file.readU8();
			color[i][1] = file.readU8();
			color[i][0] = file.readU8();
			color[i][3] = 0xFF;
			if(palDepth == 32)
				color[i][3] = file.readU8();
		}
		for(; i < maxlen; i++){
			color[i][0] = color[i][1] = color[i][2] = 0;
			color[i][3] = 0xFF;
		}
	}

	uint8 *pixels = image->pixels;
	if(!(header.descriptor & 0x20))
		pixels += (image->height-1)*image->stride;
	for(int y = 0; y < image->height; y++){
		uint8 *line = pixels;
		for(int x = 0; x < image->width; x++){
			if(palette)
				*line++ = file.readU8();
			else{
				line[2] = file.readU8();
				line[1] = file.readU8();
				line[0] = file.readU8();
				line += 3;
				if(depth == 32)
					*line++ = file.readU8();
			}
		}
		pixels += (header.descriptor&0x20) ?
		              image->stride : -image->stride;
	}

	file.close();
	delete[] data;
	return image;
}

void
writeTGA(Image *image, const char *filename)
{
	TGAHeader header;
	StreamFile file;
	if(!file.open(filename, "wb")){
		RWERROR((ERR_FILE, filename));
		return;
	}
	header.IDlen = 0;
	header.imageType = image->palette != nil ? 1 : 2;
	header.colorMapType = image->palette != nil;
	header.colorMapOrigin = 0;
	header.colorMapLength = image->depth == 4 ? 16 :
	                        image->depth == 8 ? 256 : 0;
	header.colorMapDepth = image->palette ? 32 : 0;
	header.xOrigin = 0;
	header.yOrigin = 0;
	header.width = image->width;
	header.height = image->height;
	header.depth = image->depth == 4 ? 8 : image->depth;
	header.descriptor = 0x20 | (image->depth == 32 ? 8 : 0);
	file.write(&header, sizeof(header));

	uint8 *pixels = image->pixels;
	uint8 *palette = header.colorMapType ? image->palette : nil;
	uint8 (*color)[4] = (uint8(*)[4])palette;;
	if(palette)
		for(int i = 0; i < header.colorMapLength; i++){
			file.writeU8(color[i][2]);
			file.writeU8(color[i][1]);
			file.writeU8(color[i][0]);
			file.writeU8(color[i][3]);
		}

	for(int y = 0; y < image->height; y++){
		uint8 *line = pixels;
		for(int x = 0; x < image->width; x++){
			if(palette)
				file.writeU8(*line++);
			else{
				file.writeU8(line[2]);
				file.writeU8(line[1]);
				file.writeU8(line[0]);
				line += 3;
				if(image->depth == 32)
					file.writeU8(*line++);
			}
		}
		pixels += image->stride;
	}
	file.close();
}

//
// Raster
//

Raster*
Raster::create(int32 width, int32 height, int32 depth, int32 format, int32 platform)
{
	Raster *raster = (Raster*)malloc(PluginBase::s_size);
	assert(raster != nil);
	raster->platform = platform ? platform : rw::platform;
	raster->type = format & 0x7;
	raster->flags = format & 0xF8;
	raster->format = format & 0xFF00;
	raster->width = width;
	raster->height = height;
	raster->depth = depth;
	raster->texels = raster->palette = nil;
	raster->constructPlugins();

	driver[raster->platform].rasterCreate(raster);
	return raster;
}

void
Raster::destroy(void)
{
	this->destructPlugins();
	delete[] this->texels;
	delete[] this->palette;
	free(this);
}

uint8*
Raster::lock(int32 level)
{
	return driver[this->platform].rasterLock(this, level);
}

void
Raster::unlock(int32 level)
{
	driver[this->platform].rasterUnlock(this, level);
}

int32
Raster::getNumLevels(void)
{
	return driver[this->platform].rasterNumLevels(this);
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
Raster::createFromImage(Image *image)
{
	Raster *raster = Raster::create(image->width, image->height,
	                                image->depth, 4 | 0x80);
	driver[raster->platform].rasterFromImage(raster, image);
	return raster;
}

}
