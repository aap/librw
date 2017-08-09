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
	ALPHAALWAYS = 0,
	ALPHAGREATEREQUAL,
	ALPHALESS
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

enum DeviceReq
{
	// Device/Context creation
	DEVICESTART,
	// Device initialization before Engine/Driver plugins are opened
	DEVICEINIT,
	// Device/Context shutdown
	DEVICESTOP,
};

typedef int DeviceSystem(DeviceReq req, void *arg0);

// This is for the render device, we only have one
struct Device
{
	float32 zNear, zFar;
	void   (*beginUpdate)(Camera*);
	void   (*endUpdate)(Camera*);
	void   (*clearCamera)(Camera*, RGBA *col, uint32 mode);
	void   (*showRaster)(Raster *raster);
	void   (*setRenderState)(int32 state, uint32 value);
	uint32 (*getRenderState)(int32 state);
	void   (*im2DRenderIndexedPrimitive)(PrimitiveType,
	                                     void*, int32, void*, int32);
	DeviceSystem *system;
};

// This is for platform-dependent but portable things
// so the engine has one for every platform
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
	static int32 registerPlugin(int32 platform, int32 size, uint32 id,
	                            Constructor ctor, Destructor dtor){
		return s_plglist[platform].registerPlugin(size, id,
		                                          ctor, dtor, nil);
	}
};

struct EngineStartParams;

// This is for platform independent things
// TODO: move more stuff into this
// TODO: make this have plugins and allocate in Engine::open
struct Engine
{
	enum State {
		Dead = 0,
		Initialized,
		Opened,
		Started
	};
	void *currentCamera;
	void *currentWorld;
	Texture *imtexture;

	TexDictionary *currentTexDictionary;
	// load textures from files
	bool32 loadTextures;
	// create dummy textures to store just names
	bool32 makeDummies;
	// Dynamically allocated because of plugins
	Driver *driver[NUM_PLATFORMS];
	Device device;

	static State state;

	static bool32 init(void);
	static bool32 open(void);
	static bool32 start(EngineStartParams*);
	static void term(void);
	static void close(void);
	static void stop(void);
};

extern Engine *engine;

inline void SetRenderState(int32 state, uint32 value){
	engine->device.setRenderState(state, value); }

inline uint32 GetRenderState(int32 state){
	return engine->device.getRenderState(state); }

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

	void   im2DRenderIndexedPrimitive(PrimitiveType,
	                                  void*, int32, void*, int32);

	int deviceSystem(DeviceReq req);

	extern Device renderdevice;
}

}
