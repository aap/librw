#include <rw.h>
#include <skeleton.h>

#include "subrast.h"

rw::Camera *Camera;
rw::Camera *SubCameras[4];

void
CameraSetViewWindow(rw::Camera *camera, float width, float height, float vw)
{
	rw::V2d viewWindow;

	// TODO: aspect ratio when fullscreen
	if(width > height){
		viewWindow.x = vw;
		viewWindow.y = vw / (width/height);
	}else{
		viewWindow.x = vw / (height/width);
		viewWindow.y = vw;
	}

	camera->setViewWindow(&viewWindow);
}

void
UpdateSubRasters(rw::Camera *mainCamera, rw::int32 mainWidth, rw::int32 mainHeight)
{
	rw::Rect rect[4];
	float width, height, border;

	border = mainHeight*0.05f;

	width = (mainWidth - border*3.0f) / 2.0f;
	height = (mainHeight - border*3.0f) / 2.0f;

	// top left
	rect[0].x = border;
	rect[0].y = border;
	rect[0].w = width;
	rect[0].h = height;

	// top right
	rect[1].x = border*2 + width;
	rect[1].y = border;
	rect[1].w = width;
	rect[1].h = height;

	// bottom left
	rect[2].x = border;
	rect[2].y = border*2 + height;
	rect[2].w = width;
	rect[2].h = height;

	// bottom left
	rect[3].x = border*2 + width;
	rect[3].y = border*2 + height;
	rect[3].w = width;
	rect[3].h = height;

	CameraSetViewWindow(SubCameras[0], width, height, 0.5f);
	for(int i = 1; i < 4; i++)
		CameraSetViewWindow(SubCameras[i], width, height, 0.5f + 0.4f);

	for(int i = 0; i < 4; i++){
		SubCameras[i]->frameBuffer->subRaster(mainCamera->frameBuffer, &rect[i]);
		SubCameras[i]->zBuffer->subRaster(mainCamera->zBuffer, &rect[i]);
	}
}

void
PositionSubCameras(void)
{
	rw::Frame *frame;
	rw::V3d pos;
	const float dist = 2.5f;

	// perspective
	pos.x = pos.y = 0.0f;
	pos.z = -4.0f;
	frame = SubCameras[0]->getFrame();
	frame->translate(&pos, rw::COMBINEREPLACE);

	// look along z
	pos.x = pos.y = 0.0f;
	pos.z = -dist;
	frame = SubCameras[1]->getFrame();
	frame->translate(&pos, rw::COMBINEREPLACE);

	// look along x
	pos.x = -dist;
	pos.y = pos.z = 0.0f;
	frame = SubCameras[2]->getFrame();
	frame->rotate(&Yaxis, 90.0f, rw::COMBINEREPLACE);
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// look along y
	pos.x = pos.z = 0.0f;
	pos.y = -dist;
	frame = SubCameras[3]->getFrame();
	frame->rotate(&Xaxis, -90.0f, rw::COMBINEREPLACE);
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);
}

void
CreateCameras(rw::World *world)
{
	Camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
	assert(Camera);

	for(int i = 0; i < 4; i++){
		SubCameras[i] = sk::CameraCreate(0, 0, 1);
		assert(SubCameras[i]);

		SubCameras[i]->setNearPlane(0.1f);
		SubCameras[i]->setFarPlane(30.0f);

		world->addCamera(SubCameras[i]);

		if(i > 0)
			SubCameras[i]->setProjection(rw::Camera::PARALLEL);
	}

	PositionSubCameras();
}

void
DestroyCameras(rw::World *world)
{
	if(Camera){
		sk::CameraDestroy(Camera);
		Camera = nil;
	}

	for(int i = 0; i < 4; i++)
		if(SubCameras[i]){
			world->removeCamera(SubCameras[i]);
			sk::CameraDestroy(SubCameras[i]);
			SubCameras[i] = nil;
		}
}
