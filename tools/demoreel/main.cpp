#include <assert.h>
#include <math.h>

#include "demoreel.h"

rw::EngineOpenParams engineOpenParams;

rw::RGBA ForegroundColor = { 200, 200, 200, 255 };
rw::RGBA BackgroundColor = { 0, 0, 0, 0 };

SceneGlobals Scene;
float DemoTime;

float TimeDelta;

static DemoScene *scenes[] = {
	&TunnelScene,
	&ParticleScene,
	&KnotScene,
};
static int numScenes = sizeof(scenes)/sizeof(scenes[0]);
static int curScene = -1;
static int nextScene = 0;

void
LookAt(rw::Frame *frame, rw::V3d pos, rw::V3d target)
{
	static rw::V3d worldup = { 0.0f, 0.0f, 1.0f };
	rw::Matrix m;
	rw::V3d at, right, up;

	at = rw::normalize(rw::sub(target, pos));
	right = rw::cross(worldup, at);
	if(rw::length(right) < 0.001f)
		right.x = 1.0f, right.y = right.z = 0.0f;
	right = rw::normalize(right);
	up = rw::cross(at, right);

	m.setIdentity();
	m.right = right;
	m.up = up;
	m.at = at;
	m.pos = pos;
	m.optimize();
	frame->transform(&m, rw::COMBINEREPLACE);
}

rw::RGBA
HSV(float h, float s, float v, rw::uint8 alpha)
{
	rw::RGBA col;
	float r, g, b;
	float f, p, q, t;
	int i;

	h = h - floorf(h);
	h *= 6.0f;
	i = (int)h;
	f = h - i;
	p = v*(1.0f - s);
	q = v*(1.0f - s*f);
	t = v*(1.0f - s*(1.0f - f));
	switch(i){
	default:
	case 0: r = v; g = t; b = p; break;
	case 1: r = q; g = v; b = p; break;
	case 2: r = p; g = v; b = t; break;
	case 3: r = p; g = q; b = v; break;
	case 4: r = t; g = p; b = v; break;
	case 5: r = v; g = p; b = q; break;
	}
	col.red = (rw::uint8)(r*255.0f);
	col.green = (rw::uint8)(g*255.0f);
	col.blue = (rw::uint8)(b*255.0f);
	col.alpha = alpha;
	return col;
}

static void
SwitchScene(int n)
{
	while(n < 0) n += numScenes;
	n %= numScenes;
	if(n == curScene)
		return;
	if(curScene >= 0)
		scenes[curScene]->term();
	curScene = n;
	DemoTime = 0.0f;
	scenes[curScene]->init();
}

rw::World*
CreateWorld(void)
{
	rw::BBox bb;
	bb.inf.x = bb.inf.y = bb.inf.z = -1000.0f;
	bb.sup.x = bb.sup.y = bb.sup.z = 1000.0f;
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
	camera->setFOV(70.0f, (float)sk::globals.width/sk::globals.height);
	world->addCamera(camera);
	return camera;
}

void
Initialize(void)
{
	sk::globals.windowtitle = "librw demo reel";
	sk::globals.width = 1280;
	sk::globals.height = 800;
	sk::globals.quit = 0;
}

bool
Initialize3D(void)
{
	if(!sk::InitRW())
		return false;

	Scene.world = CreateWorld();
	Scene.camera = CreateCamera(Scene.world);

#ifndef RW_PS2
	ImGui_ImplRW_Init();
	ImGui::StyleColorsClassic();
#endif

	SwitchScene(0);

	return true;
}

void
Terminate3D(void)
{
	if(curScene >= 0){
		scenes[curScene]->term();
		curScene = -1;
	}

	if(Scene.camera){
		Scene.world->removeCamera(Scene.camera);
		Scene.camera->destroy();
		Scene.camera = nil;
	}
	if(Scene.world){
		Scene.world->destroy();
		Scene.world = nil;
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

#ifndef RW_PS2
void
Gui(void)
{
	static bool showWindow = true;
	int i;

	ImGui::Begin("Demo reel", &showWindow);
	for(i = 0; i < numScenes; i++)
		if(ImGui::RadioButton(scenes[i]->name, curScene == i))
			nextScene = i;
	ImGui::NewLine();
	ImGui::Text("%.1f fps", 1.0f/TimeDelta);
	ImGui::End();
}
#endif

void
Render(void)
{
	Scene.camera->clear(&BackgroundColor, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);

	Scene.camera->beginUpdate();

#ifndef RW_PS2
	ImGui_ImplRW_NewFrame(TimeDelta);
#endif

	scenes[curScene]->render();

#ifndef RW_PS2
	Gui();
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());
#endif

	Scene.camera->endUpdate();

	Scene.camera->showRaster(0);
}

void
Idle(float timeDelta)
{
	TimeDelta = timeDelta;
	if(TimeDelta <= 0.0f) TimeDelta = 1.0f/60.0f;
	if(TimeDelta > 0.1f) TimeDelta = 0.1f;

	SwitchScene(nextScene);
	nextScene = curScene;

	DemoTime += TimeDelta;
	scenes[curScene]->update(TimeDelta);

	Render();
}

void
KeyDown(int key)
{
	switch(key){
	case sk::KEY_ESC:
		sk::globals.quit = 1;
		break;
	case sk::KEY_LEFT:
		nextScene = curScene-1;
		break;
	case sk::KEY_RIGHT:
	case ' ':
		nextScene = curScene+1;
		break;
	default:
		if(key >= '1' && key < '1'+numScenes)
			nextScene = key-'1';
		break;
	}
}

sk::EventStatus
AppEventHandler(sk::Event e, void *param)
{
	using namespace sk;
	Rect *r;

#ifndef RW_PS2
	ImGuiEventHandler(e, param);
#endif

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
	case RESIZE:
		r = (Rect*)param;
		if(r->w == 0) r->w = 1;
		if(r->h == 0) r->h = 1;
		sk::globals.width = r->w;
		sk::globals.height = r->h;
		if(Scene.camera)
			sk::CameraSize(Scene.camera, r, 0.5f, 4.0f/3.0f);
		break;
	case IDLE:
		Idle(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
