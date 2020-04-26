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

#include "shaders/header_vs.inc"

UniformRegistry uniformRegistry;

int32
registerUniform(const char *name)
{
	int i;
	i = findUniform(name);
	if(i >= 0) return i;
	// TODO: print error
	if(uniformRegistry.numUniforms+1 >= MAX_UNIFORMS)
		return -1;
	uniformRegistry.uniformNames[uniformRegistry.numUniforms] = strdup(name);
	return uniformRegistry.numUniforms++;
}

int32
findUniform(const char *name)
{
	int i;
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		if(strcmp(name, uniformRegistry.uniformNames[i]) == 0)
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
	uniformRegistry.blockNames[uniformRegistry.numBlocks] = strdup(name);
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

Shader *currentShader;

// TODO: maybe make this public somewhere?
static char*
loadfile(const char *path)
{
	FILE *f;
	char *buf;
	long len;

	if(f = fopen(path, "rb"), f == nil){
		fprintf(stderr, "Couldn't open file %s\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	buf = (char*)rwMalloc(len+1, MEMDUR_EVENT);
	rewind(f);
	fread(buf, 1, len, f);
	buf[len] = '\0';
	fclose(f);
	return buf;
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
	if(fail)
		return nil;

	fail = linkprogram(vs, fs, &program);
	if(fail)
		return nil;
	glDeleteProgram(vs);
	glDeleteProgram(fs);

	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);	 // or global?

	// set uniform block binding
	for(i = 0; i < uniformRegistry.numBlocks; i++){
		int idx = glGetUniformBlockIndex(program,
		                                 uniformRegistry.blockNames[i]);
		if(idx >= 0)
			glUniformBlockBinding(program, idx, i);
	}

	// query uniform locations
	sh->program = program;
	sh->uniformLocations = rwNewT(GLint, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		sh->uniformLocations[i] = glGetUniformLocation(program,
			uniformRegistry.uniformNames[i]);

	// set samplers
	glUseProgram(program);
	char name[64];
	GLint loc;
	for(i = 0; i < 4; i++){
		sprintf(name, "tex%d", i);
		loc = glGetUniformLocation(program, name);
		glUniform1i(loc, i);
	}

	return sh;
}

#if 0
Shader*
Shader::fromStrings(const char *vsrc, const char *fsrc)
{
	GLuint vs, fs, program;
	int i;
	int fail;

	fail = compileshader(GL_VERTEX_SHADER, vsrc, &vs);
	if(fail)
		return nil;

	fail = compileshader(GL_FRAGMENT_SHADER, fsrc, &fs);
	if(fail)
		return nil;

	fail = linkprogram(vs, fs, &program);
	if(fail)
		return nil;
	glDeleteProgram(vs);
	glDeleteProgram(fs);

	Shader *sh = rwNewT(Shader, 1, MEMDUR_EVENT | ID_DRIVER);	 // or global?

	// set uniform block binding
	for(i = 0; i < uniformRegistry.numBlocks; i++){
		int idx = glGetUniformBlockIndex(program,
		                                 uniformRegistry.blockNames[i]);
		if(idx >= 0)
			glUniformBlockBinding(program, idx, i);
	}

	// query uniform locations
	sh->program = program;
	sh->uniformLocations = rwNewT(GLint, uniformRegistry.numUniforms, MEMDUR_EVENT | ID_DRIVER);
	for(i = 0; i < uniformRegistry.numUniforms; i++)
		sh->uniformLocations[i] = glGetUniformLocation(program,
			uniformRegistry.uniformNames[i]);

	return sh;
}

Shader*
Shader::fromFiles(const char *vspath, const char *fspath)
{
	char *vsrc, *fsrc;
	vsrc = loadfile(vspath);
	fsrc = loadfile(fspath);
	Shader *s = fromStrings(vsrc, fsrc);
	rwFree(vsrc);
	rwFree(fsrc);
	return s;
}
#endif

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
	rwFree(this);
}

}
}

#endif
