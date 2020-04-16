namespace rw {
namespace d3d {

#ifdef RW_D3D9
void openIm2D(void);
void closeIm2D(void);
void im2DRenderLine(void *vertices, int32 numVertices,
  int32 vert1, int32 vert2);
void im2DRenderTriangle(void *vertices, int32 numVertices,
  int32 vert1, int32 vert2, int32 vert3);
void im2DRenderPrimitive(PrimitiveType primType,
   void *vertices, int32 numVertices);
void im2DRenderIndexedPrimitive(PrimitiveType primType,
   void *vertices, int32 numVertices, void *indices, int32 numIndices);

void openIm3D(void);
void closeIm3D(void);
void im3DTransform(void *vertices, int32 numVertices, Matrix *world);
void im3DRenderIndexed(PrimitiveType primType, void *indices, int32 numIndices);
void im3DEnd(void);


struct DisplayMode
{
	D3DDISPLAYMODE mode;
	uint32 flags;
};

struct D3d9Globals
{
	HWND window;

	IDirect3D9 *d3d9;
	int numAdapters;
	int adapter;
	D3DCAPS9 caps;
	DisplayMode *modes;
	int numModes;
	int currentMode;

	D3DPRESENT_PARAMETERS present;
};

extern D3d9Globals d3d9Globals;

int findFormatDepth(uint32 format);
#endif

void rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level, int32 lockMode);
void rasterUnlock(Raster *raster, int32 level);
int32 rasterNumLevels(Raster *raster);
void rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
