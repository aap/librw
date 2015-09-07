#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwps2.h"

#define COLOR_ARGB(a,r,g,b) \
    ((uint32)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

using namespace std;

namespace rw {

Pipeline::Pipeline(uint32 platform)
{
	this->pluginID = 0;
	this->pluginData = 0;
	this->platform = platform;
}

Pipeline::Pipeline(Pipeline *)
{
	assert(0 && "Can't copy pipeline");
}

Pipeline::~Pipeline(void)
{
}

void
Pipeline::dump(void)
{
}

void
ObjPipeline::instance(Atomic*)
{
	fprintf(stderr, "This pipeline can't instance\n");
}

void
ObjPipeline::uninstance(Atomic*)
{
	fprintf(stderr, "This pipeline can't uninstance\n");
}

void
ObjPipeline::render(Atomic*)
{
	fprintf(stderr, "This pipeline can't render\n");
}

// helper functions

void
findMinVertAndNumVertices(uint16 *indices, uint32 numIndices, uint32 *minVert, int32 *numVertices)
{
	uint32 min = 0xFFFFFFFF;
	uint32 max = 0;
	while(numIndices--){
		if(*indices < min)
			min = *indices;
		if(*indices > max)
			max = *indices;
		indices++;
	}
	*minVert = min;
	*numVertices = max - min + 1;
}

void
instV3d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride)
{
	if(type == VERT_FLOAT3)
		for(uint32 i = 0; i < numVertices; i++){
			memcpy(dst, src, 12);
			dst += stride;
			src += 3;
		}
	else if(type == VERT_COMPNORM)
		for(uint32 i = 0; i < numVertices; i++){
			uint32 n = ((((uint32)(src[2] *  511.0f)) & 0x3ff) << 22) |
				   ((((uint32)(src[1] * 1023.0f)) & 0x7ff) << 11) |
				   ((((uint32)(src[0] * 1023.0f)) & 0x7ff) <<  0);
			*(uint32*)dst = n;
			dst += stride;
			src += 3;
		}
	else
		assert(0 && "unsupported instV3d type");
}

void
instV2d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride)
{
	assert(type == VERT_FLOAT2);
	for(uint32 i = 0; i < numVertices; i++){
		memcpy(dst, src, 8);
		dst += stride;
		src += 2;
	}
}

bool32
instColor(int type, uint8 *dst, uint8 *src, uint32 numVertices, uint32 stride)
{
	assert(type == VERT_ARGB);
	bool32 hasAlpha = 0;
	for(uint32 i = 0; i < numVertices; i++){
		uint32 col = COLOR_ARGB(src[3], src[0], src[1], src[2]);
		if(src[3] < 0xFF)
			hasAlpha = 1;
		memcpy(dst, &col, 4);
		dst += stride;
		src += 4;
	}
	return hasAlpha;
}

}
