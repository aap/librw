#include <stddef.h>

namespace rw {

struct RGBA
{
	uint8 red;
	uint8 green;
	uint8 blue;
	uint8 alpha;
};

struct RGBAf
{
	float32 red;
	float32 green;
	float32 blue;
	float32 alpha;
};

struct V2d
{
	float32 x, y;
	void set(float32 x, float32 y){
		this->x = x; this->y = y; }
};

struct V3d
{
	float32 x, y, z;
	void set(float32 x, float32 y, float32 z){
		this->x = x; this->y = y; this->z = z; }
};

struct LLLink
{
	LLLink *next;
	LLLink *prev;
	void init(void){
		this->next = NULL;
		this->prev = NULL;
	}
	void remove(void){
		this->prev->next = this->next;
		this->next->prev = this->prev;
	}
};

#define LLLinkGetData(linkvar,type,entry)                           \
    ((type*)(((uint8*)(linkvar))-offsetof(type,entry)))

// Have to be careful since the link might be deleted.
#define FORLIST(_link, _list) \
	for(LLLink *_next = NULL, *_link = (_list).link.next; \
	_next = (_link)->next, (_link) != (_list).end(); \
	(_link) = _next)

struct LinkList
{
	LLLink link;
	void init(void){
		this->link.next = &this->link;
		this->link.prev = &this->link;
	}
	bool32 isEmpty(void){
		return this->link.next == &this->link;
	}
	void add(LLLink *link){
		link->next = this->link.next;
		link->prev = &this->link;
		this->link.next->prev = link;
		this->link.next = link;
	}
	void append(LLLink *link){
		link->next = &this->link;
		link->prev = this->link.prev;
		this->link.prev->next = link;
		this->link.prev = link;
	}
	LLLink *end(void){
		return &this->link;
	}
	int32 count(void){
		int32 n = 0;
		FORLIST(lnk, (*this))
			n++;
		return n;
	}
};

struct Object
{
	uint8 type;
	uint8 subType;
	uint8 flags;
	uint8 privateFlags;
	void *parent;

	void init(uint8 type, uint8 subType){
		this->type = type;
		this->subType = subType;
		this->flags = 0;
		this->privateFlags = 0;
		this->parent = NULL;
	}
	void copy(Object *o){
		this->type = o->type;
		this->subType = o->subType;
		this->flags = o->flags;
		this->privateFlags = o->privateFlags;
		this->parent = NULL;
	}
};

struct Frame : PluginBase<Frame>
{
	typedef Frame *(*Callback)(Frame *f, void *data);

	enum { ID = 0 };
	Object object;
	LinkList objectList;
	float32	matrix[16];
	float32	ltm[16];

	Frame *child;
	Frame *next;
	Frame *root;

	// temporary
	int32 matflag;
	bool dirty;

	static Frame *create(void);
	Frame *cloneHierarchy(void);
	void destroy(void);
	void destroyHierarchy(void);
	Frame *addChild(Frame *f, bool32 append = 0);
	Frame *removeChild(void);
	Frame *forAllChildren(Callback cb, void *data);
	Frame *getParent(void){
		return (Frame*)this->object.parent; }
	int32 count(void);
	void updateLTM(void);
	void setDirty(void);

	void setHierarchyRoot(Frame *root);
	Frame *cloneAndLink(Frame *clonedroot);
	void purgeClone(void);

	// private flags:
	// #define rwFRAMEPRIVATEHIERARCHYSYNCLTM  0x01
	// #define rwFRAMEPRIVATEHIERARCHYSYNCOBJ  0x02
	// #define rwFRAMEPRIVATESUBTREESYNCLTM    0x04
	// #define rwFRAMEPRIVATESUBTREESYNCOBJ    0x08
	// #define rwFRAMEPRIVATESTATIC            0x10
};

Frame **makeFrameList(Frame *frame, Frame **flist);

struct ObjectWithFrame : Object
{
	LLLink inFrame;
	void setFrame(Frame *f){
		if(this->parent)
			this->inFrame.remove();
		this->parent = f;
		if(f)
			f->objectList.add(&this->inFrame);
	}
	static ObjectWithFrame *fromFrame(LLLink *lnk){
		return LLLinkGetData(lnk, ObjectWithFrame, inFrame);
	}
};

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

	static HAnimHierarchy *create(int32 numNodes, int32 *nodeFlags, int32 *nodeIDs, int32 flags, int32 maxKeySize);
	void destroy(void);
	void attachByIndex(int32 id);
	void attach(void);
	int32 getIndex(int32 id);

	static HAnimHierarchy *get(Frame *f);
	static HAnimHierarchy *find(Frame *f);

	enum NodeFlag {
		POP = 1,
		PUSH
	};
};

struct HAnimData
{
	int32 id;
	HAnimHierarchy *hierarchy;

	static HAnimData *get(Frame *f);
};

extern int32 hAnimOffset;
extern bool32 hAnimDoStream;
void registerHAnimPlugin(void);

struct Image
{
	int32 flags;
	int32 width, height;
	int32 depth;
	int32 stride;
	uint8 *pixels;
	uint8 *palette;

	static Image *create(int32 width, int32 height, int32 depth);
	void destroy(void);
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

	static Raster *create(int32 width, int32 height, int32 depth, int32 format, int32 platform = 0);
	void destroy(void);
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
	virtual void create(Raster*)
		{ assert(IGNORERASTERIMP && "unimplemented"); };
	virtual uint8 *lock(Raster*, int32)
		{ assert(IGNORERASTERIMP && "unimplemented"); return NULL; };
	virtual void unlock(Raster*, int32)
		{ assert(IGNORERASTERIMP && "unimplemented"); };
	virtual int32 getNumLevels(Raster*)
		{ assert(IGNORERASTERIMP && "unimplemented"); return 0; };
};

struct TexDictionary;

struct Texture : PluginBase<Texture>
{
	Raster *raster;
	TexDictionary *dict;
	LLLink inDict;
	char name[32];
	char mask[32];
	uint32 filterAddressing; // VVVVUUUU FFFFFFFF
	int32 refCount;

	static Texture *create(Raster *raster);
	void destroy(void);
	static Texture *fromDict(LLLink *lnk){
		return LLLinkGetData(lnk, Texture, inDict); }
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

struct SurfaceProperties
{
	float32 ambient;
	float32 specular;
	float32 diffuse;
};

struct Material : PluginBase<Material>
{
	Texture *texture;
	RGBA color;
	SurfaceProperties surfaceProps;
	Pipeline *pipeline;
	int32 refCount;

	static Material *create(void);
	Material *clone(void);
	void destroy(void);
	static Material *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

void registerMaterialRightsPlugin(void);

struct MatFX
{
	enum {
		NOTHING = 0,
		BUMPMAP,
		ENVMAP,
		BUMPENVMAP,	// BUMP | ENV
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
	uint32 type;

	void setEffects(uint32 flags);
	int32 getEffectIndex(uint32 type);
	void setBumpTexture(Texture *t);
	void setBumpCoefficient(float32 coef);
	void setEnvTexture(Texture *t);
	void setEnvCoefficient(float32 coef);
	void setDualTexture(Texture *t);
	void setDualSrcBlend(int32 blend);
	void setDualDestBlend(int32 blend);

	static void enableEffects(Atomic *atomic);
};

struct MatFXGlobals
{
	int32 atomicOffset;
	int32 materialOffset;
	ObjPipeline *pipelines[NUM_PLATFORMS];
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
	enum {
		TRISTRIP = 1
	};
	uint32 flags;
	uint16 numMeshes;
	// RW has uint16 serialNum here
	uint32 totalIndices;
	Mesh *mesh;	// RW has a byte offset here

	void allocateIndices(void);
	~MeshHeader(void);
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

struct Triangle
{
	uint16 v[3];
	uint16 matId;
};

struct Geometry : PluginBase<Geometry>
{
	enum { ID = 8 };
	Object object;
	uint32 geoflags;	// TODO: rename
	int32 numTriangles;
	int32 numVertices;
	int32 numMorphTargets;
	int32 numTexCoordSets;

	Triangle *triangles;
	uint8 *colors;
	float32 *texCoords[8];

	MorphTarget *morphTargets;

	// TODO: struct
	int32 numMaterials;
	Material **materialList;

	MeshHeader *meshHeader;

	InstanceDataHeader *instData;

	int32 refCount;

	static Geometry *create(int32 numVerts, int32 numTris, uint32 flags);
	void destroy(void);
	static Geometry *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
	void addMorphTargets(int32 n);
	void calculateBoundingSphere(void);
	bool32 hasColoredMaterial(void);
	void allocateData(void);
	void generateTriangles(int8 *adc = NULL);
	void buildMeshes(void);

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
	static void setPipeline(Atomic *a, int32 type);
};

struct SkinGlobals
{
	int32 offset;
	ObjPipeline *pipelines[NUM_PLATFORMS];
};
extern SkinGlobals skinGlobals;
void registerSkinPlugin(void);

struct Clump;

struct Atomic : PluginBase<Atomic>
{
	enum { ID = 1 };
	ObjectWithFrame object;
	Geometry *geometry;
	Clump *clump;
	LLLink inClump;
	ObjPipeline *pipeline;

	static Atomic *create(void);
	Atomic *clone(void);
	void destroy(void);
	void setFrame(Frame *f) { this->object.setFrame(f); }
	Frame *getFrame(void) { return (Frame*)this->object.parent; }
	static Atomic *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Atomic, inClump); }
	static Atomic *streamReadClump(Stream *stream,
	Frame **frameList, Geometry **geometryList);
	bool streamWriteClump(Stream *stream,
	Frame **frameList, int32 numframes);
	uint32 streamGetSize(void);
	ObjPipeline *getPipeline(void);

	static void init(void);
};

extern ObjPipeline *defaultPipelines[NUM_PLATFORMS];

void registerAtomicRightsPlugin(void);

struct Light : PluginBase<Light>
{
	enum { ID = 3 };
	ObjectWithFrame object;
	float32 radius;
	RGBAf color;
	float32 minusCosAngle;

	// clump link handled by plugin in RW
	Clump *clump;
	LLLink inClump;

	static Light *create(int32 type);
	void destroy(void);
	void setFrame(Frame *f) { this->object.setFrame(f); }
	Frame *getFrame(void){ return (Frame*)this->object.parent; }
	static Light *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Light, inClump); }
	void setAngle(float32 angle);
	float32 getAngle(void);
	void setColor(float32 r, float32 g, float32 b);
	int32 getType(void){ return this->object.subType; }
	static Light *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

	enum Type {
		DIRECTIONAL = 1,
		AMBIENT,
		POINT = 0x80,	// positioned
		SPOT,
		SOFTSPOT,
	};
	enum Flags {
		LIGHTATOMICS = 1,
		LIGHTWORLD = 2
	};
};

struct Camera : PluginBase<Camera>
{
	enum { ID = 4 };
	ObjectWithFrame object;
	V2d viewWindow;
	V2d viewOffset;
	float32 nearPlane, farPlane;
	float32 fogPlane;
	int32 projection;

	Clump *clump;
	LLLink inClump;

	static Camera *create(void);
	Camera *clone(void);
	void destroy(void);
	void setFrame(Frame *f) { this->object.setFrame(f); }
	Frame *getFrame(void){ return (Frame*)this->object.parent; }
	static Camera *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Camera, inClump); }
	static Camera *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

struct Clump : PluginBase<Clump>
{
	enum { ID = 2 };
	Object object;
	LinkList atomics;
	LinkList lights;
	LinkList cameras;

	static Clump *create(void);
	Clump *clone(void);
	void destroy(void);
	int32 countAtomics(void) { return this->atomics.count(); }
	void addAtomic(Atomic *a){
		a->clump = this;
		this->atomics.append(&a->inClump);
	}
	int32 countLights(void) { return this->lights.count(); }
	void addLight(Light *l){
		l->clump = this;
		this->lights.append(&l->inClump);
	}
	int32 countCameras(void) { return this->cameras.count(); }
	void addCamera(Camera *c){
		c->clump = this;
		this->cameras.append(&c->inClump);
	}
	void setFrame(Frame *f){
		this->object.parent = f; }
	Frame *getFrame(void){
		return (Frame*)this->object.parent; }
	static Clump *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

	void frameListStreamRead(Stream *stream, Frame ***flp, int32 *nf);
	void frameListStreamWrite(Stream *stream, Frame **flp, int32 nf);
};

struct TexDictionary : PluginBase<TexDictionary>
{
	enum { ID = 6 };
	Object object;
	LinkList textures;

	static TexDictionary *create(void);
	void destroy(void);
	int32 count(void) { return this->textures.count(); }
	void add(Texture *t){
		t->dict = this;
		this->textures.append(&t->inDict);
	}
	Texture *find(const char *name);
	static TexDictionary *streamRead(Stream *stream);
	void streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

extern TexDictionary *currentTexDictionary;

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
AnimInterpolatorInfo *findAnimInterpolatorInfo(int32 id);

struct Animation
{
	AnimInterpolatorInfo *interpInfo;
	int32 numFrames;
	int32 flags;
	float duration;
	void *keyframes;
	void *customData;

	static Animation *create(AnimInterpolatorInfo*, int32 numFrames, int32 flags, float duration);
	void destroy(void);
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

struct UVAnimKeyFrame
{
	UVAnimKeyFrame *prev;
	float time;
	float uv[6];
};

struct UVAnimDictionary;

// RW does it differently...maybe we should implement RtDict
// and make it more general?

struct UVAnimCustomData
{
	char name[32];
	int32 nodeToUVChannel[8];
	int32 refCount;

	void destroy(Animation *anim);
};

// This should be more general probably
struct UVAnimDictEntry
{
	Animation *anim;
	LLLink inDict;
	static UVAnimDictEntry *fromDict(LLLink *lnk){
		return LLLinkGetData(lnk, UVAnimDictEntry, inDict); }
};

// This too
struct UVAnimDictionary
{
	LinkList animations;

	static UVAnimDictionary *create(void);
	void destroy(void);
	int32 count(void) { return this->animations.count(); }
	void add(Animation *anim);
	Animation *find(const char *name);

	static UVAnimDictionary *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

extern UVAnimDictionary *currentUVAnimDictionary;

struct UVAnim
{
	AnimInterpolator *interp[8];
};

extern int32 uvAnimOffset;

void registerUVAnimPlugin(void);

}
