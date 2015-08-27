#include <kernel.h>
#include <stdio.h>
#include "ps2.h"
#include "gs.h"
#include "gif.h"

#include "math.h"
#include "mesh.h"

//Mesh testmesh =
//	#include "test.msh"
//
//Mesh playermesh =
//	#include "player.msh"

GIF_DECLARE_PACKET(meshGifBuf, 256)

void meshDump(Mesh *m)
{
//	int i;

	printf("primitive: %d\n"
	       "attribs: %d\n"
	       "vertexCount: %d\n", m->primitive, m->attribs, m->vertexCount);
	printf("vertices:\n");

/*
	for (i = 0; i < m->vertexCount; i++) {
		printf("%f %f %f\n", m->vertices[i][0],
		                     m->vertices[i][1],
		                     m->vertices[i][2]);
	}
*/
}

void meshDraw(Mesh *m)
{
	struct GsState *g = gsCurState;
	int i, j;
	Vector4f vf;
	Vector4f n;
	Vector4f lightdir = {1.0f, 1.0f, 1.0f };
	float intens;
	uint32   vi[3];
	uint8    c[4];
	int blockCount;
	int vertexCount;
	int blockVertCount;
	int vertexOffset;
	uint64 regs, numRegs;
	uint64 prim;
	int ind;

	vec3fNormalize(lightdir);

	if (m->attribs & MESHATTR_INDEXED)
		vertexCount = m->indexCount;
	else
		vertexCount = m->vertexCount;
	blockCount = vertexCount/39 + ((vertexCount%39) ? 1 : 0);

	regs = 0x5;	// XYZ
	numRegs = 1;
	if (m->attribs & MESHATTR_COLORS || m->attribs & MESHATTR_NORMALS) {
		regs = (regs << 4) | 0x1;	// RGBAQ
		numRegs++;
	}
	// TODO: texcoords and normals

	prim = MAKE_GS_PRIM(m->primitive,IIP_GOURAUD,0,0,0,0,0,0,0);

	vertexOffset = 0;
	for (i = 0; i < blockCount; i++) {
		blockVertCount = 39;
		if (i == blockCount-1) {
			blockVertCount = vertexCount % 39;
			if (blockVertCount == 0)
				blockVertCount = 39;
		}

		if (m->primitive == PRIM_TRI_STRIP && vertexOffset > 0) {
			vertexOffset -= 2;
			blockVertCount += 2;
		}

		GIF_BEGIN_PACKET(meshGifBuf);
		GIF_TAG(meshGifBuf,blockVertCount,1,1,prim,0,numRegs,regs);
		for (j = 0; j < blockVertCount; j++) {
			if (m->attribs & MESHATTR_INDEXED)
				ind = m->indices[j + vertexOffset];
			else
				ind = j + vertexOffset;
			vec4fCopy(vf, m->vertices[ind]);
			vf[3] = 1.0f;

			matMultVec(mathModelViewMat, vf);
			matMultVec(mathProjectionMat, vf);
			vf[0] /= vf[3];
			vf[1] /= vf[3];
			vf[2] /= vf[3];

			gsNormalizedToScreen(vf, vi);

			vi[0] = ((vi[0]*16) + g->xoff) & 0xFFFF;
			vi[1] = ((vi[1]*16) + g->yoff) & 0xFFFF;

			if (m->attribs & MESHATTR_COLORS) {
				c[0] = m->colors[ind][0];
				c[1] = m->colors[ind][1];
				c[2] = m->colors[ind][2];
				c[3] = m->colors[ind][3];

				GIF_DATA_RGBAQ(meshGifBuf,c[0],c[1],c[2],c[3]);
			} else if (m->attribs & MESHATTR_NORMALS) {
				vec4fCopy(n, m->normals[ind]);

				matMultVec(mathNormalMat, n);
				vec3fNormalize(n);
				intens = vec3fDot(n, lightdir);
				intens = intens > 0.0f ? intens : 0.0f;

				c[0] = intens * 255.0f;
				c[1] = intens * 0.0f;
				c[2] = intens * 255.0f;
				c[3] = 128.0f;

				GIF_DATA_RGBAQ(meshGifBuf,c[0],c[1],c[2],c[3]);
			}
			GIF_DATA_XYZ2(meshGifBuf,vi[0],vi[1],vi[2],0);
		}

		GIF_SEND_PACKET(meshGifBuf);
		vertexOffset += blockVertCount;
	}
}
