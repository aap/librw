#include <rw.h>
#include <skeleton.h>

rw::Camera*
ViewerCreate(rw::World *world)
{
	rw::Camera *camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
	assert(camera);
	camera->setNearPlane(0.1f);
	camera->setFarPlane(500.0f);
	world->addCamera(camera);
	return camera;
}

void
ViewerDestroy(rw::Camera *camera, rw::World *world)
{
	if(camera && world){
		world->removeCamera(camera);
		sk::CameraDestroy(camera);
	}
}

void
ViewerMove(rw::Camera *camera, rw::V3d *offset)
{
	sk::CameraMove(camera, offset);
}

void
ViewerRotate(rw::Camera *camera, float deltaX, float deltaY)
{
	sk::CameraTilt(camera, nil, deltaY);
	sk::CameraPan(camera, nil, deltaX);
}

void
ViewerTranslate(rw::Camera *camera, float deltaX, float deltaY)
{
	rw::V3d offset;
	offset.x = deltaX;
	offset.y = deltaY;
	offset.z = 0.0f;
	sk::CameraMove(camera, &offset);
}

void
ViewerSetPosition(rw::Camera *camera, rw::V3d *position)
{
	camera->getFrame()->translate(position, rw::COMBINEREPLACE);
}
