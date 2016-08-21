namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

extern Shader *simpleShader;
extern uint32 im2DVbo, im2DIbo;
void im2DInit(void);
void im2DRenderIndexedPrimitive(PrimitiveType primType,
   void *vertices, int32 numVertices, void *indices, int32 numIndices);

#endif

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level);
void rasterUnlock(Raster*, int32);
int32 rasterNumLevels(Raster*);
void rasterFromImage(Raster *raster, Image *image);

}
}
