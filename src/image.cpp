#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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

#define PLUGIN_ID ID_IMAGE

namespace rw {

int32 Image::numAllocated;

// Image formats are as follows:
//  32 bit has 4 bytes: 8888 RGBA
//  24 bit has 3 bytes: 888 RGB
//  16 bit has 2 bytes: 1555 ARGB stored in platform native order (TODO?)
// palettes always have 4 bytes: r, g, b, a
//   8 bit has 1 byte: x
//   4 bit has 1 byte per two pixels: 0xLR, where L and R are the left and right pixel resp.

Image*
Image::create(int32 width, int32 height, int32 depth)
{
	Image *img = (Image*)rwMalloc(sizeof(Image), MEMDUR_EVENT | ID_IMAGE);
	if(img == nil){
		RWERROR((ERR_ALLOC, sizeof(Image)));
		return nil;
	}
	numAllocated++;
	img->flags = 0;
	img->width = width;
	img->height = height;
	img->depth = depth;
	img->bpp = depth < 8 ? 1 : depth/8;
	img->stride = 0;
	img->pixels = nil;
	img->palette = nil;
	return img;
}

void
Image::destroy(void)
{
	this->free();
	rwFree(this);
	numAllocated--;
}

void
Image::allocate(void)
{
	if(this->pixels == nil){
		this->stride = this->width*this->bpp;
		this->pixels = rwNewT(uint8, this->stride*this->height, MEMDUR_EVENT | ID_IMAGE);
		this->flags |= 1;
	}
	if(this->palette == nil){
		if(this->depth == 4 || this->depth == 8)
			this->palette = rwNewT(uint8, (this->depth==4? 16 : 256)*4, MEMDUR_EVENT | ID_IMAGE);
		this->flags |= 2;
	}
}

void
Image::free(void)
{
	if(this->flags&1){
		rwFree(this->pixels);
		this->pixels = nil;
	}
	if(this->flags&2){
		rwFree(this->palette);
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
	int32 x = 0, y = 0;
	uint32 c[4][4];
	uint8 idx[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(int32 j = 0; j < w*h/2; j += 8){
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
	int32 x = 0, y = 0;
	uint32 c[4][4];
	uint8 idx[16];
	uint8 a[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(int32 j = 0; j < w*h; j += 16){
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
	int32 x = 0, y = 0;
	uint32 c[4][4];
	uint32 a[8];
	uint8 idx[16];
	uint8 aidx[16];
	uint8 (*dst)[4] = (uint8(*)[4])adst;
	for(int32 j = 0; j < w*h; j += 16){
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
	uint8 *npixels = rwNewT(uint8, nstride*this->height, MEMDUR_EVENT | ID_IMAGE);

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
	this->bpp = ndepth < 8 ? 1 : ndepth/8;
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

static char*
rwstrdup(const char *s)
{
	char *t;
	size_t len = strlen(s)+1;
	t = (char*)rwMalloc(len, MEMDUR_EVENT);
	if(t)
		memcpy(t, s, len);
	return t;
}

void
Image::setSearchPath(const char *path)
{
	char *p, *end;
	rwFree(searchPaths);
	numSearchPaths = 0;
	if(path)
		searchPaths = p = rwstrdup(path);
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
			return rwstrdup(name);
		}
		return nil;
	}else
		for(int i = 0; i < numSearchPaths; i++){
			s = (char*)rwMalloc(strlen(p)+len, MEMDUR_EVENT | ID_IMAGE);
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
			rwFree(s);
			p += strlen(p) + 1;
		}
	return nil;
}

}
