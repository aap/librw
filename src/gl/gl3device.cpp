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
#include <GLFW/glfw3.h>
#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3impl.h"

#define PLUGIN_ID 0

namespace rw {
namespace gl3 {

struct GlGlobals
{
	GLFWwindow *window;
	int presentWidth, presentHeight;
} glGlobals;

struct UniformState
{
	int32   alphaFunc;
	float32 alphaRef;
	int32   fogEnable;
	float32 fogStart;
	float32 fogEnd;
	int32   pad[3];
	RGBAf   fogColor;
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
	RawMatrix    world;
	RGBAf        ambLight;
	int32        numLights;
	int32        pad[3];
	UniformLight lights[MAX_LIGHTS];
};

static GLuint vao;
static GLuint ubo_state, ubo_scene, ubo_object;
static GLuint whitetex;
static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;

int32 u_matColor;
int32 u_surfaceProps;

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

static int32 activeTexture;

static uint32 blendMap[] = {
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

static void
setVertexAlpha(bool32 enable)
{
	if(vertexAlpha != enable){
		vertexAlpha = enable;
		if(!textureAlpha){
			if(enable)
				glEnable(GL_BLEND);
			else
				glDisable(GL_BLEND);
		}
	}
}

static void
setRenderState(int32 state, uint32 value)
{
	switch(state){
	case VERTEXALPHA:
		setVertexAlpha(value);
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
	case FOGENABLE:
		if(uniformState.fogEnable != value){
			uniformState.fogEnable = value;
			stateDirty = 1;
		}
		break;
	case FOGCOLOR:
		// no cache check here...too lazy
		convColor(&uniformState.fogColor, (RGBA*)&value);
		stateDirty = 1;
		break;

	case ALPHATESTFUNC:
		if(uniformState.alphaFunc != value){
			uniformState.alphaFunc = value;
			stateDirty = 1;
		}
		break;
	case ALPHATESTREF:
		if(uniformState.alphaRef != value/255.0f){
			uniformState.alphaRef = value/255.0f;
			stateDirty = 1;
		}
		break;
	}
}

static uint32
getRenderState(int32 state)
{
	RGBA rgba;
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
	case FOGENABLE:
		return uniformState.fogEnable;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		return *(uint32*)&rgba;

	case ALPHATESTFUNC:
		return uniformState.alphaFunc;
	case ALPHATESTREF:
		return (uint32)(uniformState.alphaRef*255.0f);
	}
	return 0;
}

static void
resetRenderState(void)
{
	uniformState.alphaFunc = ALPHAGREATEREQUAL;
	uniformState.alphaRef = 10.0f/255.0f;
	uniformState.fogEnable = 0;
	uniformState.fogStart = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
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
	convMatrix(&uniformObject.world, mat);
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
	l->w = light->getType() >= Light::POINT ? 1.0f : 0.0f;
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

static void
setActiveTexture(int32 n)
{
	if(activeTexture != n){
		activeTexture = n;
		glActiveTexture(n);
	}
}

void
setTexture(int32 n, Texture *tex)
{
	// TODO: support mipmaps
	static GLint filternomip[] = {
		0, GL_NEAREST, GL_LINEAR,
		   GL_NEAREST, GL_LINEAR,
		   GL_NEAREST, GL_LINEAR
	};

	static GLint wrap[] = {
		0, GL_REPEAT, GL_MIRRORED_REPEAT,
		GL_CLAMP, GL_CLAMP_TO_BORDER
	};
	bool32 alpha;
	setActiveTexture(GL_TEXTURE0+n);
	if(tex == nil || tex->raster->platform != PLATFORM_GL3 ||
	   tex->raster->width == 0){
		glBindTexture(GL_TEXTURE_2D, whitetex);
		alpha = 0;
	}else{
		Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, tex->raster,
		                                 nativeRasterOffset);
		glBindTexture(GL_TEXTURE_2D, natras->texid);
		alpha = natras->hasAlpha;
		if(tex->filterAddressing != natras->filterAddressing){
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filternomip[tex->getFilter()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filternomip[tex->getFilter()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap[tex->getAddressU()]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap[tex->getAddressV()]);
			natras->filterAddressing = tex->filterAddressing;
		}
	}

	if(n == 0){
		if(alpha != textureAlpha){
			textureAlpha = alpha;
			if(!vertexAlpha){
				if(alpha)
					glEnable(GL_BLEND);
				else
					glDisable(GL_BLEND);
			}
		}
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

static void
clearCamera(Camera *cam, RGBA *col, uint32 mode)
{
	RGBAf colf;
	GLbitfield mask;

	convColor(&colf, col);
	glClearColor(colf.red, colf.green, colf.blue, colf.alpha);
	mask = 0;
	if(mode & Camera::CLEARIMAGE)
		mask |= GL_COLOR_BUFFER_BIT;
	if(mode & Camera::CLEARZ)
		mask |= GL_DEPTH_BUFFER_BIT;
	glClear(mask);
}

static void
showRaster(Raster *raster)
{
	// TODO: do this properly!
	glfwSwapBuffers(glGlobals.window);
}

static void
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
	memcpy(&cam->devView, &view, sizeof(RawMatrix));
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

	proj[8] = cam->viewOffset.x*invwx;
	proj[9] = cam->viewOffset.y*invwy;
	proj[12] = -proj[8];
	proj[13] = -proj[9];
	if(cam->projection == Camera::PERSPECTIVE){
		proj[10] = (cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 1.0f;

		proj[14] = -2.0f*cam->nearPlane*cam->farPlane*invz;
		proj[15] = 0.0f;
	}else{
		proj[10] = -(cam->farPlane+cam->nearPlane)*invz;
		proj[11] = 0.0f;

		proj[14] = -2.0f*invz;
		proj[15] = 1.0f;
	}
	memcpy(&cam->devProj, &proj, sizeof(RawMatrix));
	setProjectionMatrix(proj);

	if(uniformState.fogStart != cam->fogPlane){
		uniformState.fogStart = cam->fogPlane;
		stateDirty = 1;
	}
	if(uniformState.fogEnd != cam->farPlane){
		uniformState.fogEnd = cam->farPlane;
		stateDirty = 1;
	}

	int w, h;
	glfwGetWindowSize(glGlobals.window, &w, &h);
	if(w != glGlobals.presentWidth || h != glGlobals.presentHeight){
		glViewport(0, 0, w, h);
		glGlobals.presentWidth = w;
		glGlobals.presentHeight = h;
	}
}

static int
openGLFW(EngineStartParams *startparams)
{
	GLenum status;
	GLFWwindow *win;

	/* Init GLFW */
	if(!glfwInit()){
		RWERROR((ERR_GENERAL, "glfwInit() failed"));
		return 0;
	}
	glfwWindowHint(GLFW_SAMPLES, 0);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	win = glfwCreateWindow(startparams->width, startparams->height, startparams->windowtitle, 0, 0);
	if(win == nil){
		RWERROR((ERR_GENERAL, "glfwCreateWindow() failed"));
		glfwTerminate();
		return 0;
	}
	glfwMakeContextCurrent(win);

	/* Init GLEW */
	glewExperimental = GL_TRUE;
	status = glewInit();
	if(status != GLEW_OK){
		RWERROR((ERR_GENERAL, glewGetErrorString(status)));
		glfwDestroyWindow(win);
		glfwTerminate();
		return 0;
	}
	if(!GLEW_VERSION_3_3){
		RWERROR((ERR_GENERAL, "OpenGL 3.3 needed"));
		glfwDestroyWindow(win);
		glfwTerminate();
		return 0;
	}
	glGlobals.window = win;
	*startparams->window = win;
	return 1;
}

static int
closeGLFW(void)
{
	glfwDestroyWindow(glGlobals.window);
	glfwTerminate();
	return 1;
}

static int
initOpenGL(void)
{
	registerBlock("Scene");
	registerBlock("Object");
	registerBlock("State");
	u_matColor = registerUniform("u_matColor");
	u_surfaceProps = registerUniform("u_surfaceProps");

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

#include "shaders/simple_vs_gl3.inc"
#include "shaders/simple_fs_gl3.inc"
	simpleShader = Shader::fromStrings(simple_vert_src, simple_frag_src);

	openIm2D();
	openIm3D();

	return 1;
}

static int
termOpenGL(void)
{
	closeIm3D();
	closeIm2D();
	return 1;
}

static int
finalizeOpenGL(void)
{
	return 1;
}

static int
deviceSystem(DeviceReq req, void *arg0)
{
	switch(req){
	case DEVICEOPEN:
		return openGLFW((EngineStartParams*)arg0);
	case DEVICECLOSE:
		return closeGLFW();

	case DEVICEINIT:
		return initOpenGL();
	case DEVICETERM:
		return termOpenGL();

	case DEVICEFINALIZE:
		return finalizeOpenGL();
	}
	return 1;
}

Device renderdevice = {
	-1.0f, 1.0f,
	gl3::beginUpdate,
	null::endUpdate,
	gl3::clearCamera,
	gl3::showRaster,
	gl3::setRenderState,
	gl3::getRenderState,
	gl3::im2DRenderIndexedPrimitive,
	gl3::im3DTransform,
	gl3::im3DRenderIndexed,
	gl3::im3DEnd,
	gl3::deviceSystem
};

}
}

#endif

