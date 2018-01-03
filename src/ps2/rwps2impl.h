namespace rw {
namespace ps2 {

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level);
void rasterUnlock(Raster*, int32);
int32 rasterNumLevels(Raster*);
void rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
