namespace rw {

enum RenderState
{
	VERTEXALPHA = 0,
	SRCBLEND,
	DESTBLEND,
	ZTESTENABLE,
	ZWRITEENABLE,
	FOGENABLE,
	FOGCOLOR,
	// TODO:
	// fog type, density ?
	// ? cullmode
	// ? shademode
	// ???? stencil

	// platform specific or opaque?
	ALPHATESTFUNC,
	ALPHATESTREF,
};

enum AlphaTestFunc
{
	ALPHANEVER = 0,
	ALPHALESS,
	ALPHAGREATERTHAN
};

enum BlendFunction
{
	BLENDZERO = 0,
	BLENDONE,
	BLENDSRCCOLOR,
	BLENDINVSRCCOLOR,
	BLENDSRCALPHA,
	BLENDINVSRCALPHA,
	BLENDDESTALPHA,
	BLENDINVDESTALPHA,
	BLENDDESTCOLOR,
	BLENDINVDESTCOLOR,
	BLENDSRCALPHASAT,
	// TODO: add more perhaps
};

// This is for platform independent things and the render device (of which
// there can only ever be one).
// TODO: move more stuff into this
struct Engine
{
	void *currentCamera;
	void *currentWorld;

	// Device
	float32 zNear, zFar;
	void   (*beginUpdate)(Camera*);
	void   (*endUpdate)(Camera*);
	void   (*clearCamera)(Camera*, RGBA *col, uint32 mode);
	void   (*setRenderState)(int32 state, uint32 value);
	uint32 (*getRenderState)(int32 state);

	static void init(void);
};

extern Engine *engine;

// This is for platform driver implementations which have to be available
// regardless of the render device.
struct Driver
{
	ObjPipeline *defaultPipeline;
	int32 rasterNativeOffset;

	void   (*rasterCreate)(Raster*);
	uint8 *(*rasterLock)(Raster*, int32 level);
	void   (*rasterUnlock)(Raster*, int32 level);
	int32  (*rasterNumLevels)(Raster*);
	void   (*rasterFromImage)(Raster*, Image*);
	Image *(*rasterToImage)(Raster*);

	static PluginList s_plglist[NUM_PLATFORMS];
	static void open(void);
	static int32 registerPlugin(int32 platform, int32 size, uint32 id,
	                            Constructor ctor, Destructor dtor){
		return s_plglist[platform].registerPlugin(size, id,
		                                          ctor, dtor, nil);
	}
};

extern Driver *driver[NUM_PLATFORMS];
#define DRIVER driver[rw::platform]

inline void setRenderState(int32 state, uint32 value){
	engine->setRenderState(state, value); }

inline uint32 getRenderState(int32 state){
	return engine->getRenderState(state); }

namespace null {
	void beginUpdate(Camera*);
	void endUpdate(Camera*);
	void clearCamera(Camera*, RGBA *col, uint32 mode);

	void   setRenderState(int32 state, uint32 value);
	uint32 getRenderState(int32 state);

	void   rasterCreate(Raster*);
	uint8 *rasterLock(Raster*, int32 level);
	void   rasterUnlock(Raster*, int32 level);
	int32  rasterNumLevels(Raster*);
	void   rasterFromImage(Raster*, Image*);
	Image *rasterToImage(Raster*);
}

}
