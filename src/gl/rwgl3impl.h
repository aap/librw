namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

extern Shader *simpleShader;

#endif

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level);
void rasterUnlock(Raster*, int32);
int32 rasterNumLevels(Raster*);
void rasterFromImage(Raster *raster, Image *image);

}
}
