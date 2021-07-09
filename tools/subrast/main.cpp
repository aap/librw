#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "subrast.h"

rw::V3d zero = { 0.0f, 0.0f, 0.0f };
rw::EngineOpenParams engineOpenParams;
float FOV = 70.0f;

rw::RGBA ForegroundColor = { 200, 200, 200, 255 };
rw::RGBA BackgroundColor = { 64, 64, 64, 0 };
rw::RGBA BorderColor = { 128, 128, 128, 0 };

rw::Clump *Clump = nil;
rw::Light *MainLight = nil;
rw::Light *AmbientLight = nil;

rw::World *World;
rw::Charset *Charset;

const char *SubCameraCaption[4] = {
	"Perspective view",
	"Parallel view: Z-axis",
	"Parallel view: X-axis",
	"Parallel view: Y-axis"
};

rw::V3d Xaxis = { 1.0f, 0.0, 0.0f };
rw::V3d Yaxis = { 0.0f, 1.0, 0.0f };
rw::V3d Zaxis = { 0.0f, 0.0, 1.0f };

rw::World*
CreateWorld(void)
{
	rw::BBox bb;

	bb.inf.x = bb.inf.y = bb.inf.z = -100.0f;
	bb.sup.x = bb.sup.y = bb.sup.z = 100.0f;

	return rw::World::create(&bb);
}

rw::Light*
CreateAmbientLight(rw::World *world)
{
	rw::Light *light = rw::Light::create(rw::Light::AMBIENT);
	assert(light);
	World->addLight(light);
	return light;
}

rw::Light*
CreateMainLight(rw::World *world)
{
	rw::Light *light = rw::Light::create(rw::Light::DIRECTIONAL);
	assert(light);
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	frame->rotate(&Xaxis, 30.0f, rw::COMBINEREPLACE);
	frame->rotate(&Yaxis, 30.0f, rw::COMBINEPOSTCONCAT);
	light->setFrame(frame);
	World->addLight(light);
	return light;
}

rw::Clump*
CreateClump(rw::World *world)
{
	rw::Clump *clump;
	rw::StreamFile in;

	rw::Image::setSearchPath("files/textures/");
	const char *filename = "files/clump.dff";
	if(in.open(filename, "rb") == NULL){
		printf("couldn't open file\n");
		return nil;
	}
	if(!rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL))
		return nil;
	clump = rw::Clump::streamRead(&in);
	in.close();
	if(clump == nil)
		return nil;

	rw::Frame *frame = clump->getFrame();
	frame->rotate(&Xaxis, -120.0f, rw::COMBINEREPLACE);
	frame->rotate(&Yaxis, 45.0f, rw::COMBINEPOSTCONCAT);
	World->addClump(clump);
	return clump;
}

void
RotateClump(float xAngle, float yAngle)
{
	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::Frame *frame = Clump->getFrame();
	rw::V3d pos = frame->matrix.pos;

	pos = rw::scale(pos, -1.0f);
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);

	frame->rotate(&cameraMatrix->up, xAngle, rw::COMBINEPOSTCONCAT);
	frame->rotate(&cameraMatrix->right, yAngle, rw::COMBINEPOSTCONCAT);

	pos = rw::scale(pos, -1.0f);
	frame->translate(&pos, rw::COMBINEPOSTCONCAT);
}

void
Initialize(void)
{
	sk::globals.windowtitle = "Sub-raster example";
	sk::globals.width = 1280;
	sk::globals.height = 800;
	sk::globals.quit = 0;
}

bool
Initialize3D(void)
{
	if(!sk::InitRW())
		return false;

	Charset = rw::Charset::create(&ForegroundColor, &BackgroundColor);

	World = CreateWorld();

	AmbientLight = CreateAmbientLight(World);
	MainLight = CreateMainLight(World);
	Clump = CreateClump(World);
	if (Clump == nil)
		return false;

	CreateCameras(World);
	UpdateSubRasters(Camera, sk::globals.width, sk::globals.height);

	rw::SetRenderState(rw::CULLMODE, rw::CULLBACK);
	rw::SetRenderState(rw::ZTESTENABLE, 1);
	rw::SetRenderState(rw::ZWRITEENABLE, 1);

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	return true;
}

void
Terminate3D(void)
{
	DestroyCameras(World);

	if(AmbientLight){
		World->removeLight(AmbientLight);
		AmbientLight->destroy();
		AmbientLight = nil;
	}

	if(MainLight){
		World->removeLight(MainLight);
		rw::Frame *frame = MainLight->getFrame();
		MainLight->setFrame(nil);
		frame->destroy();
		MainLight->destroy();
		MainLight = nil;
	}

	if(Clump){
		World->removeClump(Clump);
		Clump->destroy();
		Clump = nil;
	}

	if(World){
		World->destroy();
		World = nil;
	}

	if(Charset){
		Charset->destroy();
		Charset = nil;
	}

	sk::TerminateRW();
}

bool
attachPlugins(void)
{
	rw::ps2::registerPDSPlugin(40);
	rw::ps2::registerPluginPDSPipes();

	rw::registerMeshPlugin();
	rw::registerNativeDataPlugin();
	rw::registerAtomicRightsPlugin();
	rw::registerMaterialRightsPlugin();
	rw::xbox::registerVertexFormatPlugin();
	rw::registerSkinPlugin();
	rw::registerUserDataPlugin();
	rw::registerHAnimPlugin();
	rw::registerMatFXPlugin();
	rw::registerUVAnimPlugin();
	rw::ps2::registerADCPlugin();
	return true;
}

void
DisplayOnScreenInfo(void)
{
	for(int i = 0; i < 4; i++){
		rw::Raster *scr = SubCameras[i]->frameBuffer;

		rw::int32 scrw = scr->width;
		rw::int32 scrh = scr->height;

		rw::int32 captionWidth = strlen(SubCameraCaption[i])*Charset->desc.width;

		if(captionWidth < scrw && scrh > Charset->desc.height*2){
			rw::int32 x = scr->offsetX + (scrw - captionWidth)/2;
			rw::int32 y = scr->offsetY + Charset->desc.height;
			Charset->print(SubCameraCaption[i], x, y, 0);
		}
	}
}

rw::RGBA BackgroundColors[] = {
	{ 64, 64, 64, 0 },
	{ 128, 0, 0, 0 },
	{ 0, 128, 0, 0 },
	{ 0, 0, 128, 0 },
};
void
Render(float timeDelta)
{
	Camera->clear(&BorderColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	for(int i = 0; i < 4; i++){
		SubCameras[i]->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
//		SubCameras[i]->clear(&BackgroundColors[i], rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
		SubCameras[i]->beginUpdate();
		World->render();
		SubCameras[i]->endUpdate();
	}

	Camera->beginUpdate();
	DisplayOnScreenInfo();
	Camera->endUpdate();

	Camera->showRaster(0);
}

void
Idle(float timeDelta)
{
	Render(timeDelta);
}

int MouseX, MouseY;
int MouseDeltaX, MouseDeltaY;
int MouseButtons;

bool Rotating;

void
KeyUp(int key)
{
}

void
KeyDown(int key)
{
	switch(key){
	case sk::KEY_ESC:
		sk::globals.quit = 1;
		break;
	}
}

void
MouseBtn(sk::MouseState *mouse)
{
	MouseButtons = mouse->buttons;
	Rotating = !!(MouseButtons&1);
}

void
MouseMove(sk::MouseState *mouse)
{
	MouseDeltaX = mouse->posx - MouseX;
	MouseDeltaY = mouse->posy - MouseY;
	MouseX = mouse->posx;
	MouseY = mouse->posy;
	if(Rotating)
		RotateClump(-MouseDeltaX, MouseDeltaY);
}

sk::EventStatus
AppEventHandler(sk::Event e, void *param)
{
	using namespace sk;
	Rect *r;
	MouseState *ms;

	ImGuiEventHandler(e, param);
	ImGuiIO &io = ImGui::GetIO();

	switch(e){
	case INITIALIZE:
		Initialize();
		return EVENTPROCESSED;
	case RWINITIALIZE:
		return Initialize3D() ? EVENTPROCESSED : EVENTERROR;
	case RWTERMINATE:
		Terminate3D();
		return EVENTPROCESSED;
	case PLUGINATTACH:
		return attachPlugins() ? EVENTPROCESSED : EVENTERROR;
	case KEYDOWN:
		KeyDown(*(int*)param);
		return EVENTPROCESSED;
	case KEYUP:
		KeyUp(*(int*)param);
		return EVENTPROCESSED;
	case MOUSEBTN:
		if(!io.WantCaptureMouse){
			ms = (MouseState*)param;
			MouseBtn(ms);
		}else
			MouseButtons = 0;
		return EVENTPROCESSED;
	case MOUSEMOVE:
		MouseMove((MouseState*)param);
		return EVENTPROCESSED;
	case RESIZE:
		r = (Rect*)param;
		// TODO: register when we're minimized
		if(r->w == 0) r->w = 1;
		if(r->h == 0) r->h = 1;

		sk::globals.width = r->w;
		sk::globals.height = r->h;
		if(::Camera){
			sk::CameraSize(::Camera, r);
			::Camera->setFOV(FOV, (float)sk::globals.width/sk::globals.height);

			UpdateSubRasters(::Camera, r->w, r->h);
		}
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
