namespace rw {
namespace xbox {

struct InstanceData
{
	uint32 minVert;
	int32 numVertices;
	int32 numIndices;
	void *indexBuffer;
	Material *material;
	uint32 vertexShader;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	int32 size;
	uint16 serialNumber;
	uint16 numMeshes;
	uint32 primType;
	int32 numVertices;
	int32 stride;
	void *vertexBuffer;
	bool32 vertexAlpha;
	InstanceData *begin;
	InstanceData *end;

	uint8 *data;
};

void *destroyNativeData(void *object, int32, int32);
void readNativeData(Stream *stream, int32 len, void *object, int32, int32);
void writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

// Skin plugin

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

// Vertex Format plugin

void registerVertexFormatPlugin(void);

}
}
