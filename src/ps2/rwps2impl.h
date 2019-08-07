namespace rw {
namespace ps2 {

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level, int32 lockMode);
void rasterUnlock(Raster*, int32 level);
uint8 *rasterLockPalette(Raster*, int32 lockMode);
void rasterUnlockPalette(Raster*);
int32 rasterNumLevels(Raster*);
void rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
