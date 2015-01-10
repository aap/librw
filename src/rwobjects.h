namespace Rw {

// TODO: mostly 
struct Pipeline
{
	uint32 pluginID;
	uint32 pluginData;
};

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

	Frame(void);
	Frame(Frame *f);
	~Frame(void);
	Frame *addChild(Frame *f);
	Frame *removeChild(void);
	Frame *forAllChildren(Callback cb, void *data);
	int32 count(void);
};

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
};
Image *readTGA(const char *filename);
void writeTGA(Image *image, const char *filename);

// TODO: raster, link into texdict
struct Texture : PluginBase<Texture>
{
	char name[32];
	char mask[32];
	uint32 filterAddressing;
	int32 refCount;

	Texture(void);
	~Texture(void);
	void decRef(void);
	static Texture *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

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

void RegisterMaterialRightsPlugin(void);

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

struct MatFXGlobals_
{
	int32 atomicOffset;
	int32 materialOffset;
	Pipeline *pipeline;
};
extern MatFXGlobals_ MatFXGlobals;
void RegisterMatFXPlugin(void);

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

void RegisterMeshPlugin(void);
void RegisterNativeDataPlugin(void);

struct Skin
{
	int32 numBones;
	int32 numUsedBones;
	int32 maxIndex;
	uint8 *usedBones;
	float *inverseMatrices;
	uint8 *indices;
	float *weights;
	uint8 *data;	// only used by delete
};

struct SkinGlobals_
{
	int32 offset;
	Pipeline *pipeline;
};
extern SkinGlobals_ SkinGlobals;
void RegisterSkinPlugin(void);

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
	Pipeline *pipeline;

	Atomic(void);
	Atomic(Atomic *a);
	~Atomic(void);
	static Atomic *streamReadClump(Stream *stream,
		Frame **frameList, Geometry **geometryList);
	bool streamWriteClump(Stream *stream,
		Frame **frameList, int32 numFrames);
	uint32 streamGetSize(void);
};

void RegisterAtomicRightsPlugin(void);

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

private:
	void frameListStreamRead(Stream *stream, Frame ***flp, int32 *nf);
	void frameListStreamWrite(Stream *stream, Frame **flp, int32 nf);
};

}
