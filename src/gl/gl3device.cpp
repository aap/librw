#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
#ifndef LIBRW_SDL2
struct DisplayMode
{
	GLFWvidmode mode;
	int32 depth;
	uint32 flags;
};
#endif

struct GlGlobals
{
#ifdef LIBRW_SDL2
	SDL_Window *window;
	SDL_GLContext glcontext;
#else
	GLFWwindow *window;

	GLFWmonitor *monitor;
	int numMonitors;
	int currentMonitor;

	DisplayMode *modes;
	int numModes;
	int currentMode;
	GLFWwindow **pWindow;
#endif
	int presentWidth, presentHeight;

	// for opening the window
	int winWidth, winHeight;
	const char *winTitle;
} glGlobals;

int32   alphaFunc;
float32 alphaRef;

struct UniformState
{
	float32 alphaRefLow;
	float32 alphaRefHigh;
	float32 fogStart;
	float32 fogEnd;

	float32 fogRange;
	float32 fogDisable;
	int32   pad[2];

	RGBAf   fogColor;
};

struct UniformScene
{
	float32 proj[16];
	float32 view[16];
};

#define MAX_LIGHTS 8

struct UniformObject
{
	RawMatrix    world;
	RGBAf        ambLight;
	struct {
		float type;
		float radius;
		float minusCosAngle;
		float hardSpot;
	} lightParams[MAX_LIGHTS];
	V4d lightPosition[MAX_LIGHTS];
	V4d lightDirection[MAX_LIGHTS];
	RGBAf lightColor[MAX_LIGHTS];
};

const char *shaderDecl330 = "#version 330\n";
const char *shaderDecl100es =
"#version 100\n"\
"precision highp float;\n"\
"precision highp int;\n";
const char *shaderDecl310es =
"#version 310 es\n"\
"precision highp float;\n"\
"precision highp int;\n";

#ifdef RW_GLES3
const char *shaderDecl = shaderDecl310es;
#elif defined RW_GLES2
const char *shaderDecl = shaderDecl100es;
#else
const char *shaderDecl = shaderDecl330;
#endif

// this needs a define in the shaders as well!
//#define RW_GL_USE_UBOS

#ifndef RW_GLES2
static GLuint vao;
#endif
#ifdef RW_GL_USE_UBOS
static GLuint ubo_state, ubo_scene, ubo_object;
#endif
static GLuint whitetex;
static UniformState uniformState;
static UniformScene uniformScene;
static UniformObject uniformObject;

#ifndef RW_GL_USE_UBOS
// State
int32 u_alphaRef;
int32 u_fogStart;
int32 u_fogEnd;
int32 u_fogRange;
int32 u_fogDisable;
int32 u_fogColor;

// Scene
int32 u_proj;
int32 u_view;

// Object
int32 u_world;
int32 u_ambLight;
int32 u_lightParams;
int32 u_lightPosition;
int32 u_lightDirection;
int32 u_lightColor;
#endif

int32 u_matColor;
int32 u_surfProps;

Shader *defaultShader;

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
	bool32 blendEnable;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;

	// emulation of PS2 GS
	bool32 gsalpha;
	uint32 gsalpharef;

	RwRasterStateCache texstage[MAXNUMSTAGES];
};
static RwStateCache rwStateCache;

static int32 activeTexture;
static uint32 boundTexture[MAXNUMSTAGES];

static uint32 blendMap[] = {
	GL_ZERO,	// actually invalid
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
setAlphaBlend(bool32 enable)
{
	if(rwStateCache.blendEnable != enable){
		rwStateCache.blendEnable = enable;
		(enable ? glEnable : glDisable)(GL_BLEND);
	}
}

bool32
getAlphaBlend(void)
{
	return rwStateCache.blendEnable;
}

static void
setDepthTest(bool32 enable)
{
	if(rwStateCache.ztest != enable){
		rwStateCache.ztest = enable;
		if(rwStateCache.zwrite && !enable){
			// If we still want to write, enable but set mode to always
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);
		}else{
			if(rwStateCache.ztest)
				glEnable(GL_DEPTH_TEST);
			else
				glDisable(GL_DEPTH_TEST);
			glDepthFunc(GL_LEQUAL);
		}
	}
}

static void
setDepthWrite(bool32 enable)
{
	enable = enable ? GL_TRUE : GL_FALSE;
	if(rwStateCache.zwrite != enable){
		rwStateCache.zwrite = enable;
		if(enable && !rwStateCache.ztest){
			// Have to switch on ztest so writing can work
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);
		}
		glDepthMask(rwStateCache.zwrite);
	}
}

static void
setAlphaTest(bool32 enable)
{
	uint32 shaderfunc;
	if(rwStateCache.alphaTestEnable != enable){
		rwStateCache.alphaTestEnable = enable;
		shaderfunc = rwStateCache.alphaTestEnable ? rwStateCache.alphaFunc : ALPHAALWAYS;
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
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
		if(alphaFunc != shaderfunc){
			alphaFunc = shaderfunc;
			stateDirty = 1;
		}
	}
}

static void
setVertexAlpha(bool32 enable)
{
	if(rwStateCache.vertexAlpha != enable){
		if(!rwStateCache.textureAlpha){
			setAlphaBlend(enable);
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
		glActiveTexture(GL_TEXTURE0+n);
	}
}

uint32
bindTexture(uint32 texid)
{
	uint32 prev = boundTexture[activeTexture];
	boundTexture[activeTexture] = texid;
	glBindTexture(GL_TEXTURE_2D, texid);
	return prev;
}

// TODO: support mipmaps
static GLint filterConvMap_NoMIP[] = {
	0, GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR,
	   GL_NEAREST, GL_LINEAR
};

static GLint addressConvMap[] = {
	0, GL_REPEAT, GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER
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
		setActiveTexture(stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			bindTexture(natras->texid);

			rwStateCache.texstage[stage].filter = (rw::Texture::FilterMode)natras->filterMode;
			rwStateCache.texstage[stage].addressingU = (rw::Texture::Addressing)natras->addressU;
			rwStateCache.texstage[stage].addressingV = (rw::Texture::Addressing)natras->addressV;

			alpha = natras->hasAlpha;
		}else{
			bindTexture(whitetex);
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
					setAlphaBlend(alpha);
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
		setActiveTexture(stage);
		if(raster){
			assert(raster->platform == PLATFORM_GL3);
			Gl3Raster *natras = PLUGINOFFSET(Gl3Raster, raster, nativeRasterOffset);
			bindTexture(natras->texid);
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
			if(natras->addressV != addrV){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, addressConvMap[addrV]);
				natras->addressV = addrV;
			}
			alpha = natras->hasAlpha;
		}else{
			bindTexture(whitetex);
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
					setAlphaBlend(alpha);
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
		setDepthTest(value);
		break;
	case ZWRITEENABLE:
		setDepthWrite(value);
		break;
	case FOGENABLE:
		if(rwStateCache.fogEnable != value){
			rwStateCache.fogEnable = value;
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
		if(alphaRef != value/255.0f){
			alphaRef = value/255.0f;
			stateDirty = 1;
		}
		break;
	case GSALPHATEST:
		rwStateCache.gsalpha = value;
		break;
	case GSALPHATESTREF:
		rwStateCache.gsalpharef = value;
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
		val = rwStateCache.fogEnable;
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
		val = (uint32)(alphaRef*255.0f);
		break;
	case GSALPHATEST:
		val = rwStateCache.gsalpha;
		break;
	case GSALPHATESTREF:
		val = rwStateCache.gsalpharef;
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
	alphaFunc = 0;
	alphaRef = 10.0f/255.0f;
	uniformState.fogDisable = 1.0f;
	uniformState.fogStart = 0.0f;
	uniformState.fogEnd = 0.0f;
	uniformState.fogRange = 0.0f;
	uniformState.fogColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	rwStateCache.gsalpha = 0;
	rwStateCache.gsalpharef = 128;
	stateDirty = 1;

	rwStateCache.vertexAlpha = 0;
	rwStateCache.textureAlpha = 0;
	rwStateCache.alphaTestEnable = 0;

	rwStateCache.blendEnable = 0;
	glDisable(GL_BLEND);
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

	activeTexture = -1;
	for(int i = 0; i < MAXNUMSTAGES; i++){
		setActiveTexture(i);
		bindTexture(whitetex);
	}
	setActiveTexture(0);
}

void
setWorldMatrix(Matrix *mat)
{
	convMatrix(&uniformObject.world, mat);
	objectDirty = 1;
}

int32
setLights(WorldLights *lightData)
{
	int i, n;
	Light *l;
	int32 bits;

	uniformObject.ambLight = lightData->ambient;

	bits = 0;

	if(lightData->numAmbients)
		bits |= VSLIGHT_AMBIENT;

	n = 0;
	for(i = 0; i < lightData->numDirectionals && i < 8; i++){
		l = lightData->directionals[i];
		uniformObject.lightParams[n].type = 1.0f;
		uniformObject.lightColor[n] = l->color;
		memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
		bits |= VSLIGHT_POINT;
		n++;
		if(n >= MAX_LIGHTS)
			goto out;
	}

	for(i = 0; i < lightData->numLocals; i++){
		Light *l = lightData->locals[i];

		switch(l->getType()){
		case Light::POINT:
			uniformObject.lightParams[n].type = 2.0f;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			bits |= VSLIGHT_POINT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		case Light::SPOT:
		case Light::SOFTSPOT:
			uniformObject.lightParams[n].type = 3.0f;
			uniformObject.lightParams[n].minusCosAngle = l->minusCosAngle;
			uniformObject.lightParams[n].radius = l->radius;
			uniformObject.lightColor[n] = l->color;
			memcpy(&uniformObject.lightPosition[n], &l->getFrame()->getLTM()->pos, sizeof(V3d));
			memcpy(&uniformObject.lightDirection[n], &l->getFrame()->getLTM()->at, sizeof(V3d));
			// lower bound of falloff
			if(l->getType() == Light::SOFTSPOT)
				uniformObject.lightParams[n].hardSpot = 0.0f;
			else
				uniformObject.lightParams[n].hardSpot = 1.0f;
			bits |= VSLIGHT_SPOT;
			n++;
			if(n >= MAX_LIGHTS)
				goto out;
			break;
		}
	}

	uniformObject.lightParams[n].type = 0.0f;
out:
	objectDirty = 1;
	return bits;
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

Shader *lastShaderUploaded;

void
flushCache(void)
{
#ifndef RW_GL_USE_UBOS
#define U(i) currentShader->uniformLocations[i]

	// TODO: this is probably a stupid way to do it without UBOs
	if(lastShaderUploaded != currentShader){
		lastShaderUploaded = currentShader;
		objectDirty = 1;
		sceneDirty = 1;
		stateDirty = 1;
	}

	if(objectDirty){
		glUniformMatrix4fv(U(u_world), 1, 0, (float*)&uniformObject.world);
		glUniform4fv(U(u_ambLight), 1, (float*)&uniformObject.ambLight);
		glUniform4fv(U(u_lightParams), MAX_LIGHTS, (float*)uniformObject.lightParams);
		glUniform4fv(U(u_lightPosition), MAX_LIGHTS, (float*)uniformObject.lightPosition);
		glUniform4fv(U(u_lightDirection), MAX_LIGHTS, (float*)uniformObject.lightDirection);
		glUniform4fv(U(u_lightColor), MAX_LIGHTS, (float*)uniformObject.lightColor);
		objectDirty = 0;
	}

	if(sceneDirty){
		glUniformMatrix4fv(U(u_proj), 1, 0, uniformScene.proj);
		glUniformMatrix4fv(U(u_view), 1, 0, uniformScene.view);
		sceneDirty = 0;
	}

	if(stateDirty){
		uniformState.fogDisable = rwStateCache.fogEnable ? 0.0f : 1.0f;
		uniformState.fogStart = rwStateCache.fogStart;
		uniformState.fogEnd = rwStateCache.fogEnd;
		uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd);

		switch(alphaFunc){
		case ALPHAALWAYS:
		default:
			glUniform2f(U(u_alphaRef), -1000.0f, 1000.0f);
			break;
		case ALPHAGREATEREQUAL:
			glUniform2f(U(u_alphaRef), alphaRef, 1000.0f);
			break;
		case ALPHALESS:
			glUniform2f(U(u_alphaRef), -1000.0f, alphaRef);
			break;
		}

		glUniform1f(U(u_fogStart), uniformState.fogStart);
		glUniform1f(U(u_fogEnd), uniformState.fogEnd);
		glUniform1f(U(u_fogRange), uniformState.fogRange);
		glUniform1f(U(u_fogDisable), uniformState.fogDisable);
		glUniform4fv(U(u_fogColor), 1, (float*)&uniformState.fogColor);
		stateDirty = 0;
	}
#else
	if(objectDirty){
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_object);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformObject), nil, GL_STREAM_DRAW);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformObject), &uniformObject, GL_STREAM_DRAW);
		objectDirty = 0;
	}
	if(sceneDirty){
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformScene), nil, GL_STREAM_DRAW);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformScene), &uniformScene, GL_STREAM_DRAW);
		sceneDirty = 0;
	}
	if(stateDirty){
		switch(alphaFunc){
		case ALPHAALWAYS:
		default:
			uniformState.alphaRefLow = -1000.0f;
			uniformState.alphaRefHigh = 1000.0f;
			break;
		case ALPHAGREATEREQUAL:
			uniformState.alphaRefLow = alphaRef;
			uniformState.alphaRefHigh = 1000.0f;
			break;
		case ALPHALESS:
			uniformState.alphaRefLow = -1000.0f;
			uniformState.alphaRefHigh = alphaRef;
			break;
		}
		uniformState.fogDisable = rwStateCache.fogEnable ? 0.0f : 1.0f;
		uniformState.fogStart = rwStateCache.fogStart;
		uniformState.fogEnd = rwStateCache.fogEnd;
		uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_state);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformState), nil, GL_STREAM_DRAW);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformState), &uniformState, GL_STREAM_DRAW);
		stateDirty = 0;
	}
#endif
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
	glDepthMask(GL_TRUE);
	glClear(mask);
	glDepthMask(rwStateCache.zwrite);
}

static void
showRaster(Raster *raster, uint32 flags)
{
	// TODO: do this properly!
#ifdef LIBRW_SDL2
	SDL_GL_SwapWindow(glGlobals.window);
#else
	if(flags & Raster::FLIPWAITVSYNCH)
		glfwSwapInterval(1);
	else
		glfwSwapInterval(0);
	glfwSwapBuffers(glGlobals.window);
#endif
}

static bool32
rasterRenderFast(Raster *raster, int32 x, int32 y)
{
	Raster *src = raster;
	Raster *dst = Raster::getCurrentContext();
	Gl3Raster *natdst = PLUGINOFFSET(Gl3Raster, dst, nativeRasterOffset);
	Gl3Raster *natsrc = PLUGINOFFSET(Gl3Raster, src, nativeRasterOffset);

	switch(dst->type){
	case Raster::NORMAL:
	case Raster::TEXTURE:
	case Raster::CAMERATEXTURE:
		switch(src->type){
		case Raster::CAMERA:
			setActiveTexture(0);
			glBindTexture(GL_TEXTURE_2D, natdst->texid);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, (dst->height-src->height)-y,
				0, 0, src->width, src->height);
			glBindTexture(GL_TEXTURE_2D, boundTexture[0]);
			return 1;
		}
		break;
	}
	return 0;
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

	if(rwStateCache.fogStart != cam->fogPlane){
		rwStateCache.fogStart = cam->fogPlane;
		stateDirty = 1;
	}
	if(rwStateCache.fogEnd != cam->farPlane){
		rwStateCache.fogEnd = cam->farPlane;
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
openSDL2(EngineOpenParams *openparams)
{
	if (!openparams){
		RWERROR((ERR_GENERAL, "openparams invalid"));
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
	if (openparams->fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;
	win = SDL_CreateWindow(openparams->windowtitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, openparams->width, openparams->height, flags);
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
	*openparams->window = win;
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

static void
addVideoMode(const GLFWvidmode *mode)
{
	int i;

	for(i = 1; i < glGlobals.numModes; i++){
		if(glGlobals.modes[i].mode.width == mode->width &&
		   glGlobals.modes[i].mode.height == mode->height &&
		   glGlobals.modes[i].mode.redBits == mode->redBits &&
		   glGlobals.modes[i].mode.greenBits == mode->greenBits &&
		   glGlobals.modes[i].mode.blueBits == mode->blueBits){
			// had this mode already, remember highest refresh rate
			if(mode->refreshRate > glGlobals.modes[i].mode.refreshRate)
				glGlobals.modes[i].mode.refreshRate = mode->refreshRate;
			return;
		}
	}

	// none found, add
	glGlobals.modes[glGlobals.numModes].mode = *mode;
	glGlobals.modes[glGlobals.numModes].flags = VIDEOMODEEXCLUSIVE;
	glGlobals.numModes++;
}

static void
makeVideoModeList(void)
{
	int i, num;
	const GLFWvidmode *modes;

	modes = glfwGetVideoModes(glGlobals.monitor, &num);
	rwFree(glGlobals.modes);
	glGlobals.modes = rwNewT(DisplayMode, num, ID_DRIVER | MEMDUR_EVENT);

	glGlobals.modes[0].mode = *glfwGetVideoMode(glGlobals.monitor);
	glGlobals.modes[0].flags = 0;
	glGlobals.numModes = 1;

	for(i = 0; i < num; i++)
		addVideoMode(&modes[i]);

	for(i = 0; i < glGlobals.numModes; i++){
		num = glGlobals.modes[i].mode.redBits +
			glGlobals.modes[i].mode.greenBits +
			glGlobals.modes[i].mode.blueBits;
		// set depth to power of two
		for(glGlobals.modes[i].depth = 1; glGlobals.modes[i].depth < num; glGlobals.modes[i].depth <<= 1);
	}
}

static int
openGLFW(EngineOpenParams *openparams)
{
	glGlobals.winWidth = openparams->width;
	glGlobals.winHeight = openparams->height;
	glGlobals.winTitle = openparams->windowtitle;
	glGlobals.pWindow = openparams->window;

	/* Init GLFW */
	if(!glfwInit()){
		RWERROR((ERR_GENERAL, "glfwInit() failed"));
		return 0;
	}
	glfwWindowHint(GLFW_SAMPLES, 0);
#ifdef RW_GLES3
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#elif defined RW_GLES2
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#else
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

	glGlobals.monitor = glfwGetMonitors(&glGlobals.numMonitors)[0];

	makeVideoModeList();

	return 1;
}

static int
closeGLFW(void)
{
	glfwTerminate();
	return 1;
}

static void
glfwerr(int error, const char *desc)
{
	fprintf(stderr, "GLFW Error: %s\n", desc);
}

static int
startGLFW(void)
{
	GLenum status;
	GLFWwindow *win;
	DisplayMode *mode;

	mode = &glGlobals.modes[glGlobals.currentMode];

	glfwSetErrorCallback(glfwerr);
	glfwWindowHint(GLFW_RED_BITS, mode->mode.redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->mode.greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->mode.blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->mode.refreshRate);

	if(mode->flags & VIDEOMODEEXCLUSIVE)
		win = glfwCreateWindow(mode->mode.width, mode->mode.height, glGlobals.winTitle, glGlobals.monitor, nil);
	else
		win = glfwCreateWindow(glGlobals.winWidth, glGlobals.winHeight, glGlobals.winTitle, nil, nil);
	if(win == nil){
		RWERROR((ERR_GENERAL, "glfwCreateWindow() failed"));
		return 0;
	}
	glfwMakeContextCurrent(win);
printf("version %s\n", glGetString(GL_VERSION));

	/* Init GLEW */
	glewExperimental = GL_TRUE;
	status = glewInit();
	if(status != GLEW_OK){
		RWERROR((ERR_GENERAL, glewGetErrorString(status)));
		glfwDestroyWindow(win);
		return 0;
	}
	if(!GLEW_VERSION_3_3){
		RWERROR((ERR_GENERAL, "OpenGL 3.3 needed"));
		glfwDestroyWindow(win);
		return 0;
	}
	glGlobals.window = win;
	*glGlobals.pWindow = win;
	glGlobals.presentWidth = 0;
	glGlobals.presentHeight = 0;
	return 1;
}

static int
stopGLFW(void)
{
	glfwDestroyWindow(glGlobals.window);
	return 1;
}
#endif

static int
initOpenGL(void)
{
#ifndef RW_GL_USE_UBOS
	u_alphaRef = registerUniform("u_alphaRef");
	u_fogStart = registerUniform("u_fogStart");
	u_fogEnd = registerUniform("u_fogEnd");
	u_fogRange = registerUniform("u_fogRange");
	u_fogDisable = registerUniform("u_fogDisable");
	u_fogColor = registerUniform("u_fogColor");
	u_proj = registerUniform("u_proj");
	u_view = registerUniform("u_view");
	u_world = registerUniform("u_world");
	u_ambLight = registerUniform("u_ambLight");
	u_lightParams = registerUniform("u_lightParams");
	u_lightPosition = registerUniform("u_lightPosition");
	u_lightDirection = registerUniform("u_lightDirection");
	u_lightColor = registerUniform("u_lightColor");
	lastShaderUploaded = nil;
#else
	registerBlock("Scene");
	registerBlock("Object");
	registerBlock("State");
#endif
	u_matColor = registerUniform("u_matColor");
	u_surfProps = registerUniform("u_surfProps");

	glClearColor(0.25, 0.25, 0.25, 1.0);

	byte whitepixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	glGenTextures(1, &whitetex);
	glBindTexture(GL_TEXTURE_2D, whitetex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1,
	             0, GL_RGBA, GL_UNSIGNED_BYTE, &whitepixel);

	resetRenderState();

#ifndef RW_GLES2
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
#endif

#ifdef RW_GL_USE_UBOS
	glGenBuffers(1, &ubo_state);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_state);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("State"), ubo_state);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformState), &uniformState,
	             GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glGenBuffers(1, &ubo_scene);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_scene);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("Scene"), ubo_scene);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformScene), &uniformScene,
	             GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glGenBuffers(1, &ubo_object);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_object);
	glBindBufferBase(GL_UNIFORM_BUFFER, gl3::findBlock("Object"), ubo_object);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(UniformObject), &uniformObject,
	             GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
#endif

#ifdef RW_GLES2
#include "gl2_shaders/default_vs_gl2.inc"
#include "gl2_shaders/simple_fs_gl2.inc"
#else
#include "shaders/default_vs_gl3.inc"
#include "shaders/simple_fs_gl3.inc"
#endif
	const char *vs[] = { shaderDecl, header_vert_src, default_vert_src, nil };
	const char *fs[] = { shaderDecl, header_frag_src, simple_frag_src, nil };
	defaultShader = Shader::create(vs, fs);
	assert(defaultShader);

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

#ifdef LIBRW_SDL2
static int
deviceSystemSDL2(DeviceReq req, void *arg, int32 n)
{
	switch(req){
	case DEVICEOPEN:
		return openSDL2((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeSDL2();

	case DEVICEINIT:
		return initOpenGL();
	case DEVICETERM:
		return termOpenGL();

	// TODO: implement subsystems and video modes

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

#else

static int
deviceSystemGLFW(DeviceReq req, void *arg, int32 n)
{
	GLFWmonitor **monitors;
	VideoMode *rwmode;

	switch(req){
	case DEVICEOPEN:
		return openGLFW((EngineOpenParams*)arg);
	case DEVICECLOSE:
		return closeGLFW();

	case DEVICEINIT:
		return startGLFW() && initOpenGL();
	case DEVICETERM:
		return termOpenGL() && stopGLFW();

	case DEVICEFINALIZE:
		return finalizeOpenGL();


	case DEVICEGETNUMSUBSYSTEMS:
		return glGlobals.numMonitors;

	case DEVICEGETCURRENTSUBSYSTEM:
		return glGlobals.currentMonitor;

	case DEVICESETSUBSYSTEM:
		monitors = glfwGetMonitors(&glGlobals.numMonitors);
		if(n >= glGlobals.numMonitors)
			return 0;
		glGlobals.currentMonitor = n;
		glGlobals.monitor = monitors[glGlobals.currentMonitor];
		return 1;

	case DEVICEGETSUBSSYSTEMINFO:
		monitors = glfwGetMonitors(&glGlobals.numMonitors);
		if(n >= glGlobals.numMonitors)
			return 0;
		strncpy(((SubSystemInfo*)arg)->name, glfwGetMonitorName(monitors[n]), sizeof(SubSystemInfo::name));
		return 1;


	case DEVICEGETNUMVIDEOMODES:
		return glGlobals.numModes;

	case DEVICEGETCURRENTVIDEOMODE:
		return glGlobals.currentMode;

	case DEVICESETVIDEOMODE:
		if(n >= glGlobals.numModes)
			return 0;
		glGlobals.currentMode = n;
		return 1;

	case DEVICEGETVIDEOMODEINFO:
		rwmode = (VideoMode*)arg;
		rwmode->width = glGlobals.modes[n].mode.width;
		rwmode->height = glGlobals.modes[n].mode.height;
		rwmode->depth = glGlobals.modes[n].depth;
		rwmode->flags = glGlobals.modes[n].flags;
		return 1;

	default:
		assert(0 && "not implemented");
		return 0;
	}
	return 1;
}

#endif

Device renderdevice = {
	-1.0f, 1.0f,
	gl3::beginUpdate,
	null::endUpdate,
	gl3::clearCamera,
	gl3::showRaster,
	gl3::rasterRenderFast,
	gl3::setRenderState,
	gl3::getRenderState,
	gl3::im2DRenderLine,
	gl3::im2DRenderTriangle,
	gl3::im2DRenderPrimitive,
	gl3::im2DRenderIndexedPrimitive,
	gl3::im3DTransform,
	gl3::im3DRenderPrimitive,
	gl3::im3DRenderIndexedPrimitive,
	gl3::im3DEnd,
#ifdef LIBRW_SDL2
	gl3::deviceSystemSDL2
#else
	gl3::deviceSystemGLFW
#endif
};

}
}

#endif

