struct TextureCamera
{
	rw::Raster *raster;
	rw::Raster *zRaster;
	rw::Camera *camera;
	rw::Texture *texture;
};

struct CameraData
{
	float farClipPlane;
	float nearClipPlane;
	rw::uint32 projection;
	rw::V2d offset;
	rw::V2d viewWindow;
	rw::Camera *camera;
	TextureCamera *cameraTexture;
	rw::Matrix *matrix;
};

enum CameraDataType
{
	NONE            = 0x00,
	FARCLIPPLANE    = 0x01,
	NEARCLIPPLANE   = 0x02,
	PROJECTION      = 0x04,
	OFFSET          = 0x08,
	VIEWWINDOW      = 0x10,
	MATRIX          = 0x20,
	ALL             = 0xFF
};

extern rw::Camera *MainCamera;
extern rw::Camera *SubCamera;

extern rw::int32 CameraSelected;
extern rw::int32 ProjectionIndex;

extern CameraData SubCameraData;

void CameraQueryData(CameraData *data, CameraDataType type, rw::Camera *camera);
void CameraSetData(CameraData *data, CameraDataType type);

void ChangeViewOffset(float deltaX, float deltaY);
void ChangeViewWindow(float deltaX, float deltaY);
void ProjectionCallback(void);
void ClipPlaneCallback(void);

void CamerasCreate(rw::World *world);
void CamerasDestroy(rw::World *world);
void CameraSizeUpdate(rw::Rect *rect, float viewWindow, float aspectRatio);
void RenderSubCamera(rw::RGBA *foregroundColor, rw::int32 clearMode, rw::World *world);
void RenderTextureCamera(rw::RGBA *foregroundColor, rw::int32 clearMode, rw::World *world);
void SubCameraMiniViewSelect(bool select);

void CameraTextureInit(TextureCamera *ct);
void CameraTextureTerm(TextureCamera *ct);
void DrawCameraFrustum(CameraData *c);
void DrawCameraViewplaneTexture(CameraData *c);

void ViewerRotate(rw::Camera *camera, float deltaX, float deltaY);
void ViewerTranslate(rw::Camera *camera, float deltaX, float deltaY);
