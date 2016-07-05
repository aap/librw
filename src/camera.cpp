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
	engine->currentCamera = cam;
	Frame::syncDirty();
	DRIVER->beginUpdate(cam);
}

void
defaultEndUpdateCB(Camera *cam)
{
	DRIVER->endUpdate(cam);
}

static void
cameraSync(ObjectWithFrame*)
{
	// TODO: calculate view matrix here (i.e. projection * inverseLTM)
	// RW projection matrix looks like this:
	//       (cf. Camera View Matrix white paper)
	// w = viewWindow width
	// h = viewWindow height
	// o = view offset
	//
	// 1/2w       0    ox/2w + 1/2   -ox/2w
	//    0   -1/2h   -oy/2h + 1/2    oy/2h
	//    0       0              1        0
}

void
worldBeginUpdateCB(Camera *cam)
{
	engine->currentWorld = cam->world;
	cam->originalBeginUpdate(cam);
}

void
worldEndUpdateCB(Camera *cam)
{
	cam->originalEndUpdate(cam);
}

static void
worldCameraSync(ObjectWithFrame *obj)
{
	Camera *camera = (Camera*)obj;
	camera->originalSync(obj);
}

Camera*
Camera::create(void)
{
	Camera *cam = (Camera*)malloc(s_plglist.size);
	if(cam == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	cam->object.object.init(Camera::ID, 0);
	cam->object.syncCB = cameraSync;
	cam->beginUpdateCB = defaultBeginUpdateCB;
	cam->endUpdateCB = defaultEndUpdateCB;
	cam->viewWindow.set(1.0f, 1.0f);
	cam->viewOffset.set(0.0f, 0.0f);
	cam->nearPlane = 0.05f;
	cam->farPlane = 10.0f;
	cam->fogPlane = 5.0f;
	cam->projection = Camera::PERSPECTIVE;

	// clump extension
	cam->clump = nil;
	cam->inClump.init();

	// world extension
	cam->world = nil;
	cam->originalSync = cam->object.syncCB;
	cam->originalBeginUpdate = cam->beginUpdateCB;
	cam->originalEndUpdate = cam->endUpdateCB;
	cam->object.syncCB = worldCameraSync;
	cam->beginUpdateCB = worldBeginUpdateCB;
	cam->endUpdateCB = worldEndUpdateCB;

	s_plglist.construct(cam);
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
	s_plglist.copy(cam, this);
	return cam;
}

void
Camera::destroy(void)
{
	s_plglist.destruct(this);
	if(this->clump)
		this->inClump.remove();
	free(this);
}

void
Camera::clear(RGBA *col, uint32 mode)
{
	DRIVER->clearCamera(this, col, mode);
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
	if(s_plglist.streamRead(stream, cam))
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
	s_plglist.streamWrite(stream, this);
	return true;
}

uint32
Camera::streamGetSize(void)
{
	return 12 + sizeof(CameraChunkData) + 12 +
	       s_plglist.streamGetSize(this);
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
