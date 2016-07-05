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

struct UniformState
{
	int     alphaFunc;
	float32 alphaRef;
};

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
GLuint ubo_state, ubo_scene, ubo_object;
GLuint whitetex;
UniformState uniformState;
UniformScene uniformScene;
UniformObject uniformObject;

Shader *simpleShader;

static bool32 stateDirty = 1;
static bool32 sceneDirty = 1;
static bool32 objectDirty = 1;

// cached render states
static bool32 vertexAlpha;
static bool32 textureAlpha;
static uint32 srcblend, destblend;
static uint32 zwrite;
static uint32 ztest;

uint32 blendMap[] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA_SATURATE,
};

void
setRenderState(int32 state, uint32 value)
{
	switch(state){
	case VERTEXALPHA:
		if(vertexAlpha != value){
			vertexAlpha = value;
			if(vertexAlpha)
				glEnable(GL_BLEND);
			else if(!textureAlpha)
				glDisable(GL_BLEND);
		}
		break;
	case SRCBLEND:
		if(srcblend != value){
			srcblend = value;
			glBlendFunc(blendMap[srcblend], blendMap[destblend]);
		}
		break;
	case DESTBLEND:
		if(destblend != value){
			destblend = value;
			glBlendFunc(blendMap[srcblend], blendMap[destblend]);
		}
		break;
	case ZTESTENABLE:
		if(ztest != value){
			ztest = value;
			if(ztest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}
		break;
	case ZWRITEENABLE:
		if(zwrite != (value ? GL_TRUE : GL_FALSE)){
			zwrite = value ? GL_TRUE : GL_FALSE;
			glDepthMask(zwrite);
		}
		break;

	case ALPHATESTFUNC:
		uniformState.alphaFunc = value;
		stateDirty = 1;
		break;
	case ALPHATESTREF:
		uniformState.alphaRef = value/255.0f;
		stateDirty = 1;
		break;
	case ZTESTFUNC:
		break;
	}
}

uint32
getRenderState(int32 state)
{
	switch(state){
	case VERTEXALPHA:
		return vertexAlpha;
	case SRCBLEND:
		return srcblend;
	case DESTBLEND:
		return destblend;
	case ZTESTENABLE:
		return ztest;
	case ZWRITEENABLE:
		return zwrite;

	case ALPHATESTFUNC:
		return uniformState.alphaFunc;
	case ALPHATESTREF:
		return uniformState.alphaRef*255.0f;
	case ZTESTFUNC:
		break;
	}
}

void
resetRenderState(void)
{
	uniformState.alphaFunc = ALPHAGREATERTHAN;
	uniformState.alphaRef = 10.0f/255.0f;
	stateDirty = 1;

	vertexAlpha = 0;
	textureAlpha = 0;
	glDisable(GL_BLEND);

	srcblend = BLENDSRCALPHA;
	destblend = BLENDINVSRCALPHA;
	glBlendFunc(blendMap[srcblend], blendMap[destblend]);

	zwrite = GL_TRUE;
	glDepthMask(zwrite);

	ztest = 1;
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	for(int i = 0; i < 8; i++){
		glActiveTexture(GL_TEXTURE0+i);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

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

	if(textureAlpha != alpha){
		textureAlpha = alpha;
		if(textureAlpha)
			glEnable(GL_BLEND);
		else if(!vertexAlpha)
			glDisable(GL_BLEND);
	}
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
	if(stateDirty){
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_state);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(UniformState),
				&uniformState);
		stateDirty = 0;
	}
}

void
beginUpdate(Camera *cam)
{
	float view[16], proj[16];
	// View Matrix
	Matrix inv;
	// TODO: this can be simplified, or we use matrix flags....
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
	driver[PLATFORM_GL3]->beginUpdate = beginUpdate;
	driver[PLATFORM_GL3]->setRenderState = setRenderState;
	driver[PLATFORM_GL3]->getRenderState = getRenderState;

	simpleShader = Shader::fromFiles("simple.vert", "simple.frag");

	glClearColor(0.25, 0.25, 0.25, 1.0);

	resetRenderState();

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &ubo_state);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_state);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("State"), ubo_state);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformState), &uniformState,
	             GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


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
}
}

#endif
