#include <rw.h>
#include <skeleton.h>
#include "camera.h"
#include <assert.h>

rw::V3d zero = { 0.0f, 0.0f, 0.0f };
Camera *camera;
rw::Clump *clump;
rw::World *world;
rw::EngineStartParams engineStartParams;

void
Init(void)
{
	sk::globals.windowtitle = "Clump viewer";
	sk::globals.width = 640;
	sk::globals.height = 480;
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

void
dumpUserData(rw::UserDataArray *ar)
{
	int i;
	printf("name: %s\n", ar->name);
	for(i = 0; i < ar->numElements; i++){
		switch(ar->datatype){
		case rw::USERDATAINT:
			printf("	%d\n", ar->getInt(i));
			break;
		case rw::USERDATAFLOAT:
			printf("	%f\n", ar->getFloat(i));
			break;
		case rw::USERDATASTRING:
			printf("	%s\n", ar->getString(i));
			break;
		}
	}
}

static rw::Frame*
dumpFrameUserDataCB(rw::Frame *f, void*)
{
	using namespace rw;
	int32 i;
	UserDataArray *ar;
	int32 n = UserDataArray::frameGetCount(f);
	for(i = 0; i < n; i++){
		ar = UserDataArray::frameGet(f, i);
		dumpUserData(ar);
	}
	f->forAllChildren(dumpFrameUserDataCB, nil);
	return f;
}

void
dumpUserData(rw::Clump *clump)
{
	printf("Frames\n");
	dumpFrameUserDataCB(clump->getFrame(), nil);
}

static rw::Frame*
getHierCB(rw::Frame *f, void *data)
{
	using namespace rw;
	HAnimData *hd = rw::HAnimData::get(f);
	if(hd->hierarchy){
		*(HAnimHierarchy**)data = hd->hierarchy;
		return nil;
	}
	f->forAllChildren(getHierCB, data);
	return f;
}

rw::HAnimHierarchy*
getHAnimHierarchyFromClump(rw::Clump *clump)
{
	using namespace rw;
	HAnimHierarchy *hier = nil;
	getHierCB(clump->getFrame(), &hier);
	return hier;
}

void
setupAtomic(rw::Atomic *atomic)
{
	using namespace rw;
	// just remove pipelines that we can't handle for now
//	if(atomic->pipeline && atomic->pipeline->platform != rw::platform)
		atomic->pipeline = NULL;

	// Attach hierarchy to atomic if we're skinned
	HAnimHierarchy *hier = getHAnimHierarchyFromClump(atomic->clump);
	if(hier)
		Skin::setHierarchy(atomic, hier);
}

static void
initHierFromFrames(rw::HAnimHierarchy *hier)
{
	using namespace rw;
	int32 i;
	for(i = 0; i < hier->numNodes; i++){
		if(hier->nodeInfo[i].frame){
			hier->matrices[hier->nodeInfo[i].index] = *hier->nodeInfo[i].frame->getLTM();
		}else
			assert(0);
	}
}

void
setupClump(rw::Clump *clump)
{
	using namespace rw;
	HAnimHierarchy *hier = getHAnimHierarchyFromClump(clump);
	if(hier){
		hier->attach();
		initHierFromFrames(hier);
	}

	FORLIST(lnk, clump->atomics){
		rw::Atomic *a = rw::Atomic::fromClump(lnk);
		setupAtomic(a);
	}
}

bool
InitRW(void)
{
//	rw::platform = rw::PLATFORM_D3D8;
	if(!sk::InitRW())
		return false;

	char *filename = "teapot.dff";
	if(sk::args.argc > 1)
		filename = sk::args.argv[1];
	rw::StreamFile in;
	if(in.open(filename, "rb") == NULL){
		printf("couldn't open file\n");
		return false;
	}
	rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL);
	clump = rw::Clump::streamRead(&in);
	assert(clump);
	in.close();

	clump->getFrame()->translate(&zero, rw::COMBINEREPLACE);

	dumpUserData(clump);
	setupClump(clump);

	world = rw::World::create();

	rw::Light *ambient = rw::Light::create(rw::Light::AMBIENT);
	ambient->setColor(0.2f, 0.2f, 0.2f);
	world->addLight(ambient);

	rw::V3d xaxis = { 1.0f, 0.0f, 0.0f };
	rw::Light *direct = rw::Light::create(rw::Light::DIRECTIONAL);
	direct->setColor(0.8f, 0.8f, 0.8f);
	direct->setFrame(rw::Frame::create());
	direct->getFrame()->rotate(&xaxis, 180.0f, rw::COMBINEREPLACE);
	world->addLight(direct);

	camera = new Camera;
	camera->m_rwcam = rw::Camera::create();
	camera->m_rwcam->setFrame(rw::Frame::create());
	camera->m_aspectRatio = 640.0f/480.0f;
	camera->m_near = 0.1f;
	camera->m_far = 450.0f;
	camera->m_target.set(0.0f, 0.0f, 0.0f);
	camera->m_position.set(0.0f, -10.0f, 0.0f);
//	camera->setPosition(Vec3(0.0f, 5.0f, 0.0f));
//	camera->setPosition(Vec3(0.0f, -70.0f, 0.0f));
//	camera->setPosition(Vec3(0.0f, -1.0f, 3.0f));
	camera->update();

	world->addCamera(camera->m_rwcam);

	return true;
}

void
im2dtest(void)
{
	static rw::gl3::Im2DVertex verts[] = {
	  { -0.5, -0.5, 0.0,  255, 0, 0, 255,  0.0, 0.0 },
	  {  0.5, -0.5, 0.0,  0, 255, 0, 255,  0.0, 0.0 },
	  { -0.5,  0.5, 0.0,  0, 0, 255, 255,  0.0, 0.0 },
	  {  0.5,  0.5, 0.0,  0, 255, 255, 255,  0.0, 0.0 },
	};
	static short indices[] = {
		0, 1, 2, 3
	};
	rw::engine->device.im2DRenderIndexedPrimitive(rw::PRIMTYPETRISTRIP,
		&verts, 4, &indices, 4);
}

void
Draw(float timeDelta)
{
	static rw::RGBA clearcol = { 0x80, 0x80, 0x80, 0xFF };
	camera->m_rwcam->clear(&clearcol, rw::Camera::CLEARIMAGE|rw::Camera::CLEARZ);
	camera->update();
	camera->m_rwcam->beginUpdate();

	clump->render();
	im2dtest();

	camera->m_rwcam->endUpdate();
	camera->m_rwcam->showRaster();
}


void
KeyUp(int key)
{
}

void
KeyDown(int key)
{
	switch(key){
	case 'W':
		camera->orbit(0.0f, 0.1f);
		break;
	case 'S':
		camera->orbit(0.0f, -0.1f);
		break;
	case 'A':
		camera->orbit(-0.1f, 0.0f);
		break;
	case 'D':
		camera->orbit(0.1f, 0.0f);
		break;
	case sk::KEY_UP:
		camera->turn(0.0f, 0.1f);
		break;
	case sk::KEY_DOWN:
		camera->turn(0.0f, -0.1f);
		break;
	case sk::KEY_LEFT:
		camera->turn(0.1f, 0.0f);
		break;
	case sk::KEY_RIGHT:
		camera->turn(-0.1f, 0.0f);
		break;
	case 'R':
		camera->zoom(0.1f);
		break;
	case 'F':
		camera->zoom(-0.1f);
		break;
	case sk::KEY_ESC:
		sk::globals.quit = 1;
		break;
	}
}

sk::EventStatus
AppEventHandler(sk::Event e, void *param)
{
	using namespace sk;
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
	case IDLE:
		Draw(*(float*)param);
		return EVENTPROCESSED;
	}
	return sk::EVENTNOTPROCESSED;
}
