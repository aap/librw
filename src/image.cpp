#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"

namespace Rw {

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
		this->pixels = new uint8[this->width*this->height*
					 (this->depth==4) ? 1 : this->depth/8];
		this->flags |= 1;
	}
	if(this->palette == NULL){
		if(this->depth == 4 || this->depth == 8)
			this->palette = new uint8[this->depth == 4 ? 16 : 256];
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

}
