#ifdef RW_D3D11
#ifdef WITH_D3D
#include <d3d11.h>
#include <dxgi.h>
#define MAX_D3D11_ADAPTERS 16
#endif
#endif

namespace rw {
namespace d3d {

#if defined(RW_D3D9) || defined(RW_D3D11)
void openIm2D(void);
void closeIm2D(void);
void im2DRenderLine(void *vertices, int32 numVertices, int32 vert1, int32 vert2);
void im2DRenderTriangle(void *vertices, int32 numVertices, int32 vert1, int32 vert2, int32 vert3);
void im2DRenderPrimitive(PrimitiveType primType, void *vertices, int32 numVertices);
void im2DRenderIndexedPrimitive(PrimitiveType primType, void *vertices, int32 numVertices, void *indices, int32 numIndices);

void openIm3D(void);
void closeIm3D(void);
void im3DTransform(void *vertices, int32 numVertices, Matrix *world, uint32 flags);
void im3DRenderPrimitive(PrimitiveType primType);
void im3DRenderIndexedPrimitive(PrimitiveType primType, void *indices, int32 numIndices);
void im3DEnd(void);

#endif

#ifdef RW_D3D9
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
	DisplayMode startMode;
	
	uint32 msLevel;

	D3DPRESENT_PARAMETERS present;

	IDirect3DSurface9 *defaultRenderTarget;
	IDirect3DSurface9 *defaultDepthSurf;

	int numTextures;
	int numVertexShaders;
	int numPixelShaders;
	int numVertexBuffers;
	int numIndexBuffers;
	int numVertexDeclarations;
};

extern D3d9Globals d3d9Globals;

void addVidmemRaster(Raster *raster);
void removeVidmemRaster(Raster *raster);

void addDynamicVB(uint32 length, uint32 fvf, IDirect3DVertexBuffer9** buf);	// NB: don't share this pointer
void removeDynamicVB(IDirect3DVertexBuffer9** buf);

void addDynamicIB(uint32 length, IDirect3DIndexBuffer9** buf);	// NB: don't share this pointer
void removeDynamicIB(IDirect3DIndexBuffer9** buf);

int findFormatDepth(uint32 format);
void evictD3D9Raster(Raster *raster);

#endif

#ifdef RW_D3D11

struct DisplayMode
{
	DXGI_MODE_DESC mode;
	uint32 flags;
};

struct D3d11Globals
{
	HWND window;

	IDXGIFactory* factory;
	int numAdapters;
	int adapter;
	IDXGIAdapter* adapters[MAX_D3D11_ADAPTERS];
	DXGI_ADAPTER_DESC adapterDescs[MAX_D3D11_ADAPTERS];

	IDXGIOutput* output;

	int numModes;
	int currentMode;
	DisplayMode* modes;
	DisplayMode startMode;

	uint32 msLevel;

	ID3D11Device* d3ddevice;
	ID3D11DeviceContext* context;
	IDXGISwapChain* swapChain;
	D3D_FEATURE_LEVEL featureLevel;

	DXGI_SWAP_CHAIN_DESC present;
	ID3D11RenderTargetView* defaultRenderTarget;
	ID3D11DepthStencilView* defaultDepthStencilView;

	int numTextures;
	int numVertexShaders;
	int numPixelShaders;
	int numVertexBuffers;
	int numIndexBuffers;
	int numInputLayouts;

};

extern D3d11Globals d3d11Globals;

void addDynamicVB(uint32 length, uint32 stride, ID3D11Buffer** buf);	// NB: don't share this pointer
void removeDynamicVB(ID3D11Buffer** buf);

void addDynamicIB(uint32 length, ID3D11Buffer** buf);	// NB: don't share this pointer
void removeDynamicIB(ID3D11Buffer** buf);



#endif

Raster *rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level, int32 lockMode);
void rasterUnlock(Raster *raster, int32 level);
int32 rasterNumLevels(Raster *raster);
bool32 imageFindRasterFormat(Image *img, int32 type,
	int32 *width, int32 *height, int32 *depth, int32 *format);
bool32 rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);

}
}
