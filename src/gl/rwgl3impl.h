namespace rw {
namespace gl3 {

#ifdef RW_OPENGL

extern Shader *simpleShader;
extern uint32 im2DVbo, im2DIbo;
void openIm2D(void);
void closeIm2D(void);
void im2DRenderIndexedPrimitive(PrimitiveType primType,
   void *vertices, int32 numVertices, void *indices, int32 numIndices);

void openIm3D(void);
void closeIm3D(void);
void im3DTransform(void *vertices, int32 numVertices, Matrix *world);
void im3DRenderIndexed(PrimitiveType primType, void *indices, int32 numIndices);
void im3DEnd(void);
#endif

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster*, int32 level);
void rasterUnlock(Raster*, int32);
int32 rasterNumLevels(Raster*);
void rasterFromImage(Raster *raster, Image *image);

}
}
