#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "ps2/rwps2.h"

#define COLOR_ARGB(a,r,g,b) \
    ((uint32)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

namespace rw {

static void nothing(ObjPipeline *, Atomic*) {}

Pipeline::Pipeline(uint32 platform)
{
	this->pluginID = 0;
	this->pluginData = 0;
	this->platform = platform;
}

ObjPipeline::ObjPipeline(uint32 platform)
 : Pipeline(platform)
{
	this->impl.instance = nothing;
	this->impl.uninstance = nothing;
	this->impl.render = nothing;
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
	if(minVert)
		*minVert = min;
	if(numVertices)
		*numVertices = max - min + 1;
}

void
instV4d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride)
{
	if(type == VERT_FLOAT4)
		for(uint32 i = 0; i < numVertices; i++){
			memcpy(dst, src, 16);
			dst += stride;
			src += 4;
		}
	else
		assert(0 && "unsupported instV3d type");
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
uninstV3d(int type, float *dst, uint8 *src, uint32 numVertices, uint32 stride)
{
	if(type == VERT_FLOAT3)
		for(uint32 i = 0; i < numVertices; i++){
			memcpy(dst, src, 12);
			src += stride;
			dst += 3;
		}
	else if(type == VERT_COMPNORM)
		for(uint32 i = 0; i < numVertices; i++){
			uint32 n = *(uint32*)src;
			int32 normal[3];
			normal[0] = n & 0x7FF;
			normal[1] = (n >> 11) & 0x7FF;
			normal[2] = (n >> 22) & 0x3FF;
			// sign extend
			if(normal[0] & 0x400) normal[0] |= ~0x7FF;
			if(normal[1] & 0x400) normal[1] |= ~0x7FF;
			if(normal[2] & 0x200) normal[2] |= ~0x3FF;
			dst[0] = normal[0] / 1023.0f;
			dst[1] = normal[1] / 1023.0f;
			dst[2] = normal[2] / 511.0f;
			src += stride;
			dst += 3;
		}
	else
		assert(0 && "unsupported uninstV3d type");
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

void
uninstV2d(int type, float *dst, uint8 *src, uint32 numVertices, uint32 stride)
{
	assert(type == VERT_FLOAT2);
	for(uint32 i = 0; i < numVertices; i++){
		memcpy(dst, src, 8);
		src += stride;
		dst += 2;
	}
}

bool32
instColor(int type, uint8 *dst, uint8 *src, uint32 numVertices, uint32 stride)
{
	bool32 hasAlpha = 0;
	if(type == VERT_ARGB){
		for(uint32 i = 0; i < numVertices; i++){
			dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = src[3];
			if(dst[3] != 0xFF)
				hasAlpha = 1;
			dst += stride;
			src += 4;
		}
	}else if(type == VERT_RGBA){
		for(uint32 i = 0; i < numVertices; i++){
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			if(dst[3] != 0xFF)
				hasAlpha = 1;
			dst += stride;
			src += 4;
		}
	}else
		assert(0 && "unsupported color type");
	return hasAlpha;
}

void
uninstColor(int type, uint8 *dst, uint8 *src, uint32 numVertices, uint32 stride)
{
	assert(type == VERT_ARGB);
	for(uint32 i = 0; i < numVertices; i++){
		dst[0] = src[2];
		dst[1] = src[1];
		dst[2] = src[0];
		dst[3] = src[3];
		src += stride;
		dst += 4;
	}
}

}
