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

/* can't write alpha */
void
writeBMP(Image *image, const char *filename)
{
	uint8 buf[54];
	uint8 *p;
	StreamFile file;
	if(!file.open(filename, "wb")){
		RWERROR((ERR_FILE, filename));
		return;
	}

	int32 pallen = image->depth > 8  ? 0 :
	               image->depth == 4 ? 16 : 256;
	int32 stride = image->width*image->depth/8;
	int32 depth = image->depth == 32 ? 24 : image->depth;
	stride = stride+3 & ~3;

	// File header
	p = buf;
	*p++ = 'B';
	*p++ = 'M';
	*(uint32*)p = 0x36 + 4*pallen + image->height*stride; p += 4;
	*(uint16*)p = 0; p += 2;       // reserved
	*(uint16*)p = 0; p += 2;
	*(uint32*)p = 0x36 + 4*pallen; p += 4;    // data offset

	// DIB header
	*(uint32*)p = 0x28; p += 4;    // header size
	*(int32*)p = image->width; p += 4;
	*(int32*)p = image->height; p += 4;
	*(int16*)p = 1; p += 2;        // number of planes
	*(int16*)p = depth; p += 2;
	*(uint32*)p = 0; p += 4;       // compression: none
	*(int32*)p = 0; p += 4;        // size, not needed in our case
	*(int32*)p = 2835; p += 4;     // x resolution, 72dpi
	*(int32*)p = 2835; p += 4;     // y resolution
	*(int32*)p = 0; p += 4;        // number of used palette colors: max
	*(int32*)p = 0; p += 4;        // important pixels

	file.write(buf, 54);

	for(int i = 0; i < pallen; i++){
		file.writeU8(image->palette[i*4+2]);
		file.writeU8(image->palette[i*4+1]);
		file.writeU8(image->palette[i*4+0]);
		file.writeU8(0);
	}

	uint8 *line = image->pixels + (image->height-1)*image->stride;
	int32 n;
	for(int y = 0; y < image->height; y++){
		p = line;
		for(int x = 0; x < image->width; x++){
			switch(image->depth){
			case 4:
				file.writeU8((p[0]&0xF)<<4 | p[1]&0xF);
				p += 2;
				x++;
				break;
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
				p += 4;
			}
		}
		n = (p-line) % 4;
		while(n--)
			file.writeU8(0);
		line -= image->stride;
	}

	file.close();
}

}
