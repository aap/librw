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
	int numUniforms;
	char *uniformNames[MAX_UNIFORMS];

	int numBlocks;
	char *blockNames[MAX_BLOCKS];
};

int registerUniform(const char *name);
int findUniform(const char *name);
int registerBlock(const char *name);
int findBlock(const char *name);

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
