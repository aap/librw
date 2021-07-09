#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "im2d.h"

rw::EngineOpenParams engineOpenParams;

rw::RGBA ForegroundColor = { 200, 200, 200, 255 };
rw::RGBA BackgroundColor = { 64, 64, 64, 0 };

rw::Camera *Camera;
rw::Charset *Charset;

float TimeDelta;

rw::Camera*
CreateCamera(void)
{
	Camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
	assert(Camera);

	Camera->setNearPlane(0.1f);
	Camera->setFarPlane(10.0f);

	return Camera;
}

void
Initialize(void)
{
	sk::globals.windowtitle = "Im2D example";
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

	Camera = CreateCamera();

	Im2DInitialize(Camera);

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	return true;
}

void
Terminate3D(void)
{
	Im2DTerminate();

	if(Camera){
		Camera->destroy();
		Camera = nil;
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
}

void
Gui(void)
{
	static bool showWindow = true;
	ImGui::Begin("Im2D", &showWindow);

	ImGui::RadioButton("Line-list", &Im2DPrimType, 0);
	ImGui::RadioButton("Line-list indexed", &Im2DPrimType, 1);
	ImGui::RadioButton("Poly-line", &Im2DPrimType, 2);
	ImGui::RadioButton("Poly-line indexed", &Im2DPrimType, 3);
	ImGui::RadioButton("Tri-list", &Im2DPrimType, 4);
	ImGui::RadioButton("Tri-list indexed", &Im2DPrimType, 5);
	ImGui::RadioButton("Tri-strip", &Im2DPrimType, 6);
	ImGui::RadioButton("Tri-strip indexed", &Im2DPrimType, 7);
	ImGui::RadioButton("Tri-fan", &Im2DPrimType, 8);
	ImGui::RadioButton("Tri-fan indexed", &Im2DPrimType, 9);

	ImGui::NewLine();

	ImGui::Checkbox("Textured", &Im2DTextured);
	if(ImGui::Checkbox("Colored", &Im2DColored)){
		LineListSetColor(!Im2DColored);
		IndexedLineListSetColor(!Im2DColored);
		PolyLineSetColor(!Im2DColored);
		IndexedPolyLineSetColor(!Im2DColored);
		TriListSetColor(!Im2DColored);
		IndexedTriListSetColor(!Im2DColored);
		TriStripSetColor(!Im2DColored);
		IndexedTriStripSetColor(!Im2DColored);
		TriFanSetColor(!Im2DColored);
		IndexedTriFanSetColor(!Im2DColored);
	}

	ImGui::End();
}

void
Render(void)
{
	Camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	Camera->beginUpdate();

	ImGui_ImplRW_NewFrame(TimeDelta);

	Im2DRender();
	DisplayOnScreenInfo();

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
	TimeDelta = timeDelta;
	Render();
}

int MouseX, MouseY;
int MouseDeltaX, MouseDeltaY;
int MouseButtons;

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
}

void
MouseMove(sk::MouseState *mouse)
{
	MouseDeltaX = mouse->posx - MouseX;
	MouseDeltaY = mouse->posy - MouseY;
	MouseX = mouse->posx;
	MouseY = mouse->posy;
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
			sk::CameraSize(::Camera, r, 0.5f, 4.0f/3.0f);
			Im2DSize(::Camera, r->w, r->h);
		}
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
