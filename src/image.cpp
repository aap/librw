#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"

using namespace std;

namespace Rw {

uint8
readUInt8(istream &rw)
{
	uint8 tmp;
	rw.read((char*)&tmp, sizeof(uint8));
	return tmp;
}

uint32
writeUInt8(uint8 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(uint8));
        return sizeof(uint8);
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

//#pragma pack(push)
//#pragma pack(1)
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
//#pragma pack(push)

Image*
readTGA(const char *afilename)
{
	TGAHeader header;
	Image *image;
	char *filename;
	int depth = 0, palDepth = 0;
	// TODO: open from image path
	filename = Image::getFilename(afilename);
	if(filename == NULL)
		return NULL;
	ifstream file(filename, ios::binary);
	free(filename);
	file.read((char*)&header, sizeof(header));

	assert(header.imageType == 1 || header.imageType == 2);
	file.seekg(header.IDlen, ios::cur);
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
			color[i][2] = readUInt8(file);
			color[i][1] = readUInt8(file);
			color[i][0] = readUInt8(file);
			color[i][3] = 0xFF;
			if(palDepth == 32)
				color[i][3] = readUInt8(file);
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
				*line++ = readUInt8(file);
			else{
				line[2] = readUInt8(file);
				line[1] = readUInt8(file);
				line[0] = readUInt8(file);
				line += 3;
				if(depth == 32)
					*line++ = readUInt8(file);
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
	// TODO: open from image path
	ofstream file(filename, ios::binary);
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
	file.write((char*)&header, sizeof(header));

	uint8 *pixels = image->pixels;
	uint8 *palette = header.colorMapType ? image->palette : NULL;
	uint8 (*color)[4] = (uint8(*)[4])palette;;
	if(palette)
		for(int i = 0; i < header.colorMapLength; i++){
			writeUInt8(color[i][2], file);
			writeUInt8(color[i][1], file);
			writeUInt8(color[i][0], file);
			writeUInt8(color[i][3], file);
		}

	for(int y = 0; y < image->height; y++){
		uint8 *line = pixels;
		for(int x = 0; x < image->width; x++){
			if(palette)
				writeUInt8(*line++, file);
			else{
				writeUInt8(line[2], file);
				writeUInt8(line[1], file);
				writeUInt8(line[0], file);
				line += 3;
				if(image->depth == 32)
					writeUInt8(*line++, file);
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

// TODO: do this properly, only an ugly hack right now
Raster*
Raster::read(const char *name, const char *mask)
{
	(void)mask;
	char *n = (char*)malloc(strlen(name) + 5);
	strcpy(n, name);
	strcat(n, ".tga");
	Image *img = readTGA(n);
	free(n);
	if(img){
		Raster *raster = Raster::createFromImage(img);
		delete img;
		return raster;
	}
	return NULL;
}

}
