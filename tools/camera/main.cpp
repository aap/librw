#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "viewer.h"
#include "camexamp.h"

rw::V3d zero = { 0.0f, 0.0f, 0.0f };
rw::EngineOpenParams engineOpenParams;
float FOV = 70.0f;

rw::RGBA ForegroundColor = { 200, 200, 200, 255 };
rw::RGBA BackgroundColor = { 64, 64, 64, 0 };
rw::RGBA BackgroundColorSub = { 74, 74, 74, 0 };

rw::World *World;
rw::Charset *Charset;

rw::V3d Xaxis = { 1.0f, 0.0, 0.0f };
rw::V3d Yaxis = { 0.0f, 1.0, 0.0f };
rw::V3d Zaxis = { 0.0f, 0.0, 1.0f };

float TimeDelta;

rw::Clump *Clump;

rw::World*
CreateWorld(void)
{
	rw::BBox bb;

	bb.inf.x = bb.inf.y = bb.inf.z = -100.0f;
	bb.sup.x = bb.sup.y = bb.sup.z = 100.0f;

	return rw::World::create(&bb);
}

void
LightsCreate(rw::World *world)
{
	rw::Light *light = rw::Light::create(rw::Light::AMBIENT);
	assert(light);
	World->addLight(light);

	light = rw::Light::create(rw::Light::DIRECTIONAL);
	assert(light);
	rw::Frame *frame = rw::Frame::create();
	assert(frame);
	frame->rotate(&Xaxis, 30.0f, rw::COMBINEREPLACE);
	frame->rotate(&Yaxis, 30.0f, rw::COMBINEPOSTCONCAT);
	light->setFrame(frame);
	World->addLight(light);
}

rw::Clump*
ClumpCreate(rw::World *world)
{
	rw::Clump *clump;
	rw::StreamFile in;

	rw::Image::setSearchPath("files/clump/");
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
	rw::V3d pos = { 0.0f, 0.0f, 8.0f };
	frame->translate(&pos, rw::COMBINEREPLACE);
	World->addClump(clump);
	return clump;
}

void
ClumpRotate(rw::Clump *clump, rw::Camera *camera, float xAngle, float yAngle)
{
	rw::Matrix *cameraMatrix = &camera->getFrame()->matrix;
	rw::Frame *clumpFrame = clump->getFrame();
	rw::V3d pos = clumpFrame->matrix.pos;

	pos = rw::scale(pos, -1.0f);
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	clumpFrame->rotate(&cameraMatrix->up, xAngle, rw::COMBINEPOSTCONCAT);
	clumpFrame->rotate(&cameraMatrix->right, yAngle, rw::COMBINEPOSTCONCAT);

	pos = rw::scale(pos, -1.0f);
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);
}

void
ClumpTranslate(rw::Clump *clump, rw::Camera *camera, float xDelta, float yDelta)
{
	rw::Matrix *cameraMatrix = &camera->getFrame()->matrix;
	rw::Frame *clumpFrame = clump->getFrame();

	rw::V3d deltaX = rw::scale(cameraMatrix->right, xDelta);
	rw::V3d deltaZ = rw::scale(cameraMatrix->at, yDelta);
	rw::V3d delta = rw::add(deltaX, deltaZ);

	clumpFrame->translate(&delta, rw::COMBINEPOSTCONCAT);
}

void
ClumpSetPosition(rw::Clump *clump, rw::V3d *position)
{
	clump->getFrame()->translate(position, rw::COMBINEREPLACE);
}

void
Initialize(void)
{
	sk::globals.windowtitle = "Camera example";
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

	CamerasCreate(World);
	LightsCreate(World);

	Clump = ClumpCreate(World);

	rw::SetRenderState(rw::CULLMODE, rw::CULLBACK);
	rw::SetRenderState(rw::ZTESTENABLE, 1);
	rw::SetRenderState(rw::ZWRITEENABLE, 1);

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	rw::Rect r;
	r.x = 0;
	r.y = 0;
	r.w = sk::globals.width;
	r.h = sk::globals.height;
	CameraSizeUpdate(&r, 0.5f, 4.0f/3.0f);

	return true;
}

void
DestroyLight(rw::Light *light, rw::World *world)
{
	world->removeLight(light);
	rw::Frame *frame = light->getFrame();
	if(frame){
		light->setFrame(nil);
		frame->destroy();
	}
	light->destroy();
}

void
Terminate3D(void)
{
	if(Clump){
		World->removeClump(Clump);
		Clump->destroy();
		Clump = nil;
	}

	FORLIST(lnk, World->globalLights){
		rw::Light *light = rw::Light::fromWorld(lnk);
		DestroyLight(light, World);
	}
	FORLIST(lnk, World->localLights){
		rw::Light *light = rw::Light::fromWorld(lnk);
		DestroyLight(light, World);
	}

	CamerasDestroy(World);

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
	char str[256];
	sprintf(str, "View window (%.2f, %.2f)", SubCameraData.viewWindow.x, SubCameraData.viewWindow.y);
	Charset->print(str, 100, 100, 0);
	sprintf(str, "View offset (%.2f, %.2f)", SubCameraData.offset.x, SubCameraData.offset.y);
	Charset->print(str, 100, 120, 0);
}

void
ResetCameraAndClump(void)
{
        SubCameraData.nearClipPlane = 0.3f;
        SubCameraData.farClipPlane = 5.0f;
        SubCameraData.projection = rw::Camera::PERSPECTIVE;
        SubCameraData.offset.x = 0.0f;
        SubCameraData.offset.y = 0.0f;
        SubCameraData.viewWindow.x = 0.5f;
        SubCameraData.viewWindow.y = 0.38f;
	CameraSetData(&SubCameraData, ALL);
	ProjectionIndex = 0;

	rw::V3d position = { 3.0f, 0.0f, 8.0f };
	rw::V3d point = { 0.0f, 0.0f, 8.0f };
	ViewerSetPosition(SubCameraData.camera, &position);
	ViewerRotate(SubCamera, -90.0f, 0.0f);

	ClumpSetPosition(Clump, &point);
}

void
Gui(void)
{
	static bool showCameraWindow = true;
	ImGui::Begin("Camera", &showCameraWindow);

	ImGui::RadioButton("Main camera", &CameraSelected, 0);
	ImGui::RadioButton("Sub camera", &CameraSelected, 1);

	if(ImGui::RadioButton("Perspective", &ProjectionIndex, 0))
		ProjectionCallback();
	if(ImGui::RadioButton("Parallel", &ProjectionIndex, 1))
		ProjectionCallback();

	if(ImGui::SliderFloat("Near clip-plane", &SubCameraData.nearClipPlane, 0.1f, SubCameraData.farClipPlane-0.1f))
		ClipPlaneCallback();
	if(ImGui::SliderFloat("Far clip-plane", &SubCameraData.farClipPlane, SubCameraData.nearClipPlane+0.1f, 20.0f))
		ClipPlaneCallback();

	if(ImGui::Button("Reset"))
		ResetCameraAndClump();

	ImGui::End();
}

void
MainCameraRender(rw::Camera *camera)
{
	RenderTextureCamera(&BackgroundColorSub, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ, World);

	camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	camera->beginUpdate();

	ImGui_ImplRW_NewFrame(TimeDelta);

	World->render();

	DrawCameraViewplaneTexture(&SubCameraData);
	DrawCameraFrustum(&SubCameraData);

	DisplayOnScreenInfo();

	Gui();
	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());

	camera->endUpdate();


	RenderSubCamera(&BackgroundColorSub, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ, World);
}

void
SubCameraRender(rw::Camera *camera)
{
	camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	camera->beginUpdate();

	ImGui_ImplRW_NewFrame(TimeDelta);

	World->render();

	DisplayOnScreenInfo();

	Gui();
	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());

	camera->endUpdate();
}

void
Render(void)
{
	rw::Camera *camera;

	SubCameraMiniViewSelect(CameraSelected == 0);

	switch(CameraSelected){
	default:
	case 0:
		camera = MainCamera;
		MainCameraRender(camera);
		break;
	case 1:
		camera = SubCamera;
		SubCameraRender(camera);
		break;
	}
	camera->showRaster(0);
}

void
Idle(float timeDelta)
{
	TimeDelta = timeDelta;
	Render();
}

int MouseX, MouseY;
int MouseDeltaX, MouseDeltaY;
int MouseButtons;

bool RotateClump;
bool TranslateClump;
bool RotateCamera;
bool TranslateCamera;
bool ViewXWindow;
bool ViewYWindow;
bool ViewXOffset;
bool ViewYOffset;

bool Ctrl, Alt, Shift;

void
KeyUp(int key)
{
	switch(key){
	case sk::KEY_LCTRL:
	case sk::KEY_RCTRL:
		Ctrl = false;
		break;
	case sk::KEY_LALT:
	case sk::KEY_RALT:
		Alt = false;
		break;
	case sk::KEY_LSHIFT:
	case sk::KEY_RSHIFT:
		Shift = false;
		break;
	}
}

void
KeyDown(int key)
{
	switch(key){
	case sk::KEY_LCTRL:
	case sk::KEY_RCTRL:
		Ctrl = true;
		break;
	case sk::KEY_LALT:
	case sk::KEY_RALT:
		Alt = true;
		break;
	case sk::KEY_LSHIFT:
	case sk::KEY_RSHIFT:
		Shift = true;
		break;
	case sk::KEY_ESC:
		sk::globals.quit = 1;
		break;
	}
}

void
MouseBtn(sk::MouseState *mouse)
{
	MouseButtons = mouse->buttons;
	RotateClump = !Ctrl && !Alt && !Shift && !!(MouseButtons&1);
	TranslateClump = !Ctrl && !Alt && !Shift && !!(MouseButtons&4);
	RotateCamera = Ctrl && !!(MouseButtons&1);
	TranslateCamera = Ctrl && !!(MouseButtons&4);
	ViewXWindow = Shift && !!(MouseButtons&1);
	ViewYWindow = Shift && !!(MouseButtons&4);
	ViewXOffset = Alt && !!(MouseButtons&1);
	ViewYOffset = Alt && !!(MouseButtons&4);
}

void
MouseMove(sk::MouseState *mouse)
{
	MouseDeltaX = mouse->posx - MouseX;
	MouseDeltaY = mouse->posy - MouseY;
	MouseX = mouse->posx;
	MouseY = mouse->posy;

	if(RotateClump)
		ClumpRotate(Clump, MainCamera, MouseDeltaX, -MouseDeltaY);
	if(TranslateClump)
		ClumpTranslate(Clump, MainCamera, -MouseDeltaX*0.01f, -MouseDeltaY*0.1f);
	if(RotateCamera)
		ViewerRotate(SubCamera, -MouseDeltaX*0.1f, MouseDeltaY*0.1f);
	if(TranslateCamera)
		ViewerTranslate(SubCamera, -MouseDeltaX*0.01f, -MouseDeltaY*0.01f);
	if(ViewXWindow)
		ChangeViewWindow(-MouseDeltaY*0.01f, 0.0f);
	if(ViewYWindow)
		ChangeViewWindow(0.0f, -MouseDeltaY*0.01f);
	if(ViewXOffset)
		ChangeViewOffset(-MouseDeltaY*0.01f, 0.0f);
	if(ViewYOffset)
		ChangeViewOffset(0.0f, -MouseDeltaY*0.01f);
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

		CameraSizeUpdate(r, 0.5f, 4.0f/3.0f);
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
