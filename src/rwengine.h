namespace rw {

// TODO: move more stuff into this
struct Engine
{
	void *currentCamera;
	void *currentWorld;

	static PluginList s_plglist;
	static void init(void);
	static void open(void);
        static int32 registerPlugin(int32 size, uint32 id, Constructor ctor,
			Destructor dtor){
		return s_plglist.registerPlugin(size, id, ctor, dtor, nil);
	}
};

extern Engine *engine;

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
};

extern Driver driver[NUM_PLATFORMS];
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
