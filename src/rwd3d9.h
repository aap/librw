namespace rw {
namespace d3d9 {

struct VertexElement
{
	uint16   stream;
	uint16   offset;
	uint8    type;
	uint8    method;
	uint8    usage;
	uint8    usageIndex;
};

struct VertexStream
{
	void  *vertexBuffer;
	uint32 offset;
	uint32 stride;
	uint16 geometryFlags;
	uint8  managed;
	uint8  dynamicLock;
};

struct InstanceData
{
	uint32    numIndex;
	uint32    minVert;
	Material *material;
	bool32    vertexAlpha;
	void     *vertexShader;
	uint32    baseIndex;
	uint32    numVertices;
	uint32    startIndex;
	uint32    numPrimitives;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32  serialNumber;
	uint32  numMeshes;
	void   *indexBuffer;
	uint32  primType;
	VertexStream vertexStream[2];
	bool32  useOffsets;
	void   *vertexDeclaration;
	uint32  totalNumIndex;
	uint32  totalNumVertex;

	InstanceData *inst;
};

void *createVertexDeclaration(VertexElement *elements);
uint32 getDeclaration(void *declaration, VertexElement *elements);

void *destroyNativeData(void *object, int32, int32);
void readNativeData(Stream *stream, int32 len, void *object, int32, int32);
void writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

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

ObjPipeline *makeDefaultPipeline(void);

ObjPipeline *makeSkinPipeline(void);

ObjPipeline *makeMatFXPipeline(void);

// Native Texture and Raster

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

}
}
