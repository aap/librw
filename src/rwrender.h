namespace rw {

// Render states

enum RenderState
{
	TEXTURERASTER,
	TEXTUREADDRESS,
	TEXTUREADDRESSU,
	TEXTUREADDRESSV,
	TEXTUREFILTER,
	VERTEXALPHA,
	SRCBLEND,
	DESTBLEND,
	ZTESTENABLE,
	ZWRITEENABLE,
	FOGENABLE,
	FOGCOLOR,
	CULLMODE,
	// TODO:
	// fog type, density ?
	// ? shademode
	// ???? stencil

	// platform specific or opaque?
	ALPHATESTFUNC,
	ALPHATESTREF
};

enum AlphaTestFunc
{
	ALPHAALWAYS = 0,
	ALPHAGREATEREQUAL,
	ALPHALESS
};

enum CullMode
{
	CULLNONE,
	CULLBACK,
	CULLFRONT
};

enum BlendFunction
{
	BLENDZERO = 0,
	BLENDONE,
	BLENDSRCCOLOR,
	BLENDINVSRCCOLOR,
	BLENDSRCALPHA,
	BLENDINVSRCALPHA,
	BLENDDESTALPHA,
	BLENDINVDESTALPHA,
	BLENDDESTCOLOR,
	BLENDINVDESTCOLOR,
	BLENDSRCALPHASAT
	// TODO: add more perhaps
};

void SetRenderState(int32 state, uint32 value);
void SetRenderStatePtr(int32 state, void *value);
uint32 GetRenderState(int32 state);
void *GetRenderStatePtr(int32 state);

// Im2D

namespace im2d {

float32 GetNearZ(void);
float32 GetFarZ(void);
void RenderLine(void *verts, int32 numVerts, int32 vert1, int32 vert2);
void RenderTriangle(void *verts, int32 numVerts, int32 vert1, int32 vert2, int32 vert3);
void RenderIndexedPrimitive(PrimitiveType, void *verts, int32 numVerts, void *indices, int32 numIndices);
void RenderPrimitive(PrimitiveType type, void *verts, int32 numVerts);

}

// Im3D

namespace im3d {

void Transform(void *vertices, int32 numVertices, Matrix *world);
void RenderIndexed(PrimitiveType primType, void *indices, int32 numIndices);
void End(void);

}

}

