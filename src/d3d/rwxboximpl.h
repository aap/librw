namespace rw {
namespace xbox {

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level);
void rasterUnlock(Raster*, int32);
int32 rasterNumLevels(Raster *raster);
Image *rasterToImage(Raster *raster);

}
}
