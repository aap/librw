namespace rw {
namespace d3d {

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level);
void rasterUnlock(Raster *raster, int32 level);
int32 rasterNumLevels(Raster *raster);
void rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
