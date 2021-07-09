#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "main.h"
#include "lights.h"

rw::V3d zero = { 0.0f, 0.0f, 0.0f };
rw::EngineOpenParams engineOpenParams;
float FOV = 70.0f;

rw::RGBA ForegroundColor = { 200, 200, 200, 255 };
rw::RGBA BackgroundColor = { 64, 64, 64, 0 };

rw::World *World;
rw::Camera *Camera;

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

rw::Camera*
CreateCamera(rw::World *world)
{
	rw::Camera *camera;
	camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
	assert(camera);
	camera->setNearPlane(0.1f);
	camera->setFarPlane(300.0f);
	camera->setFOV(FOV, (float)sk::globals.width/sk::globals.height);
	world->addCamera(camera);
	return camera;
}

bool
CreateTestScene(rw::World *world)
{
	rw::Clump *clump;
	rw::StreamFile in;
	const char *filename = "checker.dff";
	if(in.open(filename, "rb") == NULL){
		printf("couldn't open file\n");
		return false;
	}
	if(!rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL))
		return false;
	clump = rw::Clump::streamRead(&in);
	in.close();
	if(clump == nil)
		return false;

	rw::Clump *clone;
	rw::Frame *clumpFrame;
	rw::V3d pos;
	float zOffset = 75.0f;

	// Bottom panel
	clumpFrame = clump->getFrame();
	clumpFrame->rotate(&Xaxis, 90.0f, rw::COMBINEREPLACE);

	pos.x = 0.0f;
	pos.y = -25.0f;
	pos.z = zOffset;
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// only need to add once
	world->addClump(clump);

	// Top panel
	clone = clump->clone();
	clumpFrame = clone->getFrame();
	clumpFrame->rotate(&Xaxis, -90.0f, rw::COMBINEREPLACE);

	pos.x = 0.0f;
	pos.y = 25.0f;
	pos.z = zOffset;
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// Left panel
	clone = clump->clone();
	clumpFrame = clone->getFrame();
	clumpFrame->rotate(&Xaxis, 0.0f, rw::COMBINEREPLACE);
	clumpFrame->rotate(&Yaxis, 90.0f, rw::COMBINEPOSTCONCAT);

	pos.x = 25.0f;
	pos.y = 0.0f;
	pos.z = zOffset;
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// Right panel
	clone = clump->clone();
	clumpFrame = clone->getFrame();
	clumpFrame->rotate(&Xaxis, 0.0f, rw::COMBINEREPLACE);
	clumpFrame->rotate(&Yaxis, -90.0f, rw::COMBINEPOSTCONCAT);

	pos.x = -25.0f;
	pos.y = 0.0f;
	pos.z = zOffset;
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// Back panel
	clone = clump->clone();
	clumpFrame = clone->getFrame();
	clumpFrame->rotate(&Xaxis, 0.0f, rw::COMBINEREPLACE);

	pos.x = 0.0f;
	pos.y = 0.0f;
	pos.z = zOffset + 25.0f;
	clumpFrame->translate(&pos, rw::COMBINEPOSTCONCAT);

	// BBox
	pos.x = 25.0f;
	pos.y = 25.0f;
	pos.z = zOffset - 25.0f;
	RoomBBox.initialize(&pos);
	pos.x = -25.0f;
	pos.y = -25.0f;
	pos.z = zOffset + 25.0f;
	RoomBBox.addPoint(&pos);

	return 1;
}

void
Initialize(void)
{
	sk::globals.windowtitle = "Lights example";
	sk::globals.width = 1280;
	sk::globals.height = 800;
	sk::globals.quit = 0;
}

bool
Initialize3D(void)
{
	if(!sk::InitRW())
		return false;

	World = CreateWorld();

	BaseAmbientLight = CreateBaseAmbientLight();
	AmbientLight = CreateAmbientLight();
	DirectLight = CreateDirectLight();
	PointLight = CreatePointLight();
	SpotLight = CreateSpotLight();
	SpotSoftLight = CreateSpotSoftLight();

	Camera = CreateCamera(World);

	CreateTestScene(World);

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	return true;
}

void
Terminate3D(void)
{
	FORLIST(lnk, World->clumps){
		rw::Clump *clump = rw::Clump::fromWorld(lnk);
		World->removeClump(clump);
		clump->destroy();
	}

	if(Camera){
		World->removeCamera(Camera);
		sk::CameraDestroy(Camera);
		Camera = nil;
	}

	LightsDestroy();

	if(World){
		World->destroy();
		World = nil;
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
Gui(void)
{
//	ImGui::ShowDemoWindow(nil);

	static bool showLightWindow = true;
	ImGui::Begin("Lights", &showLightWindow);

	ImGui::Checkbox("Light", &LightOn);
	ImGui::Checkbox("Draw Light", &LightDrawOn);
	if(ImGui::Checkbox("Base Ambient", &BaseAmbientLightOn)){
		if(BaseAmbientLightOn){
			if(BaseAmbientLight->world == nil)
				World->addLight(BaseAmbientLight);
		}else{
			if(BaseAmbientLight->world)
				World->removeLight(BaseAmbientLight);
		}
	}

	ImGui::RadioButton("Ambient Light", &LightTypeIndex, 0);
	ImGui::RadioButton("Point Light", &LightTypeIndex, 1);
	ImGui::RadioButton("Directional Light", &LightTypeIndex, 2);
	ImGui::RadioButton("Spot Light", &LightTypeIndex, 3);
	ImGui::RadioButton("Soft Spot Light", &LightTypeIndex, 4);

	ImGui::ColorEdit3("Color", (float*)&LightColor);
	float radAngle = LightConeAngle/180.0f*M_PI;
	ImGui::SliderAngle("Cone angle", &radAngle, 0.0f, 180.0f);
	LightConeAngle = radAngle/M_PI*180.0f;
	ImGui::SliderFloat("Radius", &LightRadius, 0.1f, 500.0f);

	ImGui::End();
}

void
Render(float timeDelta)
{
	Camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
	Camera->beginUpdate();

	ImGui_ImplRW_NewFrame(timeDelta);

	World->render();

	if(LightDrawOn && CurrentLight)
		DrawCurrentLight();

	Gui();

	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());
	
	Camera->endUpdate();
	Camera->showRaster(0);
}

void
Idle(float timeDelta)
{
	LightsUpdate();

	Render(timeDelta);
}

int MouseX, MouseY;
int MouseDeltaX, MouseDeltaY;
int MouseButtons;

int CtrlDown;

bool RotateLight;
bool TranslateLightXY;
bool TranslateLightZ;

void
KeyUp(int key)
{
	switch(key){
	case sk::KEY_LCTRL:
	case sk::KEY_RCTRL:
		CtrlDown = 0;
		break;
	}
}

void
KeyDown(int key)
{
	switch(key){
	case sk::KEY_LCTRL:
	case sk::KEY_RCTRL:
		CtrlDown = 1;
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
	RotateLight = !CtrlDown && !!(MouseButtons&1);
	TranslateLightXY = CtrlDown && !!(MouseButtons&1);
	TranslateLightZ = CtrlDown && !!(MouseButtons&4);
}

void
MouseMove(sk::MouseState *mouse)
{
	MouseDeltaX = mouse->posx - MouseX;
	MouseDeltaY = mouse->posy - MouseY;
	MouseX = mouse->posx;
	MouseY = mouse->posy;

	if(RotateLight)
		LightRotate(-MouseDeltaX, MouseDeltaY);
	if(TranslateLightXY)
		LightTranslateXY(-MouseDeltaX*0.1f, -MouseDeltaY*0.1f);
	if(TranslateLightZ)
		LightTranslateZ(-MouseDeltaY*0.1f);
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
		}
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
