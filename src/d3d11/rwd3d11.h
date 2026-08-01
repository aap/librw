#ifdef RW_D3D11
#include <windows.h>
#include <d3d11.h>
#endif

#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
struct SDL_Window;
#elif defined(LIBRW_GLFW)
struct GLFWwindow;
#endif

namespace rw {

#ifdef RW_D3D11
struct EngineOpenParams
{
#if defined(LIBRW_SDL3) || defined(LIBRW_SDL2)
	::SDL_Window **window;
	bool32 fullscreen;
#elif defined(LIBRW_GLFW)
	::GLFWwindow **window;
#else
	HWND window;
#endif
	int width, height;
	const char *windowtitle;
};
#endif

namespace d3d11 {

void registerPlatformPlugins(void);

#ifdef RW_D3D11
extern Device renderdevice;

struct Im3DVertex
{
	V3d     position;
	V3d     normal;
	uint8   r, g, b, a;
	float32 u, v;

	void setX(float32 x) { this->position.x = x; }
	void setY(float32 y) { this->position.y = y; }
	void setZ(float32 z) { this->position.z = z; }
	void setNormalX(float32 x) { this->normal.x = x; }
	void setNormalY(float32 y) { this->normal.y = y; }
	void setNormalZ(float32 z) { this->normal.z = z; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u) { this->u = u; }
	void setV(float32 v) { this->v = v; }

	float getX(void) { return this->position.x; }
	float getY(void) { return this->position.y; }
	float getZ(void) { return this->position.z; }
	float getNormalX(void) { return this->normal.x; }
	float getNormalY(void) { return this->normal.y; }
	float getNormalZ(void) { return this->normal.z; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

struct Im2DVertex
{
	float32 x, y, z, w;
	uint8   r, g, b, a;
	float32 u, v;

	void setScreenX(float32 x) { this->x = x; }
	void setScreenY(float32 y) { this->y = y; }
	void setScreenZ(float32 z) { this->z = z; }
	void setCameraZ(float32 z) { this->w = z; }
	void setRecipCameraZ(float32 recipz) { this->w = 1.0f/recipz; }
	void setColor(uint8 r, uint8 g, uint8 b, uint8 a) {
		this->r = r; this->g = g; this->b = b; this->a = a; }
	void setU(float32 u, float recipz) { (void)recipz; this->u = u; }
	void setV(float32 v, float recipz) { (void)recipz; this->v = v; }

	float getScreenX(void) { return this->x; }
	float getScreenY(void) { return this->y; }
	float getScreenZ(void) { return this->z; }
	float getCameraZ(void) { return this->w; }
	float getRecipCameraZ(void) { return 1.0f/this->w; }
	RGBA getColor(void) { return makeRGBA(this->r, this->g, this->b, this->a); }
	float getU(void) { return this->u; }
	float getV(void) { return this->v; }
};

struct D3D11Raster
{
	int32 bpp;
	bool32 hasAlpha;
	int32 numLevels;
	RasterLevels *levels;
	ID3D11Texture2D *texture;
	ID3D11ShaderResourceView *srv;
	ID3D11RenderTargetView *rtv;
	ID3D11DepthStencilView *dsv;
	bool32 gpuDirty;
	bool32 gpuReady;
};

struct InstanceData
{
	uint32 numIndex;
	uint32 minVert;
	Material *material;
	bool32 vertexAlpha;
	uint32 baseIndex;
	uint32 numVertices;
	uint32 startIndex;
	uint32 numPrimitives;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32 serialNumber;
	uint32 numMeshes;
	uint32 primType;
	uint32 totalNumIndex;
	uint32 totalNumVertex;
	ID3D11Buffer *vertexBuffer;
	ID3D11Buffer *indexBuffer;
	Im3DVertex *vertices;
	uint16 *indices;
	InstanceData *inst;
	bool32 gpuDirty;
};

class ObjPipeline : public rw::ObjPipeline
{
public:
	void init(void);
	static ObjPipeline *create(void);

	void (*instanceCB)(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
	void (*uninstanceCB)(Geometry *geo, InstanceDataHeader *header);
	void (*renderCB)(Atomic *atomic, InstanceDataHeader *header);
};

extern int32 nativeRasterOffset;
#define GETD3D11RASTEREXT(raster) PLUGINOFFSET(rw::d3d11::D3D11Raster, raster, rw::d3d11::nativeRasterOffset)

Raster *rasterCreate(Raster *raster);
uint8 *rasterLock(Raster *raster, int32 level, int32 lockMode);
void rasterUnlock(Raster *raster, int32 level);
int32 rasterNumLevels(Raster *raster);
bool32 imageFindRasterFormat(Image *img, int32 type,
	int32 *width, int32 *height, int32 *depth, int32 *format);
bool32 rasterFromImage(Raster *raster, Image *image);
Image *rasterToImage(Raster *raster);
void registerNativeRaster(void);
bool32 ensureTextureUploaded(Raster *raster);
ID3D11RenderTargetView *getRasterRTV(Raster *raster);
ID3D11DepthStencilView *getRasterDSV(Raster *raster);
void destroyRasterTexture(Raster *raster);
Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

void freeInstanceData(Geometry *geometry);
void *destroyNativeData(void *object, int32 offset, int32 size);
Stream *readNativeData(Stream *stream, int32 len, void *object, int32 offset, int32 size);
Stream *writeNativeData(Stream *stream, int32 len, void *object, int32 offset, int32 size);
int32 getSizeNativeData(void *object, int32 offset, int32 size);

ObjPipeline *makeDefaultPipeline(void);
void defaultInstanceCB(Geometry *geo, InstanceDataHeader *header, bool32 reinstance);
void defaultUninstanceCB(Geometry *geo, InstanceDataHeader *header);
void defaultRenderCB(Atomic *atomic, InstanceDataHeader *header);
void initSkin(void);
void initMatFX(void);
ID3D11Device *getDevice(void);
ID3D11DeviceContext *getContext(void);
#endif

}
}
