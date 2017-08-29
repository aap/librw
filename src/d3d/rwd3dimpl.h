namespace rw {
namespace d3d {

#ifdef RW_D3D9
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
uint8 *rasterLock(Raster *raster, int32 level);
void rasterUnlock(Raster *raster, int32 level);
int32 rasterNumLevels(Raster *raster);
void rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
