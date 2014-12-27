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
readTGA(const char *filename)
{
	TGAHeader header;
	Image *image;
	int depth = 0, palDepth = 0;
	// TODO: open from image path
	ifstream file(filename, ios::binary);
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

}
