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

#ifdef _WIN32
/* srsly? */
#define strdup _strdup
#endif

#define PLUGIN_ID 0

namespace rw {

// TODO: fuck packed structs
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
	rwFree(filename);
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

	uint8 *palette = header.colorMapType ? image->palette : nil;
	uint8 (*color)[4] = (uint8(*)[4])palette;;
	if(palette)
		for(int i = 0; i < header.colorMapLength; i++){
			file.writeU8(color[i][2]);
			file.writeU8(color[i][1]);
			file.writeU8(color[i][0]);
			file.writeU8(color[i][3]);
		}

	uint8 *line = image->pixels;
	uint8 *p;
	for(int y = 0; y < image->height; y++){
		p = line;
		for(int x = 0; x < image->width; x++){
			switch(image->depth){
			case 4:
			case 8:
				file.writeU8(*p++);
				break;
			case 16:
				file.writeU8(p[0]);
				file.writeU8(p[1]);
				p += 2;
				break;
			case 24:
				file.writeU8(p[2]);
				file.writeU8(p[1]);
				file.writeU8(p[0]);
				p += 3;
				break;
			case 32:
				file.writeU8(p[2]);
				file.writeU8(p[1]);
				file.writeU8(p[0]);
				file.writeU8(p[3]);
				p += 4;
			}
		}
		line += image->stride;
	}
	file.close();
}

}
