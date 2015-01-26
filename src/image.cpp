#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"

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
	tex->next = this->first;
	this->first = tex;
}

Texture*
TexDictionary::find(const char *name)
{
	for(Texture *tex = this->first; tex; tex = tex->next)
		if(strncmp(tex->name, name, 32) == 0)
			return tex;
	return NULL;
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
	if(this->refCount)
		delete this;
}

// TODO: do this properly, pretty ugly right now
Texture*
Texture::read(const char *name, const char *mask)
{
	(void)mask;
	Raster *raster = NULL;
	Texture *tex;

	if((tex = currentTexDictionary->find(name)))
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
	}
	tex->raster = raster;
	currentTexDictionary->add(tex);
	return tex;
}

Texture*
Texture::streamRead(Stream *stream)
{
	uint32 length;
	char name[32], mask[32];
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	uint32 filterAddressing = stream->readU16(); 
	// TODO: what is this? (mipmap? i think)
	stream->seek(2);

	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(name, length);

	assert(findChunk(stream, ID_STRING, &length, NULL));
	stream->read(mask, length);

	Texture *tex = Texture::read(name, mask);
	tex->refCount++;
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
		searchPaths = p = strdup(path);
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
		f = fopen(name, "r");
		if(f){
			fclose(f);
			printf("found %s\n", name);
			return strdup(name);
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

#ifndef RW_PS2
#pragma pack(push)
#pragma pack(1)
#endif
struct __attribute__((__packed__)) TGAHeader
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
	StreamFile file;
	assert(file.open(filename, "rb") != NULL);
	free(filename);
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

Raster::Raster(void)
{
	this->type = 0;
	this->width = this->height = this->depth = this->stride = 0;
	this->format = 0;
	this->texels = this->palette = NULL;
	this->constructPlugins();
}

Raster::~Raster(void)
{
	this->destructPlugins();
	delete[] this->texels;
	delete[] this->palette;
}

Raster*
Raster::createFromImage(Image *image)
{
	Raster *raster = new Raster;
	raster->type = 4;
	raster->width = image->width;
	raster->stride = image->stride;
	raster->height = image->height;
	raster->depth = image->depth;
	raster->texels = raster->palette = NULL;
	if(raster->depth == 32)
		raster->format = Raster::C8888;
	else if(raster->depth == 24)
		raster->format = Raster::C888;
	else if(raster->depth == 16)
		raster->format = Raster::C1555;
	else if(raster->depth == 8)
		raster->format = Raster::PAL8 | Raster::C8888;
	else if(raster->depth == 4)
		raster->format = Raster::PAL4 | Raster::C8888;
	else{
		delete raster;
		return NULL;
	}
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
