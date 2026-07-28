#include <stdio.h>

#include "rwbase.h"
#include "rwplg.h"
#include "rwengine.h"
#include "rwrender.h"

namespace rw {

void SetRenderState(int32 state, uint32 value){
	engine->d3ddevice.setRenderState(state, (void*)(uintptr)value); }

void SetRenderStatePtr(int32 state, void *value){
	engine->d3ddevice.setRenderState(state, value); }

uint32 GetRenderState(int32 state){
	return (uint32)(uintptr)engine->d3ddevice.getRenderState(state); }

void *GetRenderStatePtr(int32 state){
	return engine->d3ddevice.getRenderState(state); }

// Im2D

namespace im2d {

float32 GetNearZ(void) { return engine->d3ddevice.zNear; }
float32 GetFarZ(void) { return engine->d3ddevice.zFar; }
void
RenderLine(void *verts, int32 numVerts, int32 vert1, int32 vert2)
{
	engine->d3ddevice.im2DRenderLine(verts, numVerts, vert1, vert2);
}
void
RenderTriangle(void *verts, int32 numVerts, int32 vert1, int32 vert2, int32 vert3)
{
	engine->d3ddevice.im2DRenderTriangle(verts, numVerts, vert1, vert2, vert3);
}
void
RenderIndexedPrimitive(PrimitiveType type, void *verts, int32 numVerts, void *indices, int32 numIndices)
{
	engine->d3ddevice.im2DRenderIndexedPrimitive(type, verts, numVerts, indices, numIndices);
}
void
RenderPrimitive(PrimitiveType type, void *verts, int32 numVerts)
{
	engine->d3ddevice.im2DRenderPrimitive(type, verts, numVerts);
}

}

// Im3D

namespace im3d {

void
Transform(void *vertices, int32 numVertices, Matrix *world, uint32 flags)
{
	engine->d3ddevice.im3DTransform(vertices, numVertices, world, flags);
}
void
RenderLine(int32 vert1, int32 vert2)
{
	int16 indices[2];
	indices[0] = vert1;
	indices[1] = vert2;
	RenderIndexedPrimitive(rw::PRIMTYPELINELIST, indices, 2);
}
void
RenderTriangle(int32 vert1, int32 vert2, int32 vert3)
{
	int16 indices[3];
	indices[0] = vert1;
	indices[1] = vert2;
	indices[2] = vert3;
	RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indices, 3);
}
void
RenderPrimitive(PrimitiveType primType)
{
	engine->d3ddevice.im3DRenderPrimitive(primType);
}
void
RenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices)
{
	engine->d3ddevice.im3DRenderIndexedPrimitive(primType, indices, numIndices);
}
void
End(void)
{
	engine->d3ddevice.im3DEnd();
}

}

}

