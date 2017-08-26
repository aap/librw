#ifdef RW_OPENGL

namespace rw {
namespace gl3 {

// TODO: make this dynamic
enum {
	MAX_UNIFORMS = 20,
	MAX_BLOCKS = 20
};

struct UniformRegistry
{
	int32 numUniforms;
	char *uniformNames[MAX_UNIFORMS];

	int32 numBlocks;
	char *blockNames[MAX_BLOCKS];
};

int32 registerUniform(const char *name);
int32 findUniform(const char *name);
int32 registerBlock(const char *name);
int32 findBlock(const char *name);

extern UniformRegistry uniformRegistry;

class Shader
{
public:
	GLuint program;
	// same number of elements as UniformRegistry::numUniforms
	GLint *uniformLocations;

	static Shader *fromFiles(const char *vs, const char *fs);
	static Shader *fromStrings(const char *vsrc, const char *fsrc);
	void use(void);
};

extern Shader *currentShader;

}
}

#endif
