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
	engine->beginUpdate(cam);
}

void
defaultEndUpdateCB(Camera *cam)
{
	engine->endUpdate(cam);
}

static void
cameraSync(ObjectWithFrame *obj)
{
	/*
	 * RW projection matrix looks like this:
	 *       (cf. Camera View Matrix white paper)
	 * w = viewWindow width
	 * h = viewWindow height
	 * o = view offset
	 *
	 * perspective:
	 * 1/2w       0    ox/2w + 1/2   -ox/2w
	 *    0   -1/2h   -oy/2h + 1/2    oy/2h
	 *    0       0              1        0
	 *    0       0              1        0
	 *
	 * parallel:
	 * 1/2w       0    ox/2w   -ox/2w + 1/2
	 *    0   -1/2h   -oy/2h    oy/2h + 1/2
	 *    0       0        1              0
	 *    0       0        0              1
	 *
	 * The view matrix transforms from world to clip space, it is however
	 * not used for OpenGL or D3D since transformation to camera space
	 * and to clip space are handled by separate matrices there.
	 * On these platforms the two matrices are built in the platform's
	 * beginUpdate function.
	 * On the PS2 the 1/2 translation/shear is removed again on the VU1.
	 *
	 * RW builds this matrix directly without using explicit
	 * inversion and matrix multiplication.
	 */

	Camera *cam = (Camera*)obj;
	Matrix inv, proj;
	Matrix::invertOrthonormal(&inv, cam->getFrame()->getLTM());
	float32 xscl = 2.0f/cam->viewWindow.x;
	float32 yscl = 2.0f/cam->viewWindow.y;

	proj.right.x = xscl;
	proj.right.y = 0.0f;
	proj.right.z = 0.0f;
	proj.rightw = 0.0f;

	proj.up.x = 0.0f;
	proj.up.y = -yscl;
	proj.up.z = 0.0f;
	proj.upw = 0.0f;

	if(cam->projection == Camera::PERSPECTIVE){
		proj.pos.x = -cam->viewOffset.x*xscl;
		proj.pos.y = cam->viewOffset.y*yscl;
		proj.pos.z = 0.0f;
		proj.posw = 0.0f;

		proj.at.x = -proj.pos.x + 0.5f;
		proj.at.y = -proj.pos.y + 0.5f;
		proj.at.z = 1.0f;
		proj.atw = 1.0f;
	}else{
		proj.at.x = cam->viewOffset.x*xscl;
		proj.at.y = -cam->viewOffset.y*yscl;
		proj.at.z = 1.0f;
		proj.atw = 0.0f;

		proj.pos.x = -proj.at.x + 0.5f;
		proj.pos.y = -proj.at.y + 0.5f;
		proj.pos.z = 0.0f;
		proj.posw = 1.0f;
	}
	Matrix::mult(&cam->viewMatrix, &proj, &inv);
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
	engine->clearCamera(this, col, mode);
}

void
calczShiftScale(Camera *cam)
{
	float32 n = cam->nearPlane;
	float32 f = cam->farPlane;
	float32 N = engine->zNear;
	float32 F = engine->zFar;
	// RW does this
	N += (F - N)/10000.0f;
	F -= (F - N)/10000.0f;
	if(cam->projection == Camera::PERSPECTIVE){
		cam->zScale = (N - F)*n*f/(f - n);
		cam->zShift = (F*f - N*n)/(f - n);
	}else{
		cam->zScale = (F - N)/(f -n);
		cam->zShift = (N*f - F*n)/(f - n);
	}
}

void
Camera::setNearPlane(float32 near)
{
	this->nearPlane = near;
	calczShiftScale(this);
}

void
Camera::setFarPlane(float32 far)
{
	this->farPlane = far;
	calczShiftScale(this);
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
