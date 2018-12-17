#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwrender.h"
#include "../rwengine.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#ifdef LIBRW_SDL2
#include <SDL.h>
#else
#include <GLFW/glfw3.h>
#endif
#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3impl.h"

#define PLUGIN_ID 0

namespace rw {
namespace gl3 {

struct GlGlobals
{
#ifdef LIBRW_SDL2
	SDL_Window *window;
	SDL_GLContext glcontext;
#else
	GLFWwindow *window;
#endif
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

struct RwRasterStateCache {
	Raster *raster;
	Texture::Addressing addressingU;
	Texture::Addressing addressingV;
	Texture::FilterMode filter;
};

#define MAXNUMSTAGES 8

// cached RW render states
struct RwStateCache {
	bool32 vertexAlpha;
	uint32 alphaTestEnable;
	uint32 alphaFunc;
	bool32 textureAlpha;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

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
setAlphaTest(bool32 enable)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaTestEnable != enable){
		rwStateCache.alphaTestEnable = enable;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(uniformState.alphaFunc != shaderfunc){
			uniformState.alphaFunc = shaderfunc;
			stateDirty = 1;
		}
	}
}

static void
setAlphaTestFunction(uint32 function)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaFunc != function){
		rwStateCache.alphaFunc = function;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(uniformState.alphaFunc != shaderfunc){
			uniformState.alphaFunc = shaderfunc;
			stateDirty = 1;
		}
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			(enable ? glEnable : glDisable)(GL_BLEND);
			setAlphaTest(enable);
		}
		rwStateCache.vertexAlpha = enable;
	}
}

static void
setActiveTexture(int32 n)
{
	if(activeTexture != n){
		activeTexture = n;
		glActiveTexture(n);
	}
}

// TODO: support mipmaps
static GLint filterConvMap_NoMIP[] = {
	0, GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR
};

static GLint addressConvMap[] = {
	0, GL_REPEAT, GL_MIRRORED_REPEAT,
	GL_CLAMP, GL_CLAMP_TO_BORDER
};

static void
setFilterMode(uint32 stage, int32 filter)
{
	if(rwStateCache.texstage[stage].filter != (Texture::FilterMode)filter){
		rwStateCache.texstage[stage].filter = (Texture::FilterMode)filter;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if(natras->filterMode != filter){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterConvMap_NoMIP[filter]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterConvMap_NoMIP[filter]);
				natras->filterMode = filter;
			}
		}
	}
}

static void
setAddressU(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingU != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingU = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			if(natras->addressU == addressing){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressConvMap[addressing]);
				natras->addressU = addressing;
			}
		}
	}
}

static void
setAddressV(uint32 stage, int32 addressing)
{
	if(rwStateCache.texstage[stage].addressingV != (Texture::Addressing)addressing){
		rwStateCache.texstage[stage].addressingV = (Texture::Addressing)addressing;
		Raster *raster = rwStateCache.texstage[stage].raster;
		if(raster){
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, rwStateCache.texstage[stage].raster, nativeRasterOffset);
			if(natras->addressV == addressing){
				setActiveTexture(stage);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressConvMap[addressing]);
				natras->addressV = addressing;
			}
		}
	}
}

static void
setRasterStageOnly(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		setActiveTexture(GL_TEXTURE0+stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			glBindTexture(GL_TEXTURE_2D, natras->texid);

			rwStateCache.texstage[stage].filter = (rw::Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (rw::Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (rw::Texture::Addressing)natras->addressV;

			alpha = natras->hasAlpha;
		}else{
			glBindTexture(GL_TEXTURE_2D, whitetex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					(alpha ? glEnable : glDisable)(GL_BLEND);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

static void
setRasterStage(uint32 stage, Raster *raster)
{
	bool32 alpha;
	if(raster != rwStateCache.texstage[stage].raster){
		rwStateCache.texstage[stage].raster = raster;
		setActiveTexture(GL_TEXTURE0+stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			glBindTexture(GL_TEXTURE_2D, natras->texid);
			uint32 filter = rwStateCache.texstage[stage].filter;
			uint32 addrU = rwStateCache.texstage[stage].addressingU;
			uint32 addrV = rwStateCache.texstage[stage].addressingV;
			if(natras->filterMode != filter){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterConvMap_NoMIP[filter]);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterConvMap_NoMIP[filter]);
				natras->filterMode = filter;
			}
			if(natras->addressU != addrU){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, addressConvMap[addrU]);
				natras->addressU = addrU;
			}
			if(natras->addressU != addrV){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressConvMap[addrV]);
				natras->addressV = addrV;
			}
			alpha = natras->hasAlpha;
		}else{
			glBindTexture(GL_TEXTURE_2D, whitetex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			alpha = 0;
		}

		if(stage == 0){
			if(alpha != rwStateCache.textureAlpha){
				rwStateCache.textureAlpha = alpha;
				if(!rwStateCache.vertexAlpha){
					(alpha ? glEnable : glDisable)(GL_BLEND);
					setAlphaTest(alpha);
				}
			}
		}
	}
}

void
setTexture(int32 stage, Texture *tex)
{
	if(tex == nil || tex->raster == nil){
		setRasterStage(stage, nil);
		return;
	}
	setRasterStageOnly(stage, tex->raster);
	setFilterMode(stage, tex->getFilter());
	setAddressU(stage, tex->getAddressU());
	setAddressV(stage, tex->getAddressV());
}

static void
setRenderState(int32 state, void *pvalue)
{
	uint32 value = (uint32)(uintptr)pvalue;
	switch(state){
	case TEXTURERASTER:
		setRasterStage(0, (Raster*)pvalue);
		break;
	case TEXTUREADDRESS:
		setAddressU(0, value);
		setAddressV(0, value);
		break;
	case TEXTUREADDRESSU:
		setAddressU(0, value);
		break;
	case TEXTUREADDRESSV:
		setAddressV(0, value);
		break;
	case TEXTUREFILTER:
		setFilterMode(0, value);
		break;
	case VERTEXALPHA:
		setVertexAlpha(value);
		break;
	case SRCBLEND:
		if(rwStateCache.srcblend != value){
			rwStateCache.srcblend = value;
			glBlendFunc(blendMap[rwStateCache.srcblend], blendMap[rwStateCache.destblend]);
		}
		break;
	case DESTBLEND:
		if(rwStateCache.destblend != value){
			rwStateCache.destblend = value;
			glBlendFunc(blendMap[rwStateCache.srcblend], blendMap[rwStateCache.destblend]);
		}
		break;
	case ZTESTENABLE:
		if(rwStateCache.ztest != value){
			rwStateCache.ztest = value;
			if(rwStateCache.ztest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
		}
		break;
	case ZWRITEENABLE:
		if(rwStateCache.zwrite != (value ? GL_TRUE : GL_FALSE)){
			rwStateCache.zwrite = value ? GL_TRUE : GL_FALSE;
			glDepthMask(rwStateCache.zwrite);
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
		RGBA c;
		c.red = value;
		c.green = value>>8;
		c.blue = value>>16;
		c.alpha = value>>24;
		convColor(&uniformState.fogColor, &c);
		stateDirty = 1;
		break;
	case CULLMODE:
		if(rwStateCache.cullmode != value){
			rwStateCache.cullmode = value;
			if(rwStateCache.cullmode == CULLNONE)
				glDisable(GL_CULL_FACE);
			else{
				glEnable(GL_CULL_FACE);
				glCullFace(rwStateCache.cullmode == CULLBACK ? GL_BACK : GL_FRONT);
			}
		}
		break;

	case ALPHATESTFUNC:
		setAlphaTestFunction(value);
		break;
	case ALPHATESTREF:
		if(uniformState.alphaRef != value/255.0f){
			uniformState.alphaRef = value/255.0f;
			stateDirty = 1;
		}
		break;
	}
}

static void*
getRenderState(int32 state)
{
	uint32 val;
	RGBA rgba;
	switch(state){
	case TEXTURERASTER:
		return rwStateCache.texstage[0].raster;
	case TEXTUREADDRESS:
		if(rwStateCache.texstage[0].addressingU == rwStateCache.texstage[0].addressingV)
			val = rwStateCache.texstage[0].addressingU;
		else
			val = 0;	// invalid
		break;
	case TEXTUREADDRESSU:
		val = rwStateCache.texstage[0].addressingU;
		break;
	case TEXTUREADDRESSV:
		val = rwStateCache.texstage[0].addressingV;
		break;
	case TEXTUREFILTER:
		val = rwStateCache.texstage[0].filter;
		break;

	case VERTEXALPHA:
		val = rwStateCache.vertexAlpha;
		break;
	case SRCBLEND:
		val = rwStateCache.srcblend;
		break;
	case DESTBLEND:
		val = rwStateCache.destblend;
		break;
	case ZTESTENABLE:
		val = rwStateCache.ztest;
		break;
	case ZWRITEENABLE:
		val = rwStateCache.zwrite;
		break;
	case FOGENABLE:
		val = uniformState.fogEnable;
		break;
	case FOGCOLOR:
		convColor(&rgba, &uniformState.fogColor);
		val = RWRGBAINT(rgba.red, rgba.green, rgba.blue, rgba.alpha);
		break;
	case CULLMODE:
		val = rwStateCache.cullmode;
		break;

	case ALPHATESTFUNC:
		val = rwStateCache.alphaFunc;
		break;
	case ALPHATESTREF:
		val = (uint32)(uniformState.alphaRef*255.0f);
		break;
	default:
		val = 0;
	}
	return (void*)(uintptr)val;
}

static void
resetRenderState(void)
{	
	rwStateCache.alphaFunc = ALPHAGREATEREQUAL;
	uniformState.alphaFunc = 0;
	uniformState.alphaRef = 10.0f/255.0f;
	uniformState.fogEnable = 0;
	uniformState.fogStart = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	stateDirty = 1;

	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;
	glDisable(GL_BLEND);
	rwStateCache.alphaTestEnable = 0;

	rwStateCache.srcblend = BLENDSRCALPHA;
	rwStateCache.destblend = BLENDINVSRCALPHA;
	glBlendFunc(blendMap[rwStateCache.srcblend], blendMap[rwStateCache.destblend]);

	rwStateCache.zwrite = GL_TRUE;
	glDepthMask(rwStateCache.zwrite);

	rwStateCache.ztest = 1;
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	rwStateCache.cullmode = CULLNONE;
	glDisable(GL_CULL_FACE);

	for(int i = 0; i < MAXNUMSTAGES; i++){
		glActiveTexture(GL_TEXTURE0+i);
		glBindTexture(GL_TEXTURE_2D, whitetex);
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
#ifdef LIBRW_SDL2
	SDL_GL_SwapWindow(glGlobals.window);
#else
	glfwSwapBuffers(glGlobals.window);
#endif
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
#ifdef LIBRW_SDL2
	SDL_GetWindowSize(glGlobals.window, &w, &h);
#else
	glfwGetWindowSize(glGlobals.window, &w, &h);
#endif
	if(w != glGlobals.presentWidth || h != glGlobals.presentHeight){
		glViewport(0, 0, w, h);
		glGlobals.presentWidth = w;
		glGlobals.presentHeight = h;
	}
}

#ifdef LIBRW_SDL2
static int
openSDL2(EngineStartParams *startparams)
{
	if (!startparams){
		RWERROR((ERR_GENERAL, "startparams invalid"));
		return 0;
	}

	GLenum status;
	SDL_Window *win;
	SDL_GLContext ctx;

	/* Init SDL */
	if(SDL_InitSubSystem(SDL_INIT_VIDEO)){
		RWERROR((ERR_ENGINEOPEN, SDL_GetError()));
		return 0;
	}
	SDL_ClearHints();
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
	if (startparams->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;
	win = SDL_CreateWindow(startparams->windowtitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, startparams->width, startparams->height, flags);
	if(win == nil){
		RWERROR((ERR_ENGINEOPEN, SDL_GetError()));
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 0;
	}
	ctx = SDL_GL_CreateContext(win);

	/* Init GLEW */
	glewExperimental = GL_TRUE;
	status = glewInit();
	if(status != GLEW_OK){
        	RWERROR((ERR_ENGINEOPEN, glewGetErrorString(status)));
        	SDL_GL_DeleteContext(ctx);
        	SDL_DestroyWindow(win);
        	SDL_QuitSubSystem(SDL_INIT_VIDEO);
        	return 0;
	}
	if(!GLEW_VERSION_3_3){
		RWERROR((ERR_VERSION, "OpenGL 3.3 needed"));
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return 0;
	}
	glGlobals.window = win;
	glGlobals.glcontext = ctx;
	*startparams->window = win;
	return 1;
}

static int
closeSDL2(void)
{
	SDL_GL_DeleteContext(glGlobals.glcontext);
	SDL_DestroyWindow(glGlobals.window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	return 1;
}
#else
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
	if(startparams->fullscreen){
		int nbmonitors;
		auto monitors = glfwGetMonitors(&nbmonitors);
        	if (nbmonitors){
			const GLFWvidmode* mode = glfwGetVideoMode(monitors[0]);
            		glfwSetWindowMonitor(win, monitors[0], 0, 0, mode->width, mode->height, mode->refreshRate);
        	}
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
#endif

static int
initOpenGL(void)
{
	registerBlock("Scene");
	registerBlock("Object");
	registerBlock("State");
	u_matColor = registerUniform("u_matColor");
	u_surfaceProps = registerUniform("u_surfaceProps");

	glClearColor(0.25, 0.25, 0.25, 1.0);

	byte whitepixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	glGenTextures(1, &whitetex);
	glBindTexture(GL_TEXTURE_2D, whitetex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, &whitepixel);

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
#ifdef LIBRW_SDL2
		return openSDL2((EngineStartParams*)arg0);
#else
		return openGLFW((EngineStartParams*)arg0);
#endif
	case DEVICECLOSE:
#ifdef LIBRW_SDL2
		return closeSDL2();
#else
		return closeGLFW();
#endif

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
	gl3::im2DRenderLine,
	gl3::im2DRenderTriangle,
	gl3::im2DRenderPrimitive,
	gl3::im2DRenderIndexedPrimitive,
	gl3::im3DTransform,
	gl3::im3DRenderIndexed,
	gl3::im3DEnd,
	gl3::deviceSystem
};

}
}

#endif

