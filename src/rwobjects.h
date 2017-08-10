#include <stddef.h>

namespace rw {

struct LLLink
{
	LLLink *next;
	LLLink *prev;
	void init(void){
		this->next = nil;
		this->prev = nil;
	}
	void remove(void){
		this->prev->next = this->next;
		this->next->prev = this->prev;
	}
};

#define LLLinkGetData(linkvar,type,entry) \
    ((type*)(((uint8*)(linkvar))-offsetof(type,entry)))

// Have to be careful since the link might be deleted.
#define FORLIST(_link, _list) \
	for(rw::LLLink *_next = nil, *_link = (_list).link.next; \
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
		this->parent = nil;
	}
	void copy(Object *o){
		this->type = o->type;
		this->subType = o->subType;
		this->flags = o->flags;
		this->privateFlags = o->privateFlags;
		this->parent = nil;
	}
};

struct Frame
{
	PLUGINBASE
	typedef Frame *(*Callback)(Frame *f, void *data);
	enum { ID = 0 };
	enum {		// private flags
		// The hierarchy has unsynched frames
		HIERARCHYSYNCLTM = 0x01,	// LTM not synched
		HIERARCHYSYNCOBJ = 0x02,	// attached objects not synched
		HIERARCHYSYNC    = HIERARCHYSYNCLTM  | HIERARCHYSYNCOBJ,
		// This frame is not synched
		SUBTREESYNCLTM   = 0x04,
		SUBTREESYNCOBJ   = 0x08,
		SUBTREESYNC      = SUBTREESYNCLTM | SUBTREESYNCOBJ,
		SYNCLTM          = HIERARCHYSYNCLTM | SUBTREESYNCLTM,
		SYNCOBJ          = HIERARCHYSYNCOBJ | SUBTREESYNCOBJ,
		// STATIC = 0x10
	};

	Object object;
	LLLink inDirtyList;
	LinkList objectList;
	Matrix matrix;
	Matrix ltm;

	Frame *child;
	Frame *next;
	Frame *root;

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
	bool32 dirty(void) {
		return !!(this->root->object.privateFlags & HIERARCHYSYNC); }
	Matrix *getLTM(void);
	void rotate(V3d *axis, float32 angle, CombineOp op);
	void translate(V3d *trans, CombineOp op);
	void scale(V3d *scale, CombineOp op);
	void transform(Matrix *mat, CombineOp op);
	void updateObjects(void);


	void syncHierarchyLTM(void);
	void setHierarchyRoot(Frame *root);
	Frame *cloneAndLink(Frame *clonedroot);
	void purgeClone(void);


	static LinkList dirtyList;
	static void syncDirty(void);
};

struct FrameList_
{
	int32 numFrames;
	Frame **frames;

	FrameList_ *streamRead(Stream *stream);
	void streamWrite(Stream *stream);
	static uint32 streamGetSize(Frame *f);
};
Frame **makeFrameList(Frame *frame, Frame **flist);

struct ObjectWithFrame
{
	typedef void (*Sync)(ObjectWithFrame*);

	Object object;
	LLLink inFrame;
	Sync syncCB;

	void setFrame(Frame *f){
		if(this->object.parent)
			this->inFrame.remove();
		this->object.parent = f;
		if(f){
			f->objectList.add(&this->inFrame);
			f->updateObjects();
		}
	}
	void sync(void){ this->syncCB(this); }
	static ObjectWithFrame *fromFrame(LLLink *lnk){
		return LLLinkGetData(lnk, ObjectWithFrame, inFrame);
	}
};

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
	void setPixelsDXT(int32 type, uint8 *pixels);
	void setPalette(uint8 *palette);
	bool32 hasAlpha(void);
	void unindex(void);
	void removeMask(void);
	Image *extractMask(void);

	static void setSearchPath(const char*);
	static void printSearchPath(void);
	static char *getFilename(const char*);
};

Image *readTGA(const char *filename);
void writeTGA(Image *image, const char *filename);
void writeBMP(Image *image, const char *filename);

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

struct Raster
{
	PLUGINBASE
	int32 platform;

	int32 type;	// hardly used
	int32 flags;
	int32 format;
	int32 width, height, depth;
	int32 stride;
	uint8 *texels;
	uint8 *palette;

	static Raster *create(int32 width, int32 height, int32 depth,
	                      int32 format, int32 platform = 0);
	void destroy(void);
	static Raster *createFromImage(Image *image, int32 platform = 0);
	Image *toImage(void);
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
	enum Type {
		NORMAL        = 0x00,
		ZBUFFER       = 0x01,
		CAMERA        = 0x02,
		TEXTURE       = 0x04,
		CAMERATEXTURE = 0x05,
		DONTALLOCATE  = 0x80,
	};
};

#define IGNORERASTERIMP 0

struct TexDictionary;

struct Texture
{
	PLUGINBASE
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

	static Texture *(*findCB)(const char *name);
	static Texture *(*readCB)(const char *name, const char *mask);

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

struct Material
{
	PLUGINBASE
	Texture *texture;
	RGBA color;
	SurfaceProperties surfaceProps;
	Pipeline *pipeline;
	int32 refCount;

	static Material *create(void);
	Material *clone(void);
	void destroy(void);
	void setTexture(Texture *tex);
	static Material *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

void registerMaterialRightsPlugin(void);

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
	Sphere boundingSphere;
	V3d *vertices;
	V3d *normals;
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

struct MaterialList
{
	Material **materials;
	int32 numMaterials;
	int32 space;

	void init(void);
	void deinit(void);
	int32 appendMaterial(Material *mat);
	int32 findIndex(Material *mat);
	static MaterialList *streamRead(Stream *stream, MaterialList *matlist);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);
};

struct Geometry
{
	PLUGINBASE
	enum { ID = 8 };
	Object object;
	uint32 flags;
	int32 numTriangles;
	int32 numVertices;
	int32 numMorphTargets;
	int32 numTexCoordSets;

	Triangle *triangles;
	RGBA *colors;
	TexCoords *texCoords[8];

	MorphTarget *morphTargets;
	MaterialList matList;

	MeshHeader *meshHeader;
	InstanceDataHeader *instData;

	int32 refCount;

	static Geometry *create(int32 numVerts, int32 numTris, uint32 flags);
	void destroy(void);
	void addMorphTargets(int32 n);
	void calculateBoundingSphere(void);
	bool32 hasColoredMaterial(void);
	void allocateData(void);
	void generateTriangles(int8 *adc = nil);
	void buildMeshes(void);
	void buildTristrips(void);
	void correctTristripWinding(void);
	void removeUnusedMaterials(void);
	static Geometry *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

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
		// Just for documentation: RW sets this flag
		// to prevent rendering when executing a pipeline,
		// so only instancing will occur.
		// librw's pipelines are different so it's unused here.
		NATIVEINSTANCE = 0x02000000
	};
};

void registerMeshPlugin(void);
void registerNativeDataPlugin(void);

struct Clump;
struct World;

struct Atomic
{
	PLUGINBASE
	typedef void (*RenderCB)(Atomic *atomic);
	enum { ID = 1 };
	enum {
	// flags
		COLLISIONTEST = 0x01,	// unused here
		RENDER = 0x04,
	// private flags
		WORLDBOUNDDIRTY = 0x01,
	// for setGeometry
		SAMEBOUNDINGSPHERE = 0x01,
	};

	ObjectWithFrame object;
	Geometry *geometry;
	Sphere boundingSphere;
	Sphere worldBoundingSphere;
	Clump *clump;
	LLLink inClump;
	ObjPipeline *pipeline;
	RenderCB renderCB;

	World *world;
	ObjectWithFrame::Sync originalSync;

	static Atomic *create(void);
	Atomic *clone(void);
	void destroy(void);
	void setFrame(Frame *f) {
		this->object.setFrame(f);
		this->object.object.privateFlags |= WORLDBOUNDDIRTY;
	}
	Frame *getFrame(void) { return (Frame*)this->object.object.parent; }
	static Atomic *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Atomic, inClump); }
	void removeFromClump(void);
	void setGeometry(Geometry *geo, uint32 flags);
	Sphere *getWorldBoundingSphere(void);
	ObjPipeline *getPipeline(void);
	void render(void) { this->renderCB(this); }
	void setRenderCB(RenderCB renderCB){
		this->renderCB = renderCB;
		if(this->renderCB == nil)
			this->renderCB = defaultRenderCB;
	};
	static Atomic *streamReadClump(Stream *stream,
		FrameList_ *frameList, Geometry **geometryList);
	bool streamWriteClump(Stream *stream, FrameList_ *frmlst);
	uint32 streamGetSize(void);

	static void defaultRenderCB(Atomic *atomic);
};

void registerAtomicRightsPlugin(void);

struct Light
{
	PLUGINBASE
	enum { ID = 3 };
	ObjectWithFrame object;
	float32 radius;
	RGBAf color;
	float32 minusCosAngle;
	LLLink inWorld;

	// clump extension
	Clump *clump;
	LLLink inClump;

	// world extension
	World *world;
	ObjectWithFrame::Sync originalSync;

	static Light *create(int32 type);
	void destroy(void);
	void setFrame(Frame *f) { this->object.setFrame(f); }
	Frame *getFrame(void){ return (Frame*)this->object.object.parent; }
	static Light *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Light, inClump); }
	static Light *fromWorld(LLLink *lnk){
		return LLLinkGetData(lnk, Light, inWorld); }
	void setAngle(float32 angle);
	float32 getAngle(void);
	void setColor(float32 r, float32 g, float32 b);
	int32 getType(void){ return this->object.object.subType; }
	void setFlags(uint32 flags) { this->object.object.flags = flags; }
	uint32 getFlags(void) { return this->object.object.flags; }
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

struct FrustumPlane
{
	Plane plane;
	/* Used for BBox tests:
	 * 0 = inf is closer to normal direction
	 * 1 = sup is closer to normal direction */
	uint8 closestX;
	uint8 closestY;
	uint8 closestZ;
};

struct Camera
{
	PLUGINBASE
	enum { ID = 4 };
	enum { PERSPECTIVE = 1, PARALLEL };
	enum { CLEARIMAGE = 0x1, CLEARZ = 0x2};
	// return value of frustumTestSphere
	enum { SPHEREOUTSIDE, SPHEREBOUNDARY, SPHEREINSIDE };

	ObjectWithFrame object;
	void (*beginUpdateCB)(Camera*);
	void (*endUpdateCB)(Camera*);
	V2d viewWindow;
	V2d viewOffset;
	float32 nearPlane, farPlane;
	float32 fogPlane;
	int32 projection;

	Matrix viewMatrix;
	float32 zScale, zShift;

	FrustumPlane frustumPlanes[6];
	V3d frustumCorners[8];
	BBox frustumBoundBox;

	// clump link handled by plugin in RW
	Clump *clump;
	LLLink inClump;

	// world extension
	/* 3 unknowns */
	World *world;
	ObjectWithFrame::Sync originalSync;
	void (*originalBeginUpdate)(Camera*);
	void (*originalEndUpdate)(Camera*);

	static Camera *create(void);
	Camera *clone(void);
	void destroy(void);
	void setFrame(Frame *f) { this->object.setFrame(f); }
	Frame *getFrame(void){ return (Frame*)this->object.object.parent; }
	static Camera *fromClump(LLLink *lnk){
		return LLLinkGetData(lnk, Camera, inClump); }
	void beginUpdate(void) { this->beginUpdateCB(this); }
	void endUpdate(void) { this->endUpdateCB(this); }
	void clear(RGBA *col, uint32 mode);
	void showRaster(void);
	void setNearPlane(float32);
	void setFarPlane(float32);
	int32 frustumTestSphere(Sphere *s);
	static Camera *streamRead(Stream *stream);
	bool streamWrite(Stream *stream);
	uint32 streamGetSize(void);

	// fov in degrees
	void setFOV(float32 fov, float32 ratio);
};

struct Clump
{
	PLUGINBASE
	enum { ID = 2 };
	Object object;
	LinkList atomics;
	LinkList lights;
	LinkList cameras;

	World *world;

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
	void render(void);
};

// A bit of a stub right now
struct World
{
	PLUGINBASE
	enum { ID = 7 };
	Object object;
	LinkList lights;                // these have positions (type >= 0x80)
	LinkList directionalLights;     // these do not (type < 0x80)

	static World *create(void);
	void addLight(Light *light);
	void addCamera(Camera *cam);
};

struct TexDictionary
{
	PLUGINBASE
	enum { ID = 6 };
	Object object;
	LinkList textures;

	static TexDictionary *create(void);
	void destroy(void);
	int32 count(void) { return this->textures.count(); }
	void add(Texture *t);
	Texture *find(const char *name);
	static TexDictionary *streamRead(Stream *stream);
	void streamWrite(Stream *stream);
	uint32 streamGetSize(void);

	static void setCurrent(TexDictionary *txd);
	static TexDictionary *getCurrent(void);
};

}
