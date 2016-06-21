#include <cstdio>
#include <cstdlib>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID 0

namespace rw {

void
defaultBeginUpdateCB(Camera *cam)
{
	engine.currentCamera = cam;
	Frame::syncDirty();
	DRIVER.beginUpdate(cam);
}

void
defaultEndUpdateCB(Camera *cam)
{
	DRIVER.endUpdate(cam);
}

static void
cameraSync(ObjectWithFrame*)
{
}

Camera*
Camera::create(void)
{
	Camera *cam = (Camera*)malloc(PluginBase::s_size);
	if(cam == nil){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return nil;
	}
	cam->object.object.init(Camera::ID, 0);
	cam->object.syncCB = cameraSync;
	cam->viewWindow.set(1.0f, 1.0f);
	cam->viewOffset.set(0.0f, 0.0f);
	cam->nearPlane = 0.05f;
	cam->farPlane = 10.0f;
	cam->fogPlane = 5.0f;
	cam->projection = Camera::PERSPECTIVE;
	cam->clump = nil;
	cam->inClump.init();

	cam->beginUpdateCB = defaultBeginUpdateCB;
	cam->endUpdateCB = defaultEndUpdateCB;

	cam->constructPlugins();
	return cam;
}

Camera*
Camera::clone(void)
{
	Camera *cam = Camera::create();
	if(cam == nil)
		return nil;
	cam->object.object.copy(&this->object.object);
	cam->setFrame(this->getFrame());
	cam->viewWindow = this->viewWindow;
	cam->viewOffset = this->viewOffset;
	cam->nearPlane = this->nearPlane;
	cam->farPlane = this->farPlane;
	cam->fogPlane = this->fogPlane;
	cam->projection = this->projection;
	cam->copyPlugins(this);
	return cam;
}

void
Camera::destroy(void)
{
	this->destructPlugins();
	if(this->clump)
		this->inClump.remove();
	free(this);
}

struct CameraChunkData
{
	V2d viewWindow;
	V2d viewOffset;
	float32 nearPlane, farPlane;
	float32 fogPlane;
	int32 projection;
};

Camera*
Camera::streamRead(Stream *stream)
{
	CameraChunkData buf;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	stream->read(&buf, sizeof(CameraChunkData));
	Camera *cam = Camera::create();
	cam->viewWindow = buf.viewWindow;
	cam->viewOffset = buf.viewOffset;
	cam->nearPlane = buf.nearPlane;
	cam->farPlane = buf.farPlane;
	cam->fogPlane = buf.fogPlane;
	cam->projection = buf.projection;
	if(cam->streamReadPlugins(stream))
		return cam;
	cam->destroy();
	return nil;
}

bool
Camera::streamWrite(Stream *stream)
{
	CameraChunkData buf;
	writeChunkHeader(stream, ID_CAMERA, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, sizeof(CameraChunkData));
	buf.viewWindow = this->viewWindow;
	buf.viewOffset = this->viewOffset;
	buf.nearPlane = this->nearPlane;
	buf.farPlane  = this->farPlane;
	buf.fogPlane = this->fogPlane;
	buf.projection = this->projection;
	stream->write(&buf, sizeof(CameraChunkData));
	this->streamWritePlugins(stream);
	return true;
}

uint32
Camera::streamGetSize(void)
{
	return 12 + sizeof(CameraChunkData) + 12 + this->streamGetPluginSize();
}

void
Camera::updateProjectionMatrix(void)
{
	float32 invwx = 1.0f/this->viewWindow.x;
	float32 invwy = 1.0f/this->viewWindow.y;
	float32 invz = 1.0f/(this->farPlane-this->nearPlane);
	if(rw::platform == PLATFORM_D3D8 || rw::platform == PLATFORM_D3D9 ||
	   rw::platform == PLATFORM_XBOX){
		// is this all really correct?
		this->projMat[0] = invwx;
		this->projMat[1] = 0.0f;
		this->projMat[2] = 0.0f;
		this->projMat[3] = 0.0f;

		this->projMat[4] = 0.0f;
		this->projMat[5] = invwy;
		this->projMat[6] = 0.0f;
		this->projMat[7] = 0.0f;

		if(this->projection == PERSPECTIVE){
			this->projMat[8] = this->viewOffset.x*invwx;
			this->projMat[9] = this->viewOffset.y*invwy;
			this->projMat[10] = this->farPlane*invz;
			this->projMat[11] = 1.0f;

			this->projMat[12] = 0.0f;
			this->projMat[13] = 0.0f;
			this->projMat[14] = -this->nearPlane*this->projMat[10];
			this->projMat[15] = 0.0f;
		}else{
			this->projMat[8] = 0.0f;
			this->projMat[9] = 0.0f;
			this->projMat[10] = invz;
			this->projMat[11] = 0.0f;

			this->projMat[12] = this->viewOffset.x*invwx;
			this->projMat[13] = this->viewOffset.y*invwy;
			this->projMat[14] = -this->nearPlane*this->projMat[10];
			this->projMat[15] = 1.0f;
		}
	}else if(rw::platform == PLATFORM_WDGL || rw::platform == PLATFORM_GL3){
		this->projMat[0] = invwx;
		this->projMat[1] = 0.0f;
		this->projMat[2] = 0.0f;
		this->projMat[3] = 0.0f;

		this->projMat[4] = 0.0f;
		this->projMat[5] = invwy;
		this->projMat[6] = 0.0f;
		this->projMat[7] = 0.0f;

		if(this->projection == PERSPECTIVE){
			this->projMat[8] = this->viewOffset.x*invwx;
			this->projMat[9] = this->viewOffset.y*invwy;
			this->projMat[10] = (this->farPlane+this->nearPlane)*invz;
			this->projMat[11] = 1.0f;

			this->projMat[12] = 0.0f;
			this->projMat[13] = 0.0f;
			this->projMat[14] = -2.0f*this->nearPlane*this->farPlane*invz;
			this->projMat[15] = 0.0f;
		}else{
		}
	}
}

void
Camera::setFOV(float32 fov, float32 ratio)
{
	float32 a = tan(fov*3.14159f/360.0f);
	this->viewWindow.x = a;
	this->viewWindow.y = a/ratio;
	this->viewOffset.set(0.0f, 0.0f);
}

}
