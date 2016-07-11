namespace rw {

struct Atomic;

class Pipeline
{
public:
	uint32 pluginID;
	uint32 pluginData;
	int32 platform;

	Pipeline(uint32 platform);
};

class ObjPipeline : public Pipeline
{
public:
	ObjPipeline(uint32 platform);
	// not the most beautiful way of doing things but still
	// better than virtual methods (i hope?).
	struct {
		void (*instance)(ObjPipeline *pipe, Atomic *atomic);
		void (*uninstance)(ObjPipeline *pipe, Atomic *atomic);
		void (*render)(ObjPipeline *pipe, Atomic *atomic);
	} impl;
	// just for convenience
	void instance(Atomic *atomic) { this->impl.instance(this, atomic); }
	void uninstance(Atomic *atomic) { this->impl.uninstance(this, atomic); }
	void render(Atomic *atomic) { this->impl.render(this, atomic); }
};

void findMinVertAndNumVertices(uint16 *indices, uint32 numIndices, uint32 *minVert, int32 *numVertices);

// everything xbox, d3d8 and d3d9 may want to use
enum {
	VERT_BYTE2 = 1,
	VERT_BYTE3,
	VERT_SHORT2,
	VERT_SHORT3,
	VERT_NORMSHORT2,
	VERT_NORMSHORT3,
	VERT_FLOAT2,
	VERT_FLOAT3,
	VERT_FLOAT4,
	VERT_ARGB,
	VERT_RGBA,
	VERT_COMPNORM
};

void instV4d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride);
void instV3d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride);
void uninstV3d(int type, float *dst, uint8 *src, uint32 numVertices, uint32 stride);
void instV2d(int type, uint8 *dst, float *src, uint32 numVertices, uint32 stride);
void uninstV2d(int type, float *dst, uint8 *src, uint32 numVertices, uint32 stride);
bool32 instColor(int type, uint8 *dst, uint8 *src, uint32 numVertices, uint32 stride);
void uninstColor(int type, uint8 *dst, uint8 *src, uint32 numVertices, uint32 stride);

}
