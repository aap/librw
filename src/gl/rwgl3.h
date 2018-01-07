#ifdef RW_GL3
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#endif

namespace rw {

#ifdef RW_GL3
struct EngineStartParams
{
	GLFWwindow **window;
	int width, height;
	const char *windowtitle;
};
#endif

namespace gl3 {

void registerPlatformPlugins(void);

extern Device renderdevice;

// arguments to glVertexAttribPointer basically
struct AttribDesc
{
	uint32 index;
	int32  type;
	bool32 normalized;
	int32  size;
	uint32 stride;
	uint32 offset;
};

enum AttribIndices
{
	ATTRIB_POS = 0,
	ATTRIB_NORMAL,
	ATTRIB_COLOR,
	ATTRIB_TEXCOORDS0,
	ATTRIB_TEXCOORDS1,
	ATTRIB_TEXCOORDS2,
	ATTRIB_TEXCOORDS3,
	ATTRIB_TEXCOORDS4,
	ATTRIB_TEXCOORDS5,
	ATTRIB_TEXCOORDS6,
	ATTRIB_TEXCOORDS7,
};

// default uniform indices
extern int32 u_matColor;
extern int32 u_surfaceProps;

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;	// not used for rendering
	int32     numVertices;	//
	Material *material;
	bool32    vertexAlpha;
	uint32    program;
	uint32    offset;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32      serialNumber;	// not really needed right now
	uint32      numMeshes;
	uint16     *indexBuffer;
	uint32      primType;
	uint8      *vertexBuffer;
	int32       numAttribs;
	AttribDesc *attribDesc;
	uint32      totalNumIndex;
	uint32      totalNumVertex;

	uint32      ibo;
	uint32      vbo;		// or 2?

	InstanceData *inst;
};

#ifdef RW_GL3

struct Shader;

extern Shader *simpleShader;

struct Im3DVertex
{
	V3d     position;
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }
};

struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }
};

void setAttribPointers(AttribDesc *attribDescs, int32 numAttribs);
void disableAttribPointers(AttribDesc *attribDescs, int32 numAttribs);

// Render state

// per Scene
void setProjectionMatrix(float32*);
void setViewMatrix(float32*);

// per Object
void setWorldMatrix(Matrix*);
void setAmbientLight(RGBAf*);
void setNumLights(int32 n);
void setLight(int32 n, Light*);

// per Mesh
void setTexture(int32 n, Texture *tex);

void flushCache(void);

#endif

class ObjPipeline : public rw::ObjPipeline
{
public:
	void (*instanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*uninstanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*renderCB)(Atomic *atomic, InstanceDataHeader *header);

	ObjPipeline(uint32 platform);
};

void defaultInstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultRenderCB(Atomic *atomic, InstanceDataHeader *header);
void lightingCB(bool32 normals);

ObjPipeline *makeDefaultPipeline(void);

// Native Texture and Raster

extern int32 nativeRasterOffset;

struct Gl3Raster
{
	// arguments to glTexImage2D
	int32 internalFormat;
	int32 type;
	int32 format;
	// texture object
	uint32 texid;

	bool32 hasAlpha;
	// cached filtermode and addressing
	uint32 filterAddressing;
};

void registerNativeRaster(void);

}
}
