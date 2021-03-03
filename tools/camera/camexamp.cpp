#include <rw.h>
#include <skeleton.h>

#include "viewer.h"
#include "camexamp.h"

#define TEXSIZE 256

rw::Camera *MainCamera;
rw::Camera *SubCamera;

rw::Raster *SubCameraRaster;
rw::Raster *SubCameraZRaster;
rw::Raster *SubCameraMainCameraSubRaster;
rw::Raster *SubCameraMainCameraSubZRaster;

TextureCamera CameraTexture;

rw::int32 CameraSelected = 0;
rw::int32 ProjectionIndex = 0;
bool SubCameraMiniView = true;

CameraData SubCameraData;

void
CameraQueryData(CameraData *data, CameraDataType type, rw::Camera *camera)
{
	data->camera = camera;
	if(type & FARCLIPPLANE) data->farClipPlane = camera->farPlane;
	if(type & NEARCLIPPLANE) data->nearClipPlane = camera->nearPlane;
	if(type & PROJECTION) data->projection = camera->projection;
	if(type & OFFSET) data->offset = camera->viewOffset;
	if(type & VIEWWINDOW) data->viewWindow = camera->viewWindow;
	if(type & MATRIX) data->matrix = &camera->getFrame()->matrix;
}

void
CameraSetData(CameraData *data, CameraDataType type)
{
	if(type & FARCLIPPLANE) data->camera->setFarPlane(data->farClipPlane);
	if(type & NEARCLIPPLANE) data->camera->setNearPlane(data->nearClipPlane);
	if(type & PROJECTION) data->camera->setProjection(data->projection);
	if(type & OFFSET) data->camera->setViewOffset(&data->offset);
	if(type & VIEWWINDOW) data->camera->setViewWindow(&data->viewWindow);
}

void
ProjectionCallback(void)
{
	if(ProjectionIndex == 0)
		SubCameraData.projection = rw::Camera::PERSPECTIVE;
	else
		SubCameraData.projection = rw::Camera::PARALLEL;
	CameraSetData(&SubCameraData, PROJECTION);
}

void
ClipPlaneCallback(void)
{
	CameraSetData(&SubCameraData, (CameraDataType)(NEARCLIPPLANE | FARCLIPPLANE));
}

void
ChangeViewOffset(float deltaX, float deltaY)
{
	SubCameraData.offset.x += deltaX;
	SubCameraData.offset.y += deltaY;
	if(SubCameraData.offset.x > 5.0f)
		SubCameraData.offset.x = 5.0f;
	if(SubCameraData.offset.x < -5.0f)
		SubCameraData.offset.x = -5.0f;
	if(SubCameraData.offset.y > 5.0f)
		SubCameraData.offset.y = 5.0f;
	if(SubCameraData.offset.y < -5.0f)
		SubCameraData.offset.y = -5.0f;
	CameraSetData(&SubCameraData, OFFSET);
}

void
ChangeViewWindow(float deltaX, float deltaY)
{
	SubCameraData.viewWindow.x += deltaX;
	SubCameraData.viewWindow.y += deltaY;
	if(SubCameraData.viewWindow.x > 5.0f)
		SubCameraData.viewWindow.x = 5.0f;
	if(SubCameraData.viewWindow.x < 0.01f)
		SubCameraData.viewWindow.x = 0.01f;
	if(SubCameraData.viewWindow.y > 5.0f)
		SubCameraData.viewWindow.y = 5.0f;
	if(SubCameraData.viewWindow.y < 0.01f)
		SubCameraData.viewWindow.y = 0.01f;
	CameraSetData(&SubCameraData, VIEWWINDOW);
}

void
CamerasCreate(rw::World *world)
{
	rw::V3d offset = { 3.0f, 0.0f, 8.0f };
	float rotate = -90.0f;

	SubCamera = ViewerCreate(world);
	ViewerMove(SubCamera, &offset);
	ViewerRotate(SubCamera, rotate, 0.0f);

	MainCamera = ViewerCreate(world);

	CameraQueryData(&SubCameraData, ALL, SubCamera);

	SubCameraData.nearClipPlane = 0.3f;
	CameraSetData(&SubCameraData, NEARCLIPPLANE);

	SubCameraData.farClipPlane = 5.0f;
	CameraSetData(&SubCameraData, FARCLIPPLANE);

	CameraTexture.camera = SubCamera;
	CameraTextureInit(&CameraTexture);

	SubCameraData.cameraTexture = &CameraTexture;


	SubCameraMainCameraSubRaster = rw::Raster::create(0, 0, 0, rw::Raster::CAMERA);
	SubCameraMainCameraSubZRaster = rw::Raster::create(0, 0, 0, rw::Raster::ZBUFFER);
}

void
CamerasDestroy(rw::World *world)
{
	SubCameraMiniViewSelect(false);

	if(MainCamera){
		ViewerDestroy(MainCamera, world);
		MainCamera = nil;
	}

	if(SubCamera){
		ViewerDestroy(SubCamera, world);
		SubCamera = nil;
	}

	CameraTextureTerm(&CameraTexture);

	if(SubCameraMainCameraSubRaster){
		SubCameraMainCameraSubRaster->destroy();
		SubCameraMainCameraSubRaster = nil;
	}

	if(SubCameraMainCameraSubZRaster){
		SubCameraMainCameraSubZRaster->destroy();
		SubCameraMainCameraSubZRaster = nil;
	}
}

void
UpdateSubRaster(rw::Camera *camera, rw::Rect *rect)
{
	rw::Rect subRect;

	subRect.x = rect->w * 0.75f;
	subRect.y = 0;

	subRect.w = rect->w * 0.25f;
	subRect.h = rect->h * 0.25f;

	SubCameraMainCameraSubRaster->subRaster(camera->frameBuffer, &subRect);
	SubCameraMainCameraSubZRaster->subRaster(camera->zBuffer, &subRect);
}

void
CameraSizeUpdate(rw::Rect *rect, float viewWindow, float aspectRatio)
{
	static bool RasterInit;

	if(MainCamera == nil)
		return;

	sk::CameraSize(MainCamera, rect, viewWindow, aspectRatio);

	UpdateSubRaster(MainCamera, rect);

	if(RasterInit)
		SubCameraMiniViewSelect(false);

	sk::CameraSize(SubCamera, rect, viewWindow, aspectRatio);

	SubCameraRaster = SubCamera->frameBuffer;
	SubCameraZRaster = SubCamera->zBuffer;

	RasterInit = true;
	SubCameraMiniViewSelect(CameraSelected == 0);

	CameraQueryData(&SubCameraData, VIEWWINDOW, SubCamera);
}

void
RenderSubCamera(rw::RGBA *backgroundColor, rw::int32 clearMode, rw::World *world)
{
	SubCamera->clear(backgroundColor, clearMode);
	SubCamera->beginUpdate();
	world->render();
	SubCamera->endUpdate();
}

void
RenderTextureCamera(rw::RGBA *foregroundColor, rw::int32 clearMode, rw::World *world)
{
	rw::Raster *saveRaster, *saveZRaster;

	saveRaster = CameraTexture.camera->frameBuffer;
	saveZRaster = CameraTexture.camera->zBuffer;
	CameraTexture.camera->frameBuffer = CameraTexture.raster;
	CameraTexture.camera->zBuffer = CameraTexture.zRaster;

	CameraTexture.camera->clear(foregroundColor, clearMode);
	CameraTexture.camera->beginUpdate();
	world->render();
	CameraTexture.camera->endUpdate();


	CameraTexture.camera->frameBuffer = saveRaster;
	CameraTexture.camera->zBuffer = saveZRaster;
}

void
SubCameraMiniViewSelect(bool select)
{
	if(select){
		SubCamera->frameBuffer = SubCameraMainCameraSubRaster;
		SubCamera->zBuffer = SubCameraMainCameraSubZRaster;
	}else{
		SubCamera->frameBuffer = SubCameraRaster;
		SubCamera->zBuffer = SubCameraZRaster;
	}
}



void
CameraTextureInit(TextureCamera *ct)
{
	ct->raster = rw::Raster::create(TEXSIZE, TEXSIZE, 0, rw::Raster::CAMERATEXTURE);
	assert(ct->raster);
	ct->zRaster = rw::Raster::create(TEXSIZE, TEXSIZE, 0, rw::Raster::ZBUFFER);
	assert(ct->zRaster);

	ct->texture = rw::Texture::create(ct->raster);
	ct->texture->setFilter(rw::Texture::FilterMode::LINEAR);
}

void
CameraTextureTerm(TextureCamera *ct)
{
	if(ct->raster){	
		ct->raster->destroy();
		ct->raster = nil;
	}

	if(ct->zRaster){	
		ct->zRaster->destroy();
		ct->zRaster = nil;
	}

	if(ct->texture){
		ct->texture->raster = nil;
		ct->texture->destroy();
		ct->texture = nil;
	}
}



void
DrawCameraFrustum(CameraData *c)
{
	rw::RGBA yellow = { 255, 255, 0, 64 };
	rw::RGBA red = { 255, 0, 0, 255 };
	rw::RWDEVICE::Im3DVertex frustum[13];
	// lines
	rw::uint16 indicesL[] = {
		1,  2,  2,  3,  3,  4,  4,  1,
		5,  6,  6,  7,  7,  8,  8,  5,
		9, 10, 10, 11, 11, 12, 12,  9,
		5,  9,  6, 10,  7, 11,  8, 12,
		0,  0
	};
	// triangles
	rw::uint16 indicesT[] = {
		 5,  6, 10,
		10,  9,  5,
		 6,  7, 11,
		11, 10,  6,
		 7,  8, 12,
		12, 11,  7,
		 8,  5,  9,
		 9, 12,  8,
		
		 7,  6,  5,
		 5,  8,  7,
		 9, 10, 11,
		11, 12,  9
	};
	float signs[4][2] = {
		{  1,  1 },
		{ -1,  1 },
		{ -1, -1 },
		{  1, -1 }
	};

	float depth[3];
	depth[0] = 1.0f;	// view window
	depth[1] = c->nearClipPlane;
	depth[2] = c->farClipPlane;

	int k = 0;
	frustum[k].setX(c->offset.x);
	frustum[k].setY(c->offset.y);
	frustum[k].setZ(0.0f);
	k++;

	for(int i = 0; i < 3; i++)	// depths
		for(int j = 0; j < 4; j++){	// planes
			if(c->projection == rw::Camera::PERSPECTIVE){
				frustum[k].setX(-c->offset.x + depth[i]*(signs[j][0]*c->viewWindow.x + c->offset.x));
				frustum[k].setY(c->offset.y + depth[i]*(signs[j][1]*c->viewWindow.y - c->offset.y));
				frustum[k].setZ(depth[i]);
			}else{
				frustum[k].setX(-c->offset.x + signs[j][0]*c->viewWindow.x + depth[i]*c->offset.x);
				frustum[k].setY(c->offset.y + signs[j][1]*c->viewWindow.y - depth[i]*c->offset.y);
				frustum[k].setZ(depth[i]);
			}
			k++;
		}

	for(int i = 0; i < 5; i++)
		frustum[i].setColor(red.red, red.green, red.blue, red.alpha);
	for(int i = 5; i < 13; i++)
		frustum[i].setColor(yellow.red, yellow.green, yellow.blue, 255);

	rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);

	rw::im3d::Transform(frustum, 13, c->camera->getFrame()->getLTM(), rw::im3d::ALLOPAQUE);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPELINELIST, indicesL, 34);
	rw::im3d::End();

	for(int i = 5; i < 13; i++)
		frustum[i].setColor(yellow.red, yellow.green, yellow.blue, yellow.alpha);

	rw::SetRenderState(rw::VERTEXALPHA, 1);

	rw::im3d::Transform(frustum, 13, c->camera->getFrame()->getLTM(), rw::im3d::ALLOPAQUE);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indicesT, 36);
	rw::im3d::End();
}

void
DrawCameraViewplaneTexture(CameraData *c)
{
	rw::RGBA white = { 255, 255, 255, 255 };
	rw::RWDEVICE::Im3DVertex frustum[4];
	rw::uint16 indicesV[] = {
		2, 1, 0,
		0, 3, 2,
		0, 1, 2,
		2, 3, 0
	};
	float uvValues[4][2] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f },
		{ 0.0f, 1.0f }
	};
	float signs[4][2] = {
		{  1,  1 },
		{ -1,  1 },
		{ -1, -1 },
		{  1, -1 }
	};

	for(int j = 0; j < 4; j++){
		frustum[j].setX(signs[j][0]*c->viewWindow.x);
		frustum[j].setY(signs[j][1]*c->viewWindow.y);
		frustum[j].setZ(1.0f);
	}
	for(int i = 0; i < 4; i++){
		frustum[i].setColor(white.red, white.green, white.blue, white.alpha);
		frustum[i].setU(uvValues[i][0]);
		frustum[i].setV(uvValues[i][1]);
	}

	rw::SetRenderState(rw::VERTEXALPHA, 1);
	rw::SetRenderStatePtr(rw::TEXTURERASTER, c->cameraTexture->texture->raster);

	rw::im3d::Transform(frustum, 4, c->camera->getFrame()->getLTM(), rw::im3d::VERTEXUV);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, indicesV, 12);
	rw::im3d::End();
}
