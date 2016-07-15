namespace rw {
namespace gl3 {

void initializePlatform(void);
void initializeRender(void);

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

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;	// not used for rendering
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

void setAttribPointers(InstanceDataHeader *header);

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
void lightingCB(void);

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
};

void registerNativeRaster(void);

}
}
