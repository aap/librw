namespace rw {

// This is for platform independent things
// TODO: move more stuff into this
struct Engine
{
	void *currentCamera;
	void *currentWorld;

	static void init(void);
};

extern Engine *engine;

// This is for platform driver implementations
struct Driver
{
	ObjPipeline *defaultPipeline;
	int32 rasterNativeOffset;

	void (*beginUpdate)(Camera*);
	void (*endUpdate)(Camera*);

	void   (*rasterCreate)(Raster*);
	uint8 *(*rasterLock)(Raster*, int32 level);
	void   (*rasterUnlock)(Raster*, int32 level);
	int32  (*rasterNumLevels)(Raster*);
	void   (*rasterFromImage)(Raster*, Image*);

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

namespace null {
	void beginUpdate(Camera*);
	void endUpdate(Camera*);

	void   rasterCreate(Raster*);
	uint8 *rasterLock(Raster*, int32 level);
	void   rasterUnlock(Raster*, int32 level);
	int32  rasterNumLevels(Raster*);
	void   rasterFromImage(Raster*, Image*);
}

}
