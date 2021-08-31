#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#ifdef RW_OPENGL
#include "rwgl3.h"
#include "rwgl3shader.h"

namespace rw {
namespace gl3 {

#include "shaders/header_vs.inc"
#include "shaders/header_fs.inc"

UniformRegistry uniformRegistry;
static char nameBuffer[(MAX_UNIFORMS + MAX_BLOCKS)*32];	// static because memory system isn't up yet when we register
static uint32 nameBufPtr;
static float uniformData[512*4];	// seems enough
static uint32 dataPtr;

static int uniformTypesize[] = {
	0, 4, 4, 16
};

static char*
shader_strdup(const char *name)
{
	size_t len = strlen(name)+1;
	char *s = &nameBuffer[nameBufPtr];
	nameBufPtr += len;
	assert(nameBufPtr <= nelem(nameBuffer));
	memcpy(s, name, len);
	return s;
}

int32
registerUniform(const char *name, UniformType type, int32 num)
{
	int i;
	i = findUniform(name);
	if(i >= 0){
		Uniform *u = &uniformRegistry.uniforms[i];
		assert(u->type == type);
		assert(u->num == num);
		return i;
	}
	// TODO: print error
	if(uniformRegistry.numUniforms+1 >= MAX_UNIFORMS){
		assert(0 && "no space for uniform");
		return -1;
	}
	Uniform *u = &uniformRegistry.uniforms[uniformRegistry.numUniforms];
	u->name = shader_strdup(name);
	u->type = type;
	u->serialNum = 0;
	if(type == UNIFORM_NA){
		u->num = 0;
		u->data = nil;
	}else{
		u->num = num;
		u->data = &uniformData[dataPtr];
		dataPtr += uniformTypesize[type]*num;
		assert(dataPtr <= nelem(uniformData));
	}
	
	return uniformRegistry.numUniforms++;
}

int32
findUniform(const char *name)
{
	int i;
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		if(strcmp(name, uniformRegistry.uniforms[i].name) == 0)
			return i;
	return -1;
}

int32
registerBlock(const char *name)
{
	int i;
	i = findBlock(name);
	if(i >= 0) return i;
	// TODO: print error
	if(uniformRegistry.numBlocks+1 >= MAX_BLOCKS)
		return -1;
	uniformRegistry.blockNames[uniformRegistry.numBlocks] = shader_strdup(name);
	return uniformRegistry.numBlocks++;
}

int32
findBlock(const char *name)
{
	int i;
	for(i = 0; i < uniformRegistry.numBlocks; i++)
		if(strcmp(name, uniformRegistry.blockNames[i]) == 0)
			return i;
	return -1;
}

void
setUniform(int32 id, void *data)
{
	Uniform *u = &uniformRegistry.uniforms[id];
	assert(u->type != UNIFORM_NA);
	if(memcmp(u->data, data, uniformTypesize[u->type]*u->num * sizeof(float)) != 0){
		memcpy(u->data, data, uniformTypesize[u->type]*u->num * sizeof(float));
		//u->dirty = true;
		u->serialNum++;
	}
}

void
flushUniforms(void)
{
	for(int i = 0; i < uniformRegistry.numUniforms; i++){
		// this is bad!
		if(i >= currentShader->numUniforms){
			printf("trying to set uniform %d %s that doesn't exist!\n", i, uniformRegistry.uniforms[i].name);
			continue;
		}

		int32 loc = currentShader->uniformLocations[i];
		if(loc == -1)
			continue;

		Uniform *u = &uniformRegistry.uniforms[i];
		if(currentShader->serialNums[i] != u->serialNum)
			switch(u->type){
			case UNIFORM_NA:
				break;
			case UNIFORM_VEC4:
				glUniform4fv(loc, u->num, (GLfloat*)u->data);
				break;
			case UNIFORM_IVEC4:
				glUniform4iv(loc, u->num, (GLint*)u->data);
				break;
			case UNIFORM_MAT4:
				glUniformMatrix4fv(loc, u->num, GL_FALSE, (GLfloat*)u->data);
				break;
			}
		currentShader->serialNums[i] = u->serialNum;
	}
}

Shader *currentShader;

static void
printShaderSource(const char **src)
{
	int f;
	const char *file;
	bool printline;
	int line = 1;
	for(f = 0; src[f]; f++){
		int fileline = 1;
		char c;
		file = src[f];
		printline = true;
		while(c = *file++, c != '\0'){
			if(printline)
				printf("%.4d/%d:%.4d: ", line++, f, fileline++);
			putchar(c);
			printline = c == '\n';
		}
		putchar('\n');
	}
}

static int
compileshader(GLenum type, const char **src, GLuint *shader)
{
	GLint n;
	GLint shdr, success;
	GLint len;
	char *log;

	for(n = 0; src[n]; n++);

	shdr = glCreateShader(type);
	glShaderSource(shdr, n, src, nil);
	glCompileShader(shdr);
	glGetShaderiv(shdr, GL_COMPILE_STATUS, &success);
	if(!success){
		printShaderSource(src);
		fprintf(stderr, "Error in %s shader\n",
		  type == GL_VERTEX_SHADER ? "vertex" : "fragment");
		glGetShaderiv(shdr, GL_INFO_LOG_LENGTH, &len);
		log = (char*)rwMalloc(len, MEMDUR_FUNCTION);
		glGetShaderInfoLog(shdr, len, nil, log);
		fprintf(stderr, "%s\n", log);
		rwFree(log);
		return 1;
	}
	*shader = shdr;
	return 0;
}

static int
linkprogram(GLint vs, GLint fs, GLuint *program)
{
	GLint prog, success;
	GLint len;
	char *log;

	prog = glCreateProgram();

	if(gl3Caps.glversion < 30){
		// TODO: perhaps just do this always and get rid of the layout stuff?
		glBindAttribLocation(prog, ATTRIB_POS, "in_pos");
		glBindAttribLocation(prog, ATTRIB_NORMAL, "in_normal");
		glBindAttribLocation(prog, ATTRIB_COLOR, "in_color");
		glBindAttribLocation(prog, ATTRIB_WEIGHTS, "in_weights");
		glBindAttribLocation(prog, ATTRIB_INDICES, "in_indices");
		glBindAttribLocation(prog, ATTRIB_TEXCOORDS0, "in_tex0");
		glBindAttribLocation(prog, ATTRIB_TEXCOORDS1, "in_tex1");
	}

	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &success);
	if(!success){
		fprintf(stderr, "Error in program\n");
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
		log = (char*)rwMalloc(len, MEMDUR_FUNCTION);
		glGetProgramInfoLog(prog, len, nil, log);
		fprintf(stderr, "%s\n", log);
		rwFree(log);
		return 1;
	}
	*program = prog;
	return 0;
}

Shader*
Shader::create(const char **vsrc, const char **fsrc)
{
	GLuint vs, fs, program;
	int i;
	int fail;

	fail = compileshader(GL_VERTEX_SHADER, vsrc, &vs);
	if(fail)
		return nil;

	fail = compileshader(GL_FRAGMENT_SHADER, fsrc, &fs);
	if(fail){
		glDeleteShader(vs);
		return nil;
	}

	fail = linkprogram(vs, fs, &program);

	glDeleteShader(vs);
	glDeleteShader(fs);
	if(fail){
		return nil;
	}

	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);	 // or global?

#ifdef xxxRW_GLES2
	int numUniforms;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
	for(i = 0; i < numUniforms; i++){
		GLint size;
		GLenum type;
		char name[100];
		glGetActiveUniform(program, i, 100, nil, &size, &type, name);
		printf("%d %d %s\n", size, type, name);
	}
	printf("\n");
#endif

#ifdef xxxRW_GLES2
	int numAttribs;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttribs);
	for(i = 0; i < numAttribs; i++){
		GLint size;
		GLenum type;
		char name[100];
		glGetActiveAttrib(program, i, 100, nil, &size, &type, name);
		GLint bind = glGetAttribLocation(program, name);
		printf("%d %d %s. %d\n", size, type, name, bind);
	}
	printf("\n");
#endif

	// set uniform block binding
	for(i = 0; i < uniformRegistry.numBlocks; i++){
		int idx = glGetUniformBlockIndex(program,
		                                 uniformRegistry.blockNames[i]);
		if(idx >= 0)
			glUniformBlockBinding(program, idx, i);
	}

	// query uniform locations
	sh->program = program;
	sh->numUniforms = uniformRegistry.numUniforms;
	sh->uniformLocations = rwNewT(GLint, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	sh->serialNums = rwNewT(uint32, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(i = 0; i < uniformRegistry.numUniforms; i++){
		sh->uniformLocations[i] = glGetUniformLocation(program,
			uniformRegistry.uniforms[i].name);
		sh->serialNums[i] = ~0;	// let's hope this means dirty
	}

	// set samplers
	glUseProgram(program);
	char name[64];
	GLint loc;
	for(i = 0; i < 4; i++){
		sprintf(name, "tex%d", i);
		loc = glGetUniformLocation(program, name);
		glUniform1i(loc, i);
	}

	// reset program
	if(currentShader)
		glUseProgram(currentShader->program);

	return sh;
}

void
Shader::use(void)
{
	if(currentShader != this){
		glUseProgram(this->program);
		currentShader = this;
	}
}

void
Shader::destroy(void)
{
	glDeleteProgram(this->program);
	rwFree(this->uniformLocations);
	rwFree(this->serialNums);
	rwFree(this);
}

}
}

#endif
