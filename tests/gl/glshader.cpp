#include "rwtest.h"

namespace gl {

GLint
linkProgram(GLint vertshader, GLint fragshader)
{
	GLint success;
	GLint len;
	char *log;

	GLint program = glCreateProgram();
	assert(program != 0);
	glBindAttribLocation(program, 0, "in_vertex");
	glBindAttribLocation(program, 1, "in_texCoord");
	glBindAttribLocation(program, 2, "in_normal");
	glBindAttribLocation(program, 3, "in_color");
	glBindAttribLocation(program, 4, "in_weight");
	glBindAttribLocation(program, 5, "in_indices");
	glBindAttribLocation(program, 6, "in_extraColor");
	glAttachShader(program, vertshader);
	glAttachShader(program, fragshader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if(!success){
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		log = new char[len];
		glGetProgramInfoLog(program, len, NULL, log);
		fprintf(stderr, "Link Error: %s\n", log);
		delete[] log;
		glDeleteProgram(program);
		return 0;
	}
	return program;
}

GLint
compileShader(const char **src, int count, int type)
{
	GLint success;
	GLint len;
	char *log;

	GLint shader = glCreateShader(type);
	assert(shader != 0);
	glShaderSource(shader, count, src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		log = new char[len];
		glGetShaderInfoLog(shader, len, NULL, log);
		fprintf(stderr, "Shader Error: %s\n", log);
		delete[] log;
		glDeleteProgram(shader);
		return 0;
	}
	return shader;
}

}
