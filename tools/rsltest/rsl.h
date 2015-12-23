enum {
	TEX_IDENT  = 0x00746578,
	MDL_IDENT  = 0x006D646C,
	WRLD_IDENT = 0x57524C44
};

struct RslStream {
	uint32   ident;
	bool32   isMulti;
	uint32   fileSize;
	uint32   dataSize;
	uint8 ***reloc;
	uint32   relocSize;
	uint8  **hashTab;   // ??
	uint16   hashTabSize;
	uint16   numAtomics;

	uint8 *data;
	void relocate(void);
};

struct RslObject;
struct RslObjectHasFrame;
struct RslClump;
struct RslAtomic;
struct RslFrame;
struct RslNativeGeometry;
struct RslNativeMesh;
struct RslGeometry;
struct RslSkin;
struct RslMaterial;
struct RslHAnimHierarchy;
struct RslHAnimNode;
struct RslPS2ResEntryHeader;
struct RslPS2InstanceData;

typedef RslFrame *(*RslFrameCallBack)(RslFrame *frame, void *data);
typedef RslClump *(*RslClumpCallBack)(RslClump *clump, void *data);
typedef RslAtomic *(*RslAtomicCallBack)(RslAtomic *atomic, void *data);
typedef RslMaterial *(*RslMaterialCallBack)(RslMaterial *material, void *data);

struct RslLLLink {
	RslLLLink *next;
	RslLLLink *prev;
};

struct RslLinkList {
	RslLLLink link;
};

#define rslLLLinkGetData(linkvar,type,entry)                             \
    ((type *)(((uint8 *)(linkvar))-offsetof(type,entry)))

#define rslLLLinkGetNext(linkvar)                                        \
    ((linkvar)->next)

#define rslLLLinkGetPrevious(linkvar)                                    \
    ((linkvar)->prev)

#define rslLLLinkInitialize(linkvar)                                     \
    ( (linkvar)->prev = (RslLLLink *)NULL,                               \
      (linkvar)->next = (RslLLLink *)NULL )

#define rslLLLinkAttached(linkvar)                                       \
    ((linkvar)->next)

struct RslObject {
        uint8  type;
        uint8  subType;
        uint8  flags;
        uint8  privateFlags;
        void  *parent;
};

struct RslObjectHasFrame {
	RslObject   object;
	RslLLLink   lFrame;
	void      (*sync)();
};

#define rslObjectGetParent(object)           (((const RslObject *)(object))->parent)

// from Serge
//void TEX::getInfo(TEXInfo a)
//{
// bpp = (a.flags & 0xF000) >> 12;
// swizzle = (a.flags & 0xF000000) >> 24;
// Width = (int)pow(2.0, (int)(a.flags & 0xF));
// Height = (int)pow(16.0, (int)((a.flags & 0xF00) >> 8))*(int)pow(2.0, (int)(((a.flags & 0xF0) >> 4) / 4));
// mipmaps = (a.flags & 0xFF0000) >> 20;
//}

struct RslRasterPS2 {
	uint8 *data;
	// XXXXSSSSMMMMMMMMBBBBHHHHHHHHWWWW
	uint32 flags;
};

struct RslRasterPSP {
	uint32 unk1;
	uint8 *data;
	uint32 flags1, flags2;
};

union RslRaster {
	RslRasterPS2 ps2;
	RslRasterPSP psp;
};

struct RslTexDictionary {
	RslObject   object;
	RslLinkList texturesInDict;
	RslLLLink   lInInstance;
};

struct RslTexture {
	RslRaster        *raster;
	RslTexDictionary *dict;
	RslLLLink         lInDictionary;
	char              name[32];
	char              mask[32];
};

struct RslFrame {
	RslObject          object;
	RslLLLink          inDirtyListLink; // ?
		         
	float32            modelling[16];
	float32            ltm[16];
	RslFrame          *child;
	RslFrame          *next;
	RslFrame          *root;

	// RwHAnimFrameExtension
	int32              nodeId;
	RslHAnimHierarchy *hier;
	// R* Node name
	char              *name;
	// R* Visibility
	int32              hierId;
};

struct RslClump {
	RslObject   object;
	RslLinkList atomicList;
};

#define RslClumpGetFrame(_clump)                                    \
    ((RslFrame *) rslObjectGetParent(_clump))

struct RslAtomic {
	RslObjectHasFrame  object;
	RslGeometry       *geometry;
	RslClump          *clump;
	RslLLLink          inClumpLink;

	// what's this? rpWorldObj?
	uint32             unk1;
	uint16             unk2;
	uint16             unk3;
	// RpSkin
	RslHAnimHierarchy *hier;
	// what about visibility? matfx?
	int32              pad;	// 0xAAAAAAAA
};

#define RslAtomicGetFrame(_atomic)                                  \
    ((RslFrame *) rslObjectGetParent(_atomic))

struct RslMaterialList {
	RslMaterial **materials;
	int32         numMaterials;
	int32         space;
};

struct RslGeometry {
	RslObject       object;
	int16           refCount;
	int16           pad1;

	RslMaterialList matList;

	RslSkin        *skin;
	uint32          pad2;            // 0xAAAAAAAA
};

struct RslMatFXEnv {
	RslFrame *frame;
	char     *texname;
	float32   intensity;
};

struct RslMatFX {
	union {
		RslMatFXEnv env;
	};
	int32     effectType;
};

struct RslMaterial {
	char       *texname;
	uint32      color;
	uint32      refCount;
	RslMatFX   *matfx;
};

struct RslHAnimNodeInfo {
	int8      id;
	int8      index;
	int8      flags;
	RslFrame *frame;
};

struct RslHAnimHierarchy {
	int32              flags;
	int32              numNodes;
	void              *pCurrentAnim;
	float32            currentTime;
	void              *pNextFrame;
	void             (*pAnimCallBack)();
	void              *pAnimCallBackData;
	float32            animCallBackTime;
	void             (*pAnimLoopCallBack)();
	void              *pAnimLoopCallBackData;
	float32           *pMatrixArray;
	void              *pMatrixArrayUnaligned;
	RslHAnimNodeInfo  *pNodeInfo;
	RslFrame          *parentFrame;
	int32              maxKeyFrameSize;
	int32              currentKeyFrameSize;
	void             (*keyFrameToMatrixCB)();
	void             (*keyFrameBlendCB)();
	void             (*keyFrameInterpolateCB)();
	void             (*keyFrameAddCB)();
	RslHAnimHierarchy *parentHierarchy;
	int32              offsetInParent;
	int32              rootParentOffset;
};

struct RslSkin {
	uint32   numBones;
	uint32   numUsedBones;	// == numBones
	uint8   *usedBones;	// NULL
	float32 *invMatrices;
	int32    numWeights;	// 0
	uint8   *indices;	// NULL
	float32 *weights;	// NULL
	uint32   unk1;          // 0
	uint32   unk2;          // 0
	uint32   unk3;          // 0
	uint32   unk4;          // 0
	uint32   unk5;          // 0
	void    *data;          // NULL
};

struct RslPS2ResEntryHeader {
	float32 bound[4];
	uint32  size;		// and numMeshes
	int32   flags;
	uint32  unk1;
	uint32  unk2;
	uint32  unk3;
	uint32  unk4;
	float32 scale[3];
	float32 pos[3];
};

struct RslPS2InstanceData {
	float32  bound[4];      // ?
	float32  uvScale[2];
	int32    unk1;
	uint32   dmaPacket;
	uint16   numTriangles;
	int16    matID;
	int16    min[3];          // bounding box
	int16    max[3];
};

struct RslStreamHeader {
	uint32   ident;
	uint32   unk;
	uint32   fileEnd;      //
	uint32   dataEnd;      // relative to beginning of header
	uint32   reloc;	       //
	uint32   relocSize;
	uint32   root;        // absolute
	uint16   zero;
	uint16   numAtomics;
};

struct RslWorldGeometry {
	uint16 numMeshes;
	uint16 size;
	// array of numMeshes RslWorldMesh
	// dma data
};

struct RslWorldMesh {
	uint16 texID;           // index into resource table
	uint16 dmaSize;
	uint16 uvScale[2];	// half precision float
	uint16 unk1;
	int16  min[3];          // bounding box
	int16  max[3];
};

struct Resource {
	union {
		RslRaster        *raster;
		RslWorldGeometry *geometry;
		uint8            *raw;
	};
	uint32 *dma;
};

struct OverlayResource {
	int32 id;
	union {
		RslRaster        *raster;
		RslWorldGeometry *geometry;
		uint8            *raw;
	};
};

struct Placement {
	uint16 id;
	uint16 resId;
	int16 bound[4];
	int32 pad;
	float matrix[16];
};

struct Sector {
	OverlayResource *resources;
	uint16           numResources;
	uint16           unk1;
	Placement       *sectionA;
	Placement       *sectionB;
	Placement       *sectionC;
	Placement       *sectionD;
	Placement       *sectionE;
	Placement       *sectionF;
	Placement       *sectionG;
	Placement       *sectionEnd;
	uint16           unk2;
	uint16           unk3;
	uint32           unk4;
};

struct SectorEntry {
	RslStreamHeader *sector;
	uint32           num;
};

struct World {
	Resource    *resources;
	SectorEntry  sectors[47];
	uint32       numResources;
	uint8        pad[0x180];

	uint32 numX;
	void *tabX;	// size 0x4

	uint32 numY;
	void *tabY;	// size 0x30

	uint32 numZ;
	void *tabZ;	// size 0x6

	uint32           numTextures;
	RslStreamHeader *textures; // stream headers
};