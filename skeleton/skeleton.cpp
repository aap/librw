#include <rw.h>
#include "skeleton.h"


namespace sk {

Globals globals;
Args args;


bool
InitRW(void)
{
	if(!rw::Engine::init())
		return false;
	if(AppEventHandler(sk::PLUGINATTACH, nil) == EVENTERROR)
		return false;
	if(!rw::Engine::open(&engineOpenParams))
		return false;

	SubSystemInfo info;
	int i, n;
	n = Engine::getNumSubSystems();
	for(i = 0; i < n; i++)
		if(Engine::getSubSystemInfo(&info, i))
			printf("subsystem: %s\n", info.name);
	Engine::setSubSystem(n-1);

	int want = -1;
	VideoMode mode;
	n = Engine::getNumVideoModes();
	for(i = 0; i < n; i++)
		if(Engine::getVideoModeInfo(&mode, i)){
//			if(mode.width == 640 && mode.height == 480 && mode.depth == 32)
			if(mode.width == 1920 && mode.height == 1080 && mode.depth == 32)
				want = i;
			printf("mode: %dx%dx%d %d\n", mode.width, mode.height, mode.depth, mode.flags);
		}
//	if(want >= 0) Engine::setVideoMode(want);
	Engine::getVideoModeInfo(&mode, Engine::getCurrentVideoMode());

	if(mode.flags & VIDEOMODEEXCLUSIVE){
		globals.width = mode.width;
		globals.height = mode.height;
	}

	if(!rw::Engine::start())
		return false;

	rw::Charset::open();

	rw::Image::setSearchPath("./");
	return true;
}

void
TerminateRW(void)
{
	rw::Charset::close();

	// TODO: delete all tex dicts
	rw::Engine::stop();
	rw::Engine::close();
	rw::Engine::term();
}

Camera*
CameraCreate(int32 width, int32 height, bool32 z)
{
	Camera *cam;
	cam = Camera::create();
	cam->setFrame(Frame::create());
	cam->frameBuffer = Raster::create(width, height, 0, Raster::CAMERA);
	cam->zBuffer = Raster::create(width, height, 0, Raster::ZBUFFER);
	return cam;
}

void
CameraDestroy(rw::Camera *cam)
{
	if(cam->frameBuffer){
		cam->frameBuffer->destroy();
		cam->frameBuffer = nil;
	}
	if(cam->zBuffer){
		cam->zBuffer->destroy();
		cam->zBuffer = nil;
	}
	rw::Frame *frame = cam->getFrame();
	if(frame){
		cam->setFrame(nil);
		frame->destroy();
	}
	cam->destroy();
}

void
CameraSize(Camera *cam, Rect *r, float viewWindow, float aspectRatio)
{
	if(cam->frameBuffer){
		cam->frameBuffer->destroy();
		cam->frameBuffer = nil;
	}
	if(cam->zBuffer){
		cam->zBuffer->destroy();
		cam->zBuffer = nil;
	}
	cam->frameBuffer = Raster::create(r->w, r->h, 0, Raster::CAMERA);
	cam->zBuffer = Raster::create(r->w, r->h, 0, Raster::ZBUFFER);

	if(viewWindow != 0.0f){
		rw::V2d vw;
		// TODO: aspect ratio when fullscreen
		if(r->w > r->h){
			vw.x = viewWindow;
			vw.y = viewWindow / ((float)r->w/r->h);
		}else{
			vw.x = viewWindow / ((float)r->h/r->w);
			vw.y = viewWindow;
		}		
		cam->setViewWindow(&vw);
	}
}

void
CameraMove(Camera *cam, V3d *delta)
{
	rw::V3d offset;
	rw::V3d::transformVectors(&offset, delta, 1, &cam->getFrame()->matrix);
	cam->getFrame()->translate(&offset);
}

void
CameraPan(Camera *cam, V3d *pos, float angle)
{
	rw::Frame *frame = cam->getFrame();
	rw::V3d trans = pos ? *pos : frame->matrix.pos;
	rw::V3d negTrans = rw::scale(trans, -1.0f);
	frame->translate(&negTrans);
	frame->rotate(&frame->matrix.up, angle);
	frame->translate(&trans);
}

void
CameraTilt(Camera *cam, V3d *pos, float angle)
{
	rw::Frame *frame = cam->getFrame();
	rw::V3d trans = pos ? *pos : frame->matrix.pos;
	rw::V3d negTrans = rw::scale(trans, -1.0f);
	frame->translate(&negTrans);
	frame->rotate(&frame->matrix.right, angle);
	frame->translate(&trans);
}

void
CameraRotate(Camera *cam, V3d *pos, float angle)
{
	rw::Frame *frame = cam->getFrame();
	rw::V3d trans = pos ? *pos : frame->matrix.pos;
	rw::V3d negTrans = rw::scale(trans, -1.0f);
	frame->translate(&negTrans);
	frame->rotate(&frame->matrix.at, angle);
	frame->translate(&negTrans);
}

EventStatus
EventHandler(Event e, void *param)
{
	EventStatus s;
	if (e == INITIALIZE) {
		ImGui::CreateContext();
	}

	s = AppEventHandler(e, param);
	if(e == QUIT){
		globals.quit = 1;
		return EVENTPROCESSED;
	}
	if(s == EVENTNOTPROCESSED)
		switch(e){
		case RWINITIALIZE:
			return InitRW() ? EVENTPROCESSED : EVENTERROR;
		case RWTERMINATE:
			TerminateRW();
			return EVENTPROCESSED;
		default:
			break;
		}
	return s;
}

}
