namespace rw {

// TODO: move more stuff into this

struct Engine
{
	void *currentCamera;
	void *currentWorld;
};

extern Engine engine;


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
