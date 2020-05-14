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
#include "../rwanim.h"
#include "../rwplugins.h"
#ifdef RW_OPENGL
#include <GL/glew.h>
#endif
#include "rwgl3.h"
#include "rwgl3shader.h"
#include "rwgl3plg.h"

#include "rwgl3impl.h"

namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

#define U(i) currentShader->uniformLocations[i]

static Shader *envShader;
static int32 u_texMatrix;
static int32 u_coefficient;
static int32 u_colorClamp;

static void*
matfxOpen(void *o, int32, int32)
{
	u_texMatrix = registerUniform("u_texMatrix");
	u_coefficient = registerUniform("u_coefficient");
	u_colorClamp = registerUniform("u_colorClamp");
	matFXGlobals.pipelines[PLATFORM_GL3] = makeMatFXPipeline();

#ifdef RW_GLES2
#include "gl2_shaders/matfx_gl2.inc"
#else
#include "shaders/matfx_gl3.inc"
#endif
	const char *vs[] = { shaderDecl, header_vert_src, matfx_env_vert_src, nil };
	const char *fs[] = { shaderDecl, header_frag_src, matfx_env_frag_src, nil };
	envShader = Shader::create(vs, fs);
	assert(envShader);

	return o;
}

static void*
matfxClose(void *o, int32, int32)
{
	return o;
}

void
initMatFX(void)
{
	Driver::registerPlugin(PLATFORM_GL3, 0, ID_MATFX,
	                       matfxOpen, matfxClose);
}


void
matfxDefaultRender(InstanceDataHeader *header, InstanceData *inst)
{
	Material *m;
	RGBAf col;
	GLfloat surfProps[4];
	m = inst->material;

	defaultShader->use();

	convColor(&col, &m->color);
	glUniform4fv(U(u_matColor), 1, (GLfloat*)&col);

	surfProps[0] = m->surfaceProps.ambient;
	surfProps[1] = m->surfaceProps.specular;
	surfProps[2] = m->surfaceProps.diffuse;
	surfProps[3] = 0.0f;
	glUniform4fv(U(u_surfProps), 1, surfProps);

	setTexture(0, m->texture);

	rw::SetRenderState(VERTEXALPHA, inst->vertexAlpha || m->color.alpha != 0xFF);

	drawInst(header, inst);
}

static Frame *lastEnvFrame;

static RawMatrix normal2texcoord = {
	{ 0.5f,  0.0f, 0.0f }, 0.0f,
	{ 0.0f, -0.5f, 0.0f }, 0.0f,
	{ 0.0f,  0.0f, 1.0f }, 0.0f,
	{ 0.5f,  0.5f, 0.0f }, 1.0f
};

void
uploadEnvMatrix(Frame *frame)
{
	Matrix invMat;
	if(frame == nil)
		frame = engine->currentCamera->getFrame();

	// cache the matrix across multiple meshes
	static RawMatrix envMtx;
	if(frame != lastEnvFrame){
		lastEnvFrame = frame;

		RawMatrix invMtx;
		Matrix::invert(&invMat, frame->getLTM());
		convMatrix(&invMtx, &invMat);
		RawMatrix::mult(&envMtx, &invMtx, &normal2texcoord);
	}
	glUniformMatrix4fv(U(u_texMatrix), 1, GL_FALSE, (float*)&envMtx);
}

void
matfxEnvRender(InstanceDataHeader *header, InstanceData *inst, MatFX::Env *env)
{
	Material *m;
	RGBAf col;
	GLfloat surfProps[4];
	m = inst->material;

	if(env->tex == nil || env->coefficient == 0.0f){
		matfxDefaultRender(header, inst);
		return;
	}

	envShader->use();

	setTexture(0, m->texture);
	setTexture(1, env->tex);
	uploadEnvMatrix(env->frame);

	convColor(&col, &m->color);
	glUniform4fv(U(u_matColor), 1, (GLfloat*)&col);

	surfProps[0] = m->surfaceProps.ambient;
	surfProps[1] = m->surfaceProps.specular;
	surfProps[2] = m->surfaceProps.diffuse;
	surfProps[3] = 0.0f;
	glUniform4fv(U(u_surfProps), 1, surfProps);

	glUniform1fv(U(u_coefficient), 1, &env->coefficient);
	static float zero[4];
	static float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	// This clamps the vertex color below. With it we can achieve both PC and PS2 style matfx
	if(MatFX::modulateEnvMap)
		glUniform4fv(U(u_colorClamp), 1, zero);
	else
		glUniform4fv(U(u_colorClamp), 1, one);

	rw::SetRenderState(VERTEXALPHA, 1);
	rw::SetRenderState(SRCBLEND, BLENDONE);

	drawInst(header, inst);

	rw::SetRenderState(SRCBLEND, BLENDSRCALPHA);
}

void
matfxRenderCB(Atomic *atomic, InstanceDataHeader *header)
{
	setWorldMatrix(atomic->getFrame()->getLTM());
	lightingCB(atomic);

	glBindBuffer(GL_ARRAY_BUFFER, header->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, header->ibo);
	setAttribPointers(header->attribDesc, header->numAttribs);

	lastEnvFrame = nil;

	InstanceData *inst = header->inst;
	int32 n = header->numMeshes;

	while(n--){
		MatFX *matfx = MatFX::get(inst->material);

		if(matfx == nil)
			matfxDefaultRender(header, inst);
		else switch(matfx->type){
		case MatFX::ENVMAP:
			matfxEnvRender(header, inst, &matfx->fx[0].env);
			break;
		default:
			matfxDefaultRender(header, inst);
			break;
		}
		inst++;
	}
	disableAttribPointers(header->attribDesc, header->numAttribs);
}

ObjPipeline*
makeMatFXPipeline(void)
{
	ObjPipeline *pipe = new ObjPipeline(PLATFORM_GL3);
	pipe->instanceCB = defaultInstanceCB;
	pipe->uninstanceCB = defaultUninstanceCB;
	pipe->renderCB = matfxRenderCB;
	pipe->pluginID = ID_MATFX;
	pipe->pluginData = 0;
	return pipe;
}

#else

void initMatFX(void) { }

#endif

}
}

