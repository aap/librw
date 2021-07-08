#include <rw.h>
#include <skeleton.h>
#include <assert.h>

#include "im3d.h"

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
	Camera->setFarPlane(50.0f);

	return Camera;
}

void
Initialize(void)
{
	sk::globals.windowtitle = "Im3D example";
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

	Im3DInitialize();

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	rw::Rect r;
	r.x = 0;
	r.y = 0;
	r.w = sk::globals.width;
	r.h = sk::globals.height;
	sk::CameraSize(::Camera, &r, 0.5f, 4.0f/3.0f);

	return true;
}

void
Terminate3D(void)
{
	Im3DTerminate();

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

	ImGui::RadioButton("Line-list", &Im3DPrimType, 0);
	ImGui::RadioButton("Line-list indexed", &Im3DPrimType, 1);
	ImGui::RadioButton("Poly-line", &Im3DPrimType, 2);
	ImGui::RadioButton("Poly-line indexed", &Im3DPrimType, 3);
	ImGui::RadioButton("Tri-list", &Im3DPrimType, 4);
	ImGui::RadioButton("Tri-list indexed", &Im3DPrimType, 5);
	ImGui::RadioButton("Tri-strip", &Im3DPrimType, 6);
	ImGui::RadioButton("Tri-strip indexed", &Im3DPrimType, 7);
	ImGui::RadioButton("Tri-fan", &Im3DPrimType, 8);
	ImGui::RadioButton("Tri-fan indexed", &Im3DPrimType, 9);

	ImGui::NewLine();

	ImGui::Checkbox("Textured", &Im3DTextured);
	if(ImGui::Checkbox("Colored", &Im3DColored)){
		LineListSetColor(!Im3DColored);
		IndexedLineListSetColor(!Im3DColored);
		PolyLineSetColor(!Im3DColored);
		IndexedPolyLineSetColor(!Im3DColored);
		TriListSetColor(!Im3DColored);
		IndexedTriListSetColor(!Im3DColored);
		TriStripSetColor(!Im3DColored);
		IndexedTriStripSetColor(!Im3DColored);
		TriFanSetColor(!Im3DColored);
		IndexedTriFanSetColor(!Im3DColored);
	}

	ImGui::End();
}

void
Render(void)
{
	Camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	Camera->beginUpdate();

	ImGui_ImplRW_NewFrame(TimeDelta);

	Im3DRender();
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

bool Rotate, Translate;

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
	Rotate = !!(MouseButtons&1);
	Translate = !!(MouseButtons&4);
}

void
MouseMove(sk::MouseState *mouse)
{
	MouseDeltaX = mouse->posx - MouseX;
	MouseDeltaY = mouse->posy - MouseY;
	MouseX = mouse->posx;
	MouseY = mouse->posy;
	if(Rotate)
		Im3DRotate(MouseDeltaX, -MouseDeltaY);
	if(Translate)
		Im3DTranslateZ(-MouseDeltaY*0.1f);
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
		}
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
