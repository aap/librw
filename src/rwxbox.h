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

// Skin plugin

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

ObjPipeline *makeSkinPipeline(void);

ObjPipeline *makeMatFXPipeline(void);

// Vertex Format plugin

extern uint32 vertexFormatSizes[6];

uint32 *getVertexFmt(Geometry *g);
uint32 makeVertexFmt(int32 flags, uint32 numTexSets);
uint32 getVertexFmtStride(uint32 fmt);

void registerVertexFormatPlugin(void);

// Native Texture and Raster

void makeNativeRaster(Raster *raster);
uint8 *lockRaster(Raster *raster, int32 level);
void unlockRaster(Raster *raster, int32 level);
int32 getNumLevels(Raster *raster);
int32 getLevelSize(Raster *raster, int32 level);

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

struct XboxRaster
{
	void *texture;
	void *palette;
	uint32 format;
	bool32 hasAlpha;
	bool32 unknownFlag;
//	int32 compression;
};

extern int32 nativeRasterOffset;
void registerNativeRaster(void);

}
}
