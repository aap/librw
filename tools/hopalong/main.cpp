#include <rw.h>
#include <skeleton.h>
#include <assert.h>

rw::V3d zero = { 0.0f, 0.0f, 0.0f };
struct SceneGlobals {
	rw::World *world;
	rw::Camera *camera;
} Scene;
rw::EngineOpenParams engineOpenParams;

rw::Texture *ParticleTex;
rw::Raster *ParticleRaster;

static rw::RWDEVICE::Im2DVertex im2dVerts[1024];
static int numImVerts;
static rw::uint16 imIndices[1024*2];
static int numImIndices;
static rw::PrimitiveType imPrim;

void
BeginIm2D(rw::PrimitiveType prim)
{
	numImVerts = 0;
	numImIndices = 0;
	imPrim = prim;
}

void
EndIm2D(void)
{
	rw::im2d::RenderIndexedPrimitive(imPrim, &im2dVerts, numImVerts, &imIndices, numImIndices);
}

void
RenderParticle(float x, float y, float sz, rw::RGBA col)
{
	using namespace rw;
	using namespace RWDEVICE;

	if(numImVerts+4 > nelem(im2dVerts) ||
	   numImIndices+6 > nelem(imIndices)){
		EndIm2D();
		numImVerts = 0;
		numImIndices = 0;
	}

	float recipZ = 1.0f/Scene.camera->nearPlane;
	rw::RWDEVICE::Im2DVertex *verts = &im2dVerts[numImVerts];

	x += sk::globals.width/2;
	y += sk::globals.height/2;

	verts[0].setScreenX(x-sz);
	verts[0].setScreenY(y-sz);
	verts[0].setScreenZ(rw::im2d::GetNearZ());
	verts[0].setCameraZ(Scene.camera->nearPlane);
	verts[0].setRecipCameraZ(recipZ);
	verts[0].setColor(col.red, col.green, col.blue, col.alpha);
	verts[0].setU(0.0f, recipZ);
	verts[0].setV(0.0f, recipZ);

	verts[1].setScreenX(x+sz);
	verts[1].setScreenY(y-sz);
	verts[1].setScreenZ(rw::im2d::GetNearZ());
	verts[1].setCameraZ(Scene.camera->nearPlane);
	verts[1].setRecipCameraZ(recipZ);
	verts[1].setColor(col.red, col.green, col.blue, col.alpha);
	verts[1].setU(1.0f, recipZ);
	verts[1].setV(0.0f, recipZ);

	verts[2].setScreenX(x+sz);
	verts[2].setScreenY(y+sz);
	verts[2].setScreenZ(rw::im2d::GetNearZ());
	verts[2].setCameraZ(Scene.camera->nearPlane);
	verts[2].setRecipCameraZ(recipZ);
	verts[2].setColor(col.red, col.green, col.blue, col.alpha);
	verts[2].setU(1.0f, recipZ);
	verts[2].setV(1.0f, recipZ);

	verts[3].setScreenX(x-sz);
	verts[3].setScreenY(y+sz);
	verts[3].setScreenZ(rw::im2d::GetNearZ());
	verts[3].setCameraZ(Scene.camera->nearPlane);
	verts[3].setRecipCameraZ(recipZ);
	verts[3].setColor(col.red, col.green, col.blue, col.alpha);
	verts[3].setU(0.0f, recipZ);
	verts[3].setV(1.0f, recipZ);

	imIndices[numImIndices+0] = numImVerts+0;
	imIndices[numImIndices+1] = numImVerts+1;
	imIndices[numImIndices+2] = numImVerts+2;
	imIndices[numImIndices+3] = numImVerts+0;
	imIndices[numImIndices+4] = numImVerts+2;
	imIndices[numImIndices+5] = numImVerts+3;

	numImVerts += 4;
	numImIndices += 6;
}

float sgn(float f) { return f < 0.0f ? -1.0f : f > 0.0f ? 1.0f : 0.0f; }

struct Orbit
{
	ImVec4 color;
	float StartX;
	float StartY;
	float PointSz;
	int numIterations;
	bool used;
};

Orbit orbits[100];
int curOrbit = 0;

float A = 0.0f;
float B = 0.0f;
float C = 0.0f;
float D = 0.0f;
float E = 0.0f;
float Exp = 0.25f;
float Scale = 50.0f;
int type;
bool textured = true;
bool animateX;
bool animateY;
bool animateA;
bool animateB;
bool animateC;

void
ResetOrbit(Orbit *o)
{
	o->color = ImVec4(1.0f, 0.0f, 0.0f, 1.00f);
	o->StartX = 1.0f;
	o->StartY = 0.0f;
	o->PointSz = 3.0f;
	o->numIterations = 1000;
	o->used = false;
}

void
RenderOrbit(Orbit *o)
{
	rw::RGBA red = { 0xFF, 0, 0, 0xFF };
	rw::RGBA color = rw::makeRGBA(o->color.x*255, o->color.y*255, o->color.z*255, o->color.w*255);

	float x1, y1;
	float x0 = o->StartX;
	float y0 = o->StartY;
	int n = o->numIterations;

	if(textured){
		rw::SetRenderStatePtr(rw::TEXTURERASTER, ParticleRaster);
		rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::LINEAR);
	}else
		rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
	BeginIm2D(rw::PRIMTYPETRILIST);
	while(n--){
		switch(type){
		case 0:
		default:
			x1 = y0 - sgn(x0)*(D + sqrtf(fabs(B*x0 - C)));
			break;
		case 1:
			x1 = y0 - sgn(x0)*(D + powf(fabs(B*x0 - C), Exp));
			break;
		case 2:
			x1 = y0 - sgn(x0)*(D + logf(2.0f + sqrtf(fabs(B*x0 - C))));
			break;
		}
		y1 = A - x0;
		x1 += E;
		RenderParticle(x1*Scale, y1*Scale, o->PointSz, color);
		x0 = x1;
		y0 = y1;
	}
	EndIm2D();
}

void
RenderHopalong(void)
{
	int i;
	for(i = 0; i < nelem(orbits); i++)
		if(orbits[i].used)
			RenderOrbit(&orbits[i]);
}

void
Init(void)
{
	sk::globals.windowtitle = "Hopalong map";
	sk::globals.width = 1280;
	sk::globals.height = 800;
	sk::globals.quit = 0;
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

#include "particle.inc"

#include "vfs.h"
VFS_file files[] = {
	{ "particle.png", particle_png, particle_png_len }
};
VFS vfs = {
	files, nelem(files)
};

bool
InitRW(void)
{
	if(!sk::InitRW())
		return false;
	installVFS(&vfs);

	Scene.world = rw::World::create();

	ParticleTex = rw::Texture::read("particle", nil);
	if(ParticleTex)
		ParticleRaster = ParticleTex->raster;

	rw::Light *ambient = rw::Light::create(rw::Light::AMBIENT);
	ambient->setColor(0.2f, 0.2f, 0.2f);
	Scene.world->addLight(ambient);

	rw::V3d xaxis = { 1.0f, 0.0f, 0.0f };
	rw::Light *direct = rw::Light::create(rw::Light::DIRECTIONAL);
	direct->setColor(0.8f, 0.8f, 0.8f);
	direct->setFrame(rw::Frame::create());
	direct->getFrame()->rotate(&xaxis, 180.0f, rw::COMBINEREPLACE);
	Scene.world->addLight(direct);

	Scene.camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
	Scene.world->addCamera(Scene.camera);


	for(int i = 0; i < nelem(orbits); i++)
		ResetOrbit(&orbits[i]);
	orbits[0].used = true;

	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();

	return true;
}

void
orbitGUI(Orbit *o)
{
	ImGui::SliderFloat("x0", &o->StartX, -10.0f, 10.0f);
	ImGui::SliderFloat("y0", &o->StartY, -10.0f, 10.0f);
	ImGui::SliderFloat("Point Size", &o->PointSz, 0.5f, 10.0f);
	ImGui::SliderInt("NumPoints", &o->numIterations, 1, 10000);
	ImGui::ColorEdit3("color", (float*)&o->color);

/*
	ImGui::Checkbox("Textured", &textured);
	ImGui::Checkbox("AnimateX", &animateX);
	ImGui::Checkbox("AnimateY", &animateY);
	ImGui::Checkbox("AnimateA", &animateA);
	ImGui::Checkbox("AnimateB", &animateB);
	ImGui::Checkbox("AnimateC", &animateC);
*/
}

void
hopalongGUI(void)
{
	int i;
	static char tmp[128];
	if(curOrbit >= 0)
		orbitGUI(&orbits[curOrbit]);
	ImGui::SliderFloat("A", &A, -10.0f, 10.0f);
	ImGui::SliderFloat("B", &B, -10.0f, 10.0f);
	ImGui::SliderFloat("C", &C, -10.0f, 10.0f);
	ImGui::SliderFloat("D", &D, -10.0f, 10.0f);
	ImGui::SliderFloat("E", &E, -10.0f, 10.0f);
	ImGui::SliderFloat("exp", &Exp, 0.001f, 4.0f);
	ImGui::RadioButton("sqrt", &type, 0);
	ImGui::RadioButton("^exp", &type, 1);
	ImGui::RadioButton("log", &type, 2);
	ImGui::SliderFloat("Scale", &Scale, 1.0f, 100.0f);
	for(i = 0; i < nelem(orbits); i++){
		if(orbits[i].used){
			sprintf(tmp, "Orbit %d\n", i);
			if(ImGui::Button(tmp))
				curOrbit = i;
		}
	}
	if( ImGui::Button("Add Orbit"))
		for(i = 0; i < nelem(orbits); i++)
			if(!orbits[i].used){
				ResetOrbit(&orbits[i]);
				orbits[i].used = true;
				curOrbit = i;
				break;
			}
	if(curOrbit >= 0 && ImGui::Button("Remove Orbit")){
		orbits[curOrbit].used = false;
		curOrbit = -1;
		for(i = 0; i < nelem(orbits); i++)
			if(orbits[i].used){
				curOrbit = i;
				break;
			}
	}
}

#ifdef __unix__
#include <unistd.h>
#endif

void
Draw(float timeDelta)
{
	static bool show_demo_window = false;

	rw::RGBA clearcol = { 0, 0, 0, 255 };
	Scene.camera->clear(&clearcol, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
	Scene.camera->beginUpdate();

	ImGui_ImplRW_NewFrame(timeDelta);

	RenderHopalong();

/*
	if(animateX) StartX += timeDelta/10000.0f;
	if(animateY) StartY += timeDelta/10000.0f;
	if(animateA) A += timeDelta/10000.0f;
	if(animateB) B += timeDelta/10000.0f;
	if(animateC) C += timeDelta/10000.0f;
*/

	hopalongGUI();

	if(show_demo_window){
		ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiCond_FirstUseEver);
		ImGui::ShowDemoWindow(&show_demo_window);
	}


	ImGui::EndFrame();
	ImGui::Render();

	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());

	Scene.camera->endUpdate();
	Scene.camera->showRaster(0);

#ifdef __unix__
	usleep(10000);
#endif
}


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

int MouseX, MouseY;
int MouseDeltaX, MouseDeltaY;
int MouseButtons;

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
	if(MouseButtons&1){
		int mx = MouseX - sk::globals.width/2;
		int my = MouseY - sk::globals.height/2;
		if(curOrbit >= 0){
			orbits[curOrbit].StartX = mx/Scale;
			orbits[curOrbit].StartY = my/Scale;
		}
	}
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
		Init();
		return EVENTPROCESSED;
	case RWINITIALIZE:
		return ::InitRW() ? EVENTPROCESSED : EVENTERROR;
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
		// TODO: set aspect ratio
		if(Scene.camera)
			sk::CameraSize(Scene.camera, r);
		break;
	case IDLE:
		Draw(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
