#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#include "rwgl3.h"
#include "rwgl3shader.h"

#include "rwgl3impl.h"

namespace rw {
namespace gl3 {

#define MAX_LIGHTS 8

void
setAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	AttribDesc *a;
	for(a = attribDescs; a != &attribDescs[numAttribs]; a++){
		glEnableVertexAttribArray(a->index);
		glVertexAttribPointer(a->index, a->size, a->type, a->normalized,
		                      a->stride, (void*)(uint64)a->offset);
	}
}

void
disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs)
{
	AttribDesc *a;
	for(a = attribDescs; a != &attribDescs[numAttribs]; a++)
		glDisableVertexAttribArray(a->index);
}

void
lightingCB(bool32 normals)
{
	World *world;
	RGBAf ambLight = { 0.0, 0.0, 0.0, 1.0 };
	int n = 0;

	world = (World*)engine->currentWorld;
	// only unpositioned lights right now
	FORLIST(lnk, world->directionalLights){
		Light *l = Light::fromWorld(lnk);
		if(normals &&
		   l->getType() == Light::DIRECTIONAL &&
		   l->getFlags() & Light::LIGHTATOMICS){
			if(n >= MAX_LIGHTS)
				continue;
			setLight(n++, l);
		}else if(l->getType() == Light::AMBIENT){
			ambLight.red   += l->color.red;
			ambLight.green += l->color.green;
			ambLight.blue  += l->color.blue;
		}
	}
	setNumLights(n);
	setAmbientLight(&ambLight);
}

#define U(i) currentShader->uniformLocations[i]

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	Material *m;
	RGBAf col;
	GLfloat surfProps[4];

	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(!!(atomic->geometry->flags & Geometry::NORMALS));

	glBindBuffer(GL_ARRAY_BUFFER, header->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, header->ibo);
	setAttribPointers(header->attribDesc, header->numAttribs);

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	simpleShader->use();

	while(n--){
		m = inst->material;

		convColor(&col, &m->color);
		glUniform4fv(U(u_matColor), 1, (GLfloat*)&col);

		surfProps[0] = m->surfaceProps.ambient;
		surfProps[1] = m->surfaceProps.specular;
		surfProps[2] = m->surfaceProps.diffuse;
		surfProps[3] = 0.0f;
		glUniform4fv(U(u_surfaceProps), 1, surfProps);

		setTexture(0, m->texture);

		rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

		flushCache();
		glDrawElements(header->primType, inst->numIndex,
		               GL_UNSIGNED_SHORT, (void*)(uintptr)inst->offset);
		inst++;
	}
	disableAttribPointers(header->attribDesc, header->numAttribs);
}


}
}

#endif

