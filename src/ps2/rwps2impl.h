// lower level stuff

// this should go elsewhere
//#define PAL
#define SCREEN_WIDTH 640
#ifdef PAL
#define SCREEN_HEIGHT 512
#define VIDEOMODE SCE_GS_PAL
#else
#define SCREEN_HEIGHT 448
#define VIDEOMODE SCE_GS_NTSC
#endif


// RW proper

namespace rw {

#ifdef RW_PS2
typedef uint128_t uint128;
#endif

namespace ps2 {

#ifdef RW_PS2

struct VUdesc
{
	uint32 process, buf1, buf2, buf3;
};


extern int rw2gsPrim[];
extern int primSize[];
extern int primRepeat[];

struct RwStateCache {
	Raster *raster;
	uint32 addressU;
	uint32 addressV;
	uint32 filterMode;

	bool32 vertexAlpha;
	uint32 alphaTestEnable;
	uint32 alphaFunc;
	bool32 textureAlpha;
	bool32 blendEnable;
	uint32 srcblend, destblend;
	uint32 zwrite;
	uint32 ztest;
	uint32 cullmode;
	uint32 fogEnable;
	float32 fogStart;
	float32 fogEnd;
};
extern RwStateCache rwStateCache;

enum {
	vuLight		= 0x3D0,
	vuMatrix	= 0x3F0,
	vuXyzwScale	= 0x3F4,
	vuXyzwOffset	= 0x3F5,
	vuClipConsts	= 0x3F6,

	vuGifTag	= 0x3FA,
	vuColScale	= 0x3FB,
	vuSurfProps	= 0x3FC,
	vuVuSwitch	= 0x3FF
};

struct VuConst {
	QWord mat0, mat1, mat2, mat3;
	QWord xyzwScale_2D;
	QWord xyzwOffset_2D;
	QWord xyzwScale_3D;
	QWord xyzwOffset_3D;
	QWord clipConsts;

	QWord gifTag;
	QWord surfProps;
	QWord vuSwitch;
};
extern VuConst vuConst;


// The reset should be private

void renderPrim_VU(PrimitiveType type, void *verts, int32 numVerts);
void renderIndexedPrim_VU(PrimitiveType type, void *verts, int32 numVerts, void *indices, int32 numIndices);
void vuIm3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags);
void vuIm3DRenderIndexed(PrimitiveType type, void *indices, int32 numIndices);
void vuIm3DEnd(void);
void clearCamera(Camera *cam, RGBA *col, uint32 mode);
void beginUpdate(Camera *cam);
void endUpdate(Camera *cam);
void setRenderState(int32 state, void *pvalue);
void *getRenderState(int32 state);
int deviceSystem(DeviceReq req, void *arg, int32 n);

#endif

Raster *rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level, int32 lockMode);
void rasterUnlock(Raster*, int32 level);
uint8 *rasterLockPalette(Raster*, int32 lockMode);
void rasterUnlockPalette(Raster*);
int32 rasterNumLevels(Raster*);
bool32 imageFindRasterFormat(Image *img, int32 type,
	int32 *width, int32 *height, int32 *depth, int32 *format);
bool32 rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
