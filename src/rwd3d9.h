#ifdef RW_D3D9
#include <d3d9.h>
#endif

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

#ifdef RW_D3D9
extern IDirect3DDevice9 *device;
#endif

extern int vertFormatMap[];

uint16 *lockIndices(void *indexBuffer, uint32 offset, uint32 size, uint32 flags);
void unlockIndices(void *indexBuffer);
uint8 *lockVertices(void *vertexBuffer, uint32 offset, uint32 size, uint32 flags);
void unlockVertices(void *vertexBuffer);
void *createVertexDeclaration(VertexElement *elements);
uint32 getDeclaration(void *declaration, VertexElement *elements);
void *createIndexBuffer(uint32 length);
void *createVertexBuffer(uint32 length, int32 pool);

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

	ObjPipeline(uint32 platform);
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
};

ObjPipeline *makeDefaultPipeline(void);

ObjPipeline *makeSkinPipeline(void);

ObjPipeline *makeMatFXPipeline(void);

}
}
