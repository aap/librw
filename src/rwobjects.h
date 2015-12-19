namespace rw {

struct Object
{
	uint8 type;
	uint8 subType;
	uint8 flags;
	void *parent;
};

struct Frame : PluginBase<Frame>, Object
{
	typedef Frame *(*Callback)(Frame *f, void *data);
	float32	matrix[16];
	float32	ltm[16];

	Frame *child;
	Frame *next;
	Frame *root;

	// temporary
	int32 matflag;
	bool dirty;

	Frame(void);
	Frame(Frame *f);
	~Frame(void);
	Frame *addChild(Frame *f);
	Frame *removeChild(void);
	Frame *forAllChildren(Callback cb, void *data);
	int32 count(void);
	void updateLTM(void);
	void setDirty(void);
};

Frame **makeFrameList(Frame *frame, Frame **flist);

struct HAnimKeyFrame
{
	HAnimKeyFrame *prev;
	float time;
	float q[4];
	float t[3];
};

struct HAnimNodeInfo
{
	int32 id;
	int32 index;
	int32 flags;
	Frame *frame;
};

struct HAnimHierarchy
{
	int32 flags;
	int32 numNodes;
	float *matrices;
	float *matricesUnaligned;
	HAnimNodeInfo *nodeInfo;
	Frame *parentFrame;
	HAnimHierarchy *parentHierarchy;	// mostly unused

	// temporary
	int32 maxInterpKeyFrameSize;
};

struct HAnimData
{
	int32 id;
	HAnimHierarchy *hierarchy;
};

extern int32 hAnimOffset;
void registerHAnimPlugin(void);

struct Image
{
	int32 flags;
	int32 width, height;
	int32 depth;
	int32 stride;
	uint8 *pixels;
	uint8 *palette;

	Image(int32 width, int32 height, int32 depth);
	~Image(void);
	void allocate(void);
	void free(void);
	void setPixels(uint8 *pixels);
	void setPalette(uint8 *palette);

	static void setSearchPath(const char*);
	static void printSearchPath(void);
	static char *getFilename(const char*);
};
Image *readTGA(const char *filename);
void writeTGA(Image *image, const char *filename);

// used to emulate d3d and xbox textures
struct RasterLevels
{
	int32 numlevels;
	uint32 format;
	struct Level {
		int32 width, height, size;
		uint8 *data;
	} levels[1];	// 0 is illegal :/
};

struct Raster : PluginBase<Raster>
{
	int32 platform;

	int32 type;	// hardly used
	int32 flags;
	int32 format;
	int32 width, height, depth;
	int32 stride;
	uint8 *texels;
	uint8 *palette;

	static int32 nativeOffsets[NUM_PLATFORMS];

	Raster(int32 width, int32 height, int32 depth, int32 format, int32 platform = 0);
	~Raster(void);

	static Raster *createFromImage(Image *image);
	uint8 *lock(int32 level);
	void unlock(int32 level);
	int32 getNumLevels(void);
	static int32 calculateNumLevels(int32 width, int32 height);

	enum Format {
		DEFAULT    = 0,
		C1555      = 0x0100,
		C565       = 0x0200,
		C4444      = 0x0300,
		LUM8       = 0x0400,
		C8888      = 0x0500,
		C888       = 0x0600,
		D16        = 0x0700,
		D24        = 0x0800,
		D32        = 0x0900,
		C555       = 0x0A00,
		AUTOMIPMAP = 0x1000,
		PAL8       = 0x2000,
		PAL4       = 0x4000,
		MIPMAP     = 0x8000
	};
};

#define IGNORERASTERIMP 1

struct NativeRaster
{
	virtual void create(Raster *raster) { assert(IGNORERASTERIMP && "unimplemented"); };
	virtual uint8 *lock(Raster *raster, int32 level) { assert(IGNORERASTERIMP && "unimplemented"); return NULL; };
	virtual void unlock(Raster *raster, int32 level) { assert(IGNORERASTERIMP && "unimplemented"); };
	virtual int32 getNumLevels(Raster *raster) { assert(IGNORERASTERIMP && "unimplemented"); return 0; };
};

// TODO: link into texdict
struct Texture : PluginBase<Texture>
{
	char name[32];
	char mask[32];
	uint32 filterAddressing; // VVVVUUUU FFFFFFFF
	Raster *raster;
	int32 refCount;

	// temporary - pointer to next tex in dictionary
	Texture *next;

	Texture(void);
	~Texture(void);
	void decRef(void);
	static Texture *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
	static Texture *read(const char *name, const char *mask);
	static Texture *streamReadNative(Stream *stream);
	void streamWriteNative(Stream *stream);
	uint32 streamGetSizeNative(void);

	enum FilterMode {
		NEAREST = 1,
		LINEAR,
		MIPNEAREST,
		MIPLINEAR,
		LINEARMIPNEAREST,
		LINEARMIPLINEAR
	};
	enum Addressing {
		WRAP = 1,
		MIRROR,
		CLAMP,
		BORDER
	};
};

struct Material : PluginBase<Material>
{
	Texture *texture;
	uint8 color[4];
	float32 surfaceProps[3];
	Pipeline *pipeline;
	int32 refCount;

	Material(void);
	Material(Material *m);
	void decRef(void);
	~Material(void);
	static Material *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

void registerMaterialRightsPlugin(void);

struct MatFX
{
	enum Flags {
		NOTHING = 0,
		BUMPMAP,
		ENVMAP,
		BUMPENVMAP,
		DUAL,
		UVTRANSFORM,
		DUALUVTRANSFORM
	};
	struct Bump {
		Frame   *frame;
		Texture *bumpedTex;
		Texture *tex;
		float    coefficient;
	};
	struct Env {
		Frame   *frame;
		Texture *tex;
		float    coefficient;
		int32    fbAlpha;
	};
	struct Dual {
		Texture *tex;
		int32    srcBlend;
		int32    dstBlend;
	};
	struct UVtransform {
		float *baseTransform;
		float *dualTransform;
	};
	struct {
		uint32 type;
		union {
			Bump bump;
			Env  env;
			Dual dual;
			UVtransform uvtransform;
		};
	} fx[2];
	uint32 flags;

	void setEffects(uint32 flags);
	int32 getEffectIndex(uint32 type);
};

struct MatFXGlobals
{
	int32 atomicOffset;
	int32 materialOffset;
	ObjPipeline *pipelines[NUM_PLATFORMS];
	bool32 hack;
};
extern MatFXGlobals matFXGlobals;
void registerMatFXPlugin(void);

struct Mesh
{
	uint16 *indices;
	uint32 numIndices;
	Material *material;
};

struct MeshHeader
{
	uint32 flags;
	uint16 numMeshes;
	// RW has uint16 serialNum here
	uint32 totalIndices;
	Mesh *mesh;	// RW has a byte offset here

	void allocateIndices(void);
};

struct MorphTarget
{
	float32 boundingSphere[4];
	float32 *vertices;
	float32 *normals;
};

struct InstanceDataHeader
{
	uint32 platform;
};

struct Geometry : PluginBase<Geometry>, Object
{
	uint32 geoflags;
	int32 numTriangles;
	int32 numVertices;
	int32 numMorphTargets;
	int32 numTexCoordSets;

	uint16 *triangles;
	uint8 *colors;
	float32 *texCoords[8];

	MorphTarget *morphTargets;

	int32 numMaterials;
	Material **materialList;

	MeshHeader *meshHeader;

	InstanceDataHeader *instData;

	int32 refCount;

	Geometry(int32 numVerts, int32 numTris, uint32 flags);
	void decRef(void);
	~Geometry(void);
	static Geometry *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
	void addMorphTargets(int32 n);
	void calculateBoundingSphere(void);
	bool32 hasColoredMaterial(void);
	void allocateData(void);
	void generateTriangles(int8 *adc = NULL);

	enum Flags
	{
		TRISTRIP  = 0x01,
		POSITIONS = 0x02,
		TEXTURED  = 0x04,
		PRELIT    = 0x08,
		NORMALS   = 0x10,
		LIGHT     = 0x20,
		MODULATE  = 0x40,
		TEXTURED2 = 0x80,
		NATIVE         = 0x01000000,
		NATIVEINSTANCE = 0x02000000
	};
};

void registerMeshPlugin(void);
void registerNativeDataPlugin(void);

struct Skin
{
	int32 numBones;
	int32 numUsedBones;
	int32 numWeights;
	uint8 *usedBones;
	float *inverseMatrices;
	uint8 *indices;
	float *weights;
	uint8 *data;	// only used by delete
	void *platformData; // a place to store platform specific stuff

	void init(int32 numBones, int32 numUsedBones, int32 numVertices);
	void findNumWeights(int32 numVertices);
	void findUsedBones(int32 numVertices);
};

struct SkinGlobals
{
	int32 offset;
	ObjPipeline *pipelines[NUM_PLATFORMS];
};
extern SkinGlobals skinGlobals;
void registerSkinPlugin(void);

struct Clump;

struct Light : PluginBase<Light>, Object
{
	Frame *frame;
	float32 radius;
	float32 color[4];
	float32 minusCosAngle;
	Clump *clump;

	Light(void);
	Light(Light *l);
	~Light(void);
	static Light *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

struct Atomic : PluginBase<Atomic>, Object
{
	Frame *frame;
	Geometry *geometry;
	Clump *clump;
	ObjPipeline *pipeline;

	Atomic(void);
	Atomic(Atomic *a);
	~Atomic(void);
	static Atomic *streamReadClump(Stream *stream,
		Frame **frameList, Geometry **geometryList);
	bool streamWriteClump(Stream *stream,
		Frame **frameList, int32 numFrames);
	uint32 streamGetSize(void);
	ObjPipeline *getPipeline(void);

	static void init(void);
};

extern ObjPipeline *defaultPipelines[NUM_PLATFORMS];

void registerAtomicRightsPlugin(void);

struct Clump : PluginBase<Clump>, Object
{
	int32 numAtomics;
	Atomic **atomicList;
	int32 numLights;
	Light **lightList;
	int32 numCameras;
	// cameras not implemented

	Clump(void);
	Clump(Clump *c);
	~Clump(void);
	static Clump *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

	void frameListStreamRead(Stream *stream, Frame ***flp, int32 *nf);
	void frameListStreamWrite(Stream *stream, Frame **flp, int32 nf);
};

struct TexDictionary : PluginBase<TexDictionary>
{
	Texture *first;

	TexDictionary(void);
	void add(Texture *tex);
	Texture *find(const char *name);
	static TexDictionary *streamRead(Stream *stream);
	void streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

struct Animation;

struct AnimInterpolatorInfo
{
	int32 id;
	int32 keyFrameSize;
	int32 customDataSize;
	void (*streamRead)(Stream *stream, Animation *anim);
	void (*streamWrite)(Stream *stream, Animation *anim);
	uint32 (*streamGetSize)(Animation *anim);
};

void registerAnimInterpolatorInfo(AnimInterpolatorInfo *interpInfo);

struct Animation
{
	AnimInterpolatorInfo *interpInfo;
	int32 numFrames;
	int32 flags;
	float duration;
	void *keyframes;
	void *customData;

	Animation(AnimInterpolatorInfo*, int32 numFrames, int32 flags, float duration);
	static Animation *streamRead(Stream *stream);
	static Animation *streamReadLegacy(Stream *stream);
	bool streamWrite(Stream *stream);
	bool streamWriteLegacy(Stream *stream);
	uint32 streamGetSize(void);
};

struct AnimInterpolator
{
	// only a stub right now
	Animation *anim;

	AnimInterpolator(Animation *anim);
};

extern TexDictionary *currentTexDictionary;

struct UVAnimKeyFrame
{
	UVAnimKeyFrame *prev;
	float time;
	float uv[6];
};

struct UVAnimCustomData
{
	char name[32];
	int32 nodeToUVChannel[8];
	// RW has a refcount
};

struct UVAnimDictionary
{
	int32 numAnims;
	Animation **anims;

	static UVAnimDictionary *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
	Animation *find(const char *name);
};

extern UVAnimDictionary *currentUVAnimDictionary;

extern int32 uvAnimOffset;

void registerUVAnimPlugin(void);

}
