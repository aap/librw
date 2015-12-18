#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwd3d.h"
#include "rwxbox.h"
#include "rwd3d8.h"

using namespace std;

namespace rw {

//
// TexDictionary
//

TexDictionary *currentTexDictionary;

TexDictionary::TexDictionary(void)
{
	this->first = NULL;
}

void
TexDictionary::add(Texture *tex)
{
	Texture **tp;
	for(tp = &this->first; *tp; tp = &(*tp)->next)
		;
	*tp = tex;
}

Texture*
TexDictionary::find(const char *name)
{
	for(Texture *tex = this->first; tex; tex = tex->next)
		if(strncmp(tex->name, name, 32) == 0)
			return tex;
	return NULL;
}

TexDictionary*
TexDictionary::streamRead(Stream *stream)
{
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	int32 numTex = stream->readI16();
	stream->readI16();	// some platform id (1 = d3d8, 2 = d3d9, 5 = opengl,
	                        //                   6 = ps2, 8 = xbox)
	TexDictionary *txd = new TexDictionary;
	for(int32 i = 0; i < numTex; i++){
		assert(findChunk(stream, ID_TEXTURENATIVE, NULL, NULL));
		Texture *tex = Texture::streamReadNative(stream);
		txd->add(tex);
	}
	txd->streamReadPlugins(stream);
	return txd;
}

void
TexDictionary::streamWrite(Stream *stream)
{
	writeChunkHeader(stream, ID_TEXDICTIONARY, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, 4);
	int32 numTex = 0;
	for(Texture *tex = this->first; tex; tex = tex->next)
		numTex++;
	stream->writeI16(numTex);
	stream->writeI16(0);
	for(Texture *tex = this->first; tex; tex = tex->next)
		tex->streamWriteNative(stream);
	this->streamWritePlugins(stream);
}

uint32
TexDictionary::streamGetSize(void)
{
	uint32 size = 12 + 4;
	for(Texture *tex = this->first; tex; tex = tex->next)
		size += 12 + tex->streamGetSizeNative();
	size += 12 + this->streamGetPluginSize();
	return size;
}

//
// Texture
//

Texture::Texture(void)
{
	memset(this->name, 0, 32);
	memset(this->mask, 0, 32);
	this->filterAddressing = (WRAP << 12) | (WRAP << 8) | NEAREST;
	this->raster = NULL;
	this->refCount = 1;
	this->next = NULL;
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
	if(this->refCount == NULL)
		delete this;
}

// TODO: do this properly, pretty ugly right now
Texture*
Texture::read(const char *name, const char *mask)
{
	(void)mask;
	Raster *raster = NULL;
	Texture *tex;

	if(currentTexDictionary && (tex = currentTexDictionary->find(name)))
		return tex;
	tex = new Texture;
	strncpy(tex->name, name, 32);
	strncpy(tex->mask, mask, 32);
	char *n = (char*)malloc(strlen(name) + 5);
	strcpy(n, name);
	strcat(n, ".tga");
	Image *img = readTGA(n);
	free(n);
	if(img){
		raster = Raster::createFromImage(img);
		delete img;
	}else
		raster = new Raster(0, 0, 0, 0x80);
	tex->raster = raster;
	if(currentTexDictionary && img)
		currentTexDictionary->add(tex);
	return tex;
}

Texture*
Texture::streamRead(Stream *stream)
{
	uint32 length;
	char name[32], mask[32];
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	uint32 filterAddressing = stream->readU32();
	// TODO: if V addressing is 0, copy U
	// if using mipmap filter mode, set automipmapping,
	// if 0x10000 is set, set mipmapping

	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(name, length);

	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(mask, length);

	Texture *tex = Texture::read(name, mask);
	tex->refCount++;
	if(tex->refCount == 1)
		tex->filterAddressing = filterAddressing;

	tex->streamReadPlugins(stream);

	return tex;
}

bool
Texture::streamWrite(Stream *stream)
{
	int size;
	writeChunkHeader(stream, ID_TEXTURE, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, 4);
	stream->writeU32(this->filterAddressing);

	// TODO: length can't be > 32
	size = strlen(this->name)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, size);
	stream->write(this->name, size);

	size = strlen(this->mask)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, size);
	stream->write(this->mask, size);

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
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	uint32 platform = stream->readU32();
	stream->seek(-16);
	if(platform == PLATFORM_D3D8)
		return d3d8::readNativeTexture(stream);
	if(platform == PLATFORM_XBOX)
		return xbox::readNativeTexture(stream);

//	if(rw::platform == PLATFORM_D3D8)
//		return d3d8::readNativeTexture(stream);
//	if(rw::platform == PLATFORM_XBOX)
//		return xbox::readNativeTexture(stream);

	assert(0 && "unsupported platform");
	return NULL;
}

void
Texture::streamWriteNative(Stream *stream)
{
	if(this->raster->platform == PLATFORM_D3D8)
		d3d8::writeNativeTexture(this, stream);
	else if(this->raster->platform == PLATFORM_XBOX)
		xbox::writeNativeTexture(this, stream);
	else
		assert(0 && "unsupported platform");
}

uint32
Texture::streamGetSizeNative(void)
{
	if(this->raster->platform == PLATFORM_D3D8)
		return d3d8::getSizeNativeTexture(this);
	if(this->raster->platform == PLATFORM_XBOX)
		return xbox::getSizeNativeTexture(this);
	assert(0 && "unsupported platform");
	return 0;
}

//
// Image
//

Image::Image(int32 width, int32 height, int32 depth)
{
	this->flags = 0;
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->stride = 0;
	this->pixels = NULL;
	this->palette = NULL;
}

Image::~Image(void)
{
	this->free();
}

void
Image::allocate(void)
{
	if(this->pixels == NULL){
		this->stride = this->width*(this->depth==4 ? 1 : this->depth/8);
		this->pixels = new uint8[this->stride*this->height];
		this->flags |= 1;
	}
	if(this->palette == NULL){
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

static char *searchPaths = NULL;
int numSearchPaths = 0;

void
Image::setSearchPath(const char *path)
{
	char *p, *end;
	::free(searchPaths);
	numSearchPaths = 0;
	if(path)
		searchPaths = p = _strdup(path);
	else{
		searchPaths = NULL;
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
	int len = strlen(name)+1;
	if(numSearchPaths == 0){
		f = fopen(name, "rb");
		if(f){
			fclose(f);
			printf("found %s\n", name);
			return _strdup(name);
		}
		return NULL;
	}else
		for(int i = 0; i < numSearchPaths; i++){
			s = (char*)malloc(strlen(p)+len);
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
	return NULL;
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
	if(filename == NULL)
		return NULL;
	uint32 length;
	uint8 *data = getFileContents(filename, &length);
	assert(data != NULL);
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

	image = new Image(header.width, header.height, depth);
	image->allocate();
	uint8 *palette = header.colorMapType ? image->palette : NULL;
	uint8 (*color)[4] = NULL;
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
	assert(file.open(filename, "wb"));
	header.IDlen = 0;
	header.imageType = image->palette != NULL ? 1 : 2;
	header.colorMapType = image->palette != NULL;
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
	uint8 *palette = header.colorMapType ? image->palette : NULL;
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

int32 Raster::nativeOffsets[NUM_PLATFORMS];

Raster::Raster(int32 width, int32 height, int32 depth, int32 format, int32 platform)
{
	this->platform = platform ? platform : rw::platform;
	this->type = format & 0x7;
	this->flags = format & 0xF8;
	this->format = format & 0xFF00;
	this->width = width;
	this->height = height;
	this->depth = depth;
	this->texels = this->palette = NULL;
	this->constructPlugins();

	int32 offset = nativeOffsets[this->platform];
	assert(offset != 0 && "unimplemented raster platform");
	NativeRaster *nr = PLUGINOFFSET(NativeRaster, this, offset);
	nr->create(this);
}

Raster::~Raster(void)
{
	this->destructPlugins();
	delete[] this->texels;
	delete[] this->palette;
}

uint8*
Raster::lock(int32 level)
{
	int32 offset = nativeOffsets[this->platform];
	assert(offset != 0 && "unimplemented raster platform");
	NativeRaster *nr = PLUGINOFFSET(NativeRaster, this, offset);
	return nr->lock(this, level);
}

void
Raster::unlock(int32 level)
{
	int32 offset = nativeOffsets[this->platform];
	assert(offset != 0 && "unimplemented raster platform");
	NativeRaster *nr = PLUGINOFFSET(NativeRaster, this, offset);
	nr->unlock(this, level);
}

int32
Raster::getNumLevels(void)
{
	int32 offset = nativeOffsets[this->platform];
	assert(offset != 0 && "unimplemented raster platform");
	NativeRaster *nr = PLUGINOFFSET(NativeRaster, this, offset);
	return nr->getNumLevels(this);
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

// BAD BAD BAD BAD
Raster*
Raster::createFromImage(Image *image)
{
	assert(0 && "unsupported atm");
	int32 format;
	// TODO: make that into a function
	if(image->depth == 32)
		format = Raster::C8888;
	else if(image->depth == 24)
		format = Raster::C888;
	else if(image->depth == 16)
		format = Raster::C1555;
	else if(image->depth == 8)
		format = Raster::PAL8 | Raster::C8888;
	else if(image->depth == 4)
		format = Raster::PAL4 | Raster::C8888;
	else
		return NULL;
	Raster *raster = new Raster(image->width, image->height,
	                            image->depth, format | 4 | 0x80);
	raster->stride = image->stride;

	raster->texels = new uint8[raster->stride*raster->height];
	memcpy(raster->texels, image->pixels, raster->stride*raster->height);
	if(image->palette){
		int size = raster->depth == 4 ? 16 : 256;
		raster->palette = new uint8[size*4];
		memcpy(raster->palette, image->palette, size*4);
	}
	return raster;
}

}
