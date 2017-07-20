#ifdef RW_D3D9
#define NOMINMAX
#include <d3d9.h>
#endif

namespace rw {
namespace d3d {

void initializeRender(void);

extern bool32 isP8supported;

#ifdef RW_D3D9
extern IDirect3DDevice9 *device;
#else
enum {
	D3DLOCK_NOSYSLOCK     =  0,  // ignored
	D3DPOOL_MANAGED       =  0,  // ignored
	D3DPT_TRIANGLELIST    =  4,
	D3DPT_TRIANGLESTRIP   =  5,


	D3DDECLTYPE_FLOAT1    =  0,  // 1D float expanded to (value, 0., 0., 1.)
	D3DDECLTYPE_FLOAT2    =  1,  // 2D float expanded to (value, value, 0., 1.)
	D3DDECLTYPE_FLOAT3    =  2,  // 3D float expanded to (value, value, value, 1.)
	D3DDECLTYPE_FLOAT4    =  3,  // 4D float
	D3DDECLTYPE_D3DCOLOR  =  4,  // 4D packed unsigned bytes mapped to 0. to 1. range
	                             // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
	D3DDECLTYPE_UBYTE4    =  5,  // 4D unsigned byte
	D3DDECLTYPE_SHORT2    =  6,  // 2D signed short expanded to (value, value, 0., 1.)
	D3DDECLTYPE_SHORT4    =  7,  // 4D signed short

	D3DDECLTYPE_UBYTE4N   =  8,  // Each of 4 bytes is normalized by dividing to 255.0
	D3DDECLTYPE_SHORT2N   =  9,  // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
	D3DDECLTYPE_SHORT4N   = 10,  // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
	D3DDECLTYPE_USHORT2N  = 11,  // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
	D3DDECLTYPE_USHORT4N  = 12,  // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
	D3DDECLTYPE_UDEC3     = 13,  // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
	D3DDECLTYPE_DEC3N     = 14,  // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
	D3DDECLTYPE_FLOAT16_2 = 15,  // Two 16-bit floating point values, expanded to (value, value, 0, 1)
	D3DDECLTYPE_FLOAT16_4 = 16,  // Four 16-bit floating point values
	D3DDECLTYPE_UNUSED    = 17,  // When the type field in a decl is unused.


	D3DDECLMETHOD_DEFAULT =  0,


	D3DDECLUSAGE_POSITION = 0,
	D3DDECLUSAGE_BLENDWEIGHT,   // 1
	D3DDECLUSAGE_BLENDINDICES,  // 2
	D3DDECLUSAGE_NORMAL,        // 3
	D3DDECLUSAGE_PSIZE,         // 4
	D3DDECLUSAGE_TEXCOORD,      // 5
	D3DDECLUSAGE_TANGENT,       // 6
	D3DDECLUSAGE_BINORMAL,      // 7
	D3DDECLUSAGE_TESSFACTOR,    // 8
	D3DDECLUSAGE_POSITIONT,     // 9
	D3DDECLUSAGE_COLOR,         // 10
	D3DDECLUSAGE_FOG,           // 11
	D3DDECLUSAGE_DEPTH,         // 12
	D3DDECLUSAGE_SAMPLE,        // 13
};
#endif

extern int vertFormatMap[];

void *createIndexBuffer(uint32 length);
uint16 *lockIndices(void *indexBuffer, uint32 offset, uint32 size, uint32 flags);
void unlockIndices(void *indexBuffer);
void *createVertexBuffer(uint32 length, uint32 fvf, int32 pool);
uint8 *lockVertices(void *vertexBuffer, uint32 offset, uint32 size, uint32 flags);
void unlockVertices(void *vertexBuffer);
void *createTexture(int32 width, int32 height, int32 levels, uint32 format);
uint8 *lockTexture(void *texture, int32 level);
void unlockTexture(void *texture, int32 level);
void deleteObject(void *object);

// Native Texture and Raster

struct D3dRaster
{
	void *texture;
	void *palette;
	uint32 format;
	bool32 hasAlpha;
	bool32 customFormat;
};

int32 getLevelSize(Raster *raster, int32 level);
void allocateDXT(Raster *raster, int32 dxt, int32 numLevels, bool32 hasAlpha);
void setPalette(Raster *raster, void *palette, int32 size);
void setTexels(Raster *raster, void *texels, int32 level);

extern int32 nativeRasterOffset;
void registerNativeRaster(void);

// Rendering

void setRenderState(uint32 state, uint32 value);
void setTextureStageState(uint32 stage, uint32 type, uint32 value);
void flushCache(void);
void setSamplerState(uint32 stage, uint32 type, uint32 value);

void setTexture(uint32 stage, Texture *tex);
void setMaterial(Material *mat);

}
}
