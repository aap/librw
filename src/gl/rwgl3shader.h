#ifdef RW_OPENGL

namespace rw {
namespace gl3 {

// TODO: make this dynamic
enum {
	MAX_UNIFORMS = 40,
	MAX_BLOCKS = 20
};

enum UniformType
{
	UNIFORM_NA,	// managed by the user
	UNIFORM_VEC4,
	UNIFORM_IVEC4,
	UNIFORM_MAT4
};

struct Uniform
{
	char *name;
	UniformType type;
	//bool dirty;
	uint32 serialNum;
	int32 num;
	void *data;
};

struct UniformRegistry
{
	int32 numUniforms;
	Uniform uniforms[MAX_UNIFORMS];

	int32 numBlocks;
	char *blockNames[MAX_BLOCKS];
};

int32 registerUniform(const char *name, UniformType type = UNIFORM_NA, int32 num = 1);
int32 findUniform(const char *name);
int32 registerBlock(const char *name);
int32 findBlock(const char *name);

void setUniform(int32 id, void *data);
void flushUniforms(void);

extern UniformRegistry uniformRegistry;

struct Shader
{
	GLuint program;
	// same number of elements as UniformRegistry::numUniforms
	GLint *uniformLocations;
	uint32 *serialNums;
	int32 numUniforms;	// just to be sure!

	static Shader *create(const char **vsrc, const char **fsrc);
//	static Shader *fromFiles(const char *vs, const char *fs);
//	static Shader *fromStrings(const char *vsrc, const char *fsrc);
	void use(void);
	void destroy(void);
};

extern Shader *currentShader;

}
}

#endif
