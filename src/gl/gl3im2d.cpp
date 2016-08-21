#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

uint32 im2DVbo, im2DIbo;

#define STARTINDICES 1024
#define STARTVERTICES 1024

static Shader *im2dShader;
static AttribDesc attribDesc[3] = {
	{ ATTRIB_POS,        GL_FLOAT,         GL_FALSE, 3,
		sizeof(Im2DVertex), 0 },
	{ ATTRIB_COLOR,      GL_UNSIGNED_BYTE, GL_TRUE,  4,
		sizeof(Im2DVertex), offsetof(Im2DVertex, r) },
	{ ATTRIB_TEXCOORDS0, GL_FLOAT,         GL_FALSE, 2,
		sizeof(Im2DVertex), offsetof(Im2DVertex, r) },
};

static int primTypeMap[] = {
	GL_POINTS,	// invalid
	GL_LINES,
	GL_LINE_STRIP,
	GL_TRIANGLES,
	GL_TRIANGLE_STRIP,
	GL_TRIANGLE_FAN,
	GL_POINTS
};

void
im2DInit(void)
{
#include "shaders/im2d_gl3.inc"
	im2dShader = Shader::fromStrings(im2d_vert_src, im2d_frag_src);

	glGenBuffers(1, &im2DIbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, im2DIbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, STARTINDICES*2,
			nil, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glGenBuffers(1, &im2DVbo);
	glBindBuffer(GL_ARRAY_BUFFER, im2DVbo);
	glBufferData(GL_ARRAY_BUFFER, STARTVERTICES*sizeof(Im2DVertex),
			nil, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void
im2DRenderIndexedPrimitive(PrimitiveType primType,
	void *vertices, int32 numVertices,
	void *indices, int32 numIndices)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, im2DIbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices*2,
			indices, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, im2DVbo);
	glBufferData(GL_ARRAY_BUFFER, numVertices*sizeof(Im2DVertex),
			vertices, GL_DYNAMIC_DRAW);

	setAttribPointers(attribDesc, 3);
	im2dShader->use();

	setTexture(0, engine->imtexture);

	flushCache();
	glDrawElements(primTypeMap[primType], numIndices,
	               GL_UNSIGNED_SHORT, nil);
	disableAttribPointers(attribDesc, 3);
}

}
}

#endif
