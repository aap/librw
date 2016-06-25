#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwplugins.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

struct UniformLight
{
	V3d     position;
	float32 w;
	V3d     direction;
	int32   pad1;
	RGBAf   color;
	float32 radius;
	float32 minusCosAngle;
	int32   pad2[2];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	Matrix world;
	RGBAf  ambLight;
	int32  numLights;
	int32  pad[3];
	UniformLight lights[MAX_LIGHTS];
};

GLuint vao;
GLuint ubo_scene, ubo_object;
GLuint whitetex;
UniformScene uniformScene;
UniformObject uniformObject;

void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	Matrix::invert(&inv, cam->getFrame()->getLTM());
	// Since we're looking into positive Z,
	// flip X to ge a left handed view space.
	view[0]  = -inv.right.x;
	view[1]  =  inv.right.y;
	view[2]  =  inv.right.z;
	view[3]  =  0.0f;
	view[4]  = -inv.up.x;
	view[5]  =  inv.up.y;
	view[6]  =  inv.up.z;
	view[7]  =  0.0f;
	view[8]  =  -inv.at.x;
	view[9]  =   inv.at.y;
	view[10] =  inv.at.z;
	view[11] =  0.0f;
	view[12] = -inv.pos.x;
	view[13] =  inv.pos.y;
	view[14] =  inv.pos.z;
	view[15] =  1.0f;
	setViewMatrix(view);

	// Projection Matrix
        float32 invwx = 1.0f/cam->viewWindow.x;
        float32 invwy = 1.0f/cam->viewWindow.y;
        float32 invz = 1.0f/(cam->farPlane-cam->nearPlane);

	proj[0] = invwx;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = invwy;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	if(cam->projection == Camera::PERSPECTIVE){
		proj[8] = cam->viewOffset.x*invwx;
		proj[9] = cam->viewOffset.y*invwy;
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[12] = 0.0f;
		proj[13] = 0.0f;
		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		// TODO
	}
	setProjectionMatrix(proj);
}

void
initializeRender(void)
{
        driver[PLATFORM_GL3].beginUpdate = beginUpdate;

        glClearColor(0.25, 0.25, 0.25, 1.0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	registerBlock("Scene");
	registerBlock("Object");
	registerUniform("u_matColor");
	registerUniform("u_surfaceProps");

	glGenBuffers(1, &ubo_scene);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("Scene"), ubo_scene);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformScene), &uniformScene,
	             GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


	glGenBuffers(1, &ubo_object);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_object);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("Object"), ubo_object);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformObject), &uniformObject,
	             GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


	byte whitepixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	glGenTextures(1, &whitetex);
	glBindTexture(GL_TEXTURE_2D, whitetex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, &whitepixel);
}

void
setAttribPointers(InstanceDataHeader *header)
{
	AttribDesc *a;
	for(a = header->attribDesc;
	    a != &header->attribDesc[header->numAttribs];
	    a++){
		glEnableVertexAttribArray(a->index);
		glVertexAttribPointer(a->index, a->size, a->type, a->normalized,
		                      a->stride, (void*)(uint64)a->offset);
	}
}

static bool32 sceneDirty = 1;
static bool32 objectDirty = 1;

void
setWorldMatrix(Matrix *mat)
{
	uniformObject.world = *mat;
	objectDirty = 1;
}

void
setAmbientLight(RGBAf *amb)
{
	uniformObject.ambLight = *amb;
	objectDirty = 1;
}

void
setNumLights(int32 n)
{
	uniformObject.numLights = n;
	objectDirty = 1;
}

void
setLight(int32 n, Light *light)
{
	UniformLight *l;
	Frame *f;
	Matrix *m;

	l = &uniformObject.lights[n];
	f = light->getFrame();
	if(f){
		m = f->getLTM();
		l->position  = m->pos;
		l->direction = m->at;
	}
	// light has position
	l->w = light->getType() >= Light::POINT ? 1.0f : 0.0;
	l->color = light->color;
	l->radius = light->radius;
	l->minusCosAngle = light->minusCosAngle;
	objectDirty = 1;
}

void
setProjectionMatrix(float32 *mat)
{
        memcpy(&uniformScene.proj, mat, 64);
	sceneDirty = 1;
}

void
setViewMatrix(float32 *mat)
{
        memcpy(&uniformScene.view, mat, 64);
	sceneDirty = 1;
}

static bool32 vertexAlpha;
static bool32 textureAlpha;

void
setTexture(int32 n, Texture *tex)
{
	bool32 alpha;
	glActiveTexture(GL_TEXTURE0+n);
	if(tex == nil){
		glBindTexture(GL_TEXTURE_2D, whitetex);
		alpha = 0;
	}else{
		Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, tex->raster,
		                                 nativeRasterOffset);
		glBindTexture(GL_TEXTURE_2D, natras->texid);
		alpha = natras->hasAlpha;
	}

	if(textureAlpha == alpha)
		return;
	if(alpha)
		/*printf("enable\n"),*/ glEnable(GL_BLEND);
	else if(!vertexAlpha)
		/*printf("disable\n"),*/ glDisable(GL_BLEND);
	textureAlpha = alpha;
}

void
setVertexAlpha(bool32 alpha)
{
	if(vertexAlpha == alpha)
		return;
	if(alpha)
		/*printf("enable\n"),*/ glEnable(GL_BLEND);
	else if(!textureAlpha)
		/*printf("disable\n"),*/ glDisable(GL_BLEND);
	vertexAlpha = alpha;
}


void
flushCache(void)
{
	if(objectDirty){
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_object);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UniformObject),
				&uniformObject);
		objectDirty = 0;
	}
	if(sceneDirty){
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UniformScene),
				&uniformScene);
		sceneDirty = 0;
	}
}

void
lightingCB(void)
{
	World *world;
	RGBAf ambLight = (RGBAf){0.0, 0.0, 0.0, 1.0};
	int n = 0;

	world = (World*)engine->currentWorld;
	// only unpositioned lights right now
	FORLIST(lnk, world->directionalLights){
		Light *l = Light::fromWorld(lnk);
		if(l->getType() == Light::DIRECTIONAL){
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

void
defaultRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB();

	glBindBuffer(GL_ARRAY_BUFFER, header->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, header->ibo);
	setAttribPointers(header);

	Material *m;
	RGBAf col;
	GLfloat surfProps[4];
	int id;
	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		m = inst->material;

#define U(s) currentShader->uniformLocations[findUniform(s)]

		convColor(&col, &m->color);
		glUniform4fv(U("u_matColor"), 1, (GLfloat*)&col);

		surfProps[0] = m->surfaceProps.ambient;
		surfProps[1] = m->surfaceProps.specular;
		surfProps[2] = m->surfaceProps.diffuse;
		surfProps[3] = 0.0f;
		glUniform4fv(U("u_surfaceProps"), 1, surfProps);

		setTexture(0, m->texture);

		setVertexAlpha(inst->vertexAlpha || m->color.alpha != 0xFF);

		flushCache();
		glDrawElements(header->primType, inst->numIndex,
		               GL_UNSIGNED_SHORT, (void*)(uintptr)inst->offset);
		inst++;
	}
}


}
}

#endif
