#ifndef MESH_H
#define MESH_H

#include "ps2.h"
#include "math.h"

enum meshAttribs {
	MESHATTR_VERTICES  = 0x1,
	MESHATTR_NORMALS   = 0x2,
	MESHATTR_COLORS    = 0x4,
	MESHATTR_TEXCOORDS = 0x8,
	MESHATTR_INDEXED   = 0x10
};

typedef struct Mesh Mesh;
struct Mesh {
/*
	Vector3f *vertices;
	Vector3f *normals;
	Vector4b *colors;
	Vector2f *texCoords;
*/
	int primitive;
	enum meshAttribs attribs;

	int vertexCount;
	float **vertices;
	float **normals;
	uint8 **colors;
	float **texCoords;

	int indexCount;
	int *indices;

};

void meshDump(Mesh *m);
void meshDraw(Mesh *m);

extern Mesh testmesh;
extern Mesh playermesh;

#endif
