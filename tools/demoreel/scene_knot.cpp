#include <math.h>
#include <assert.h>

#include "demoreel.h"

// Retained mode: a procedural torus knot with env-mapped chrome,
// lit by two colored directional lights. Exercises Geometry
// creation, materials, MatFX and the world/light path.

using namespace rw;

#define KNOT_P 2
#define KNOT_Q 3

#define SEGS 160
#define SIDES 12
#define NUMVERTS ((SEGS+1)*(SIDES+1))
#define NUMTRIS (SEGS*SIDES*2)

static Atomic *knotAtomic;
static Frame *knotFrame;
static Light *ambient;
static Light *keyLight;
static Light *fillLight;
static Texture *envTex;
static Texture *gridTex;

static V3d
knotCenter(float t)
{
	V3d p;
	float r = 6.0f + 2.5f*cosf(KNOT_Q*t);
	p.x = r*cosf(KNOT_P*t);
	p.y = r*sinf(KNOT_P*t);
	p.z = 2.5f*sinf(KNOT_Q*t);
	return p;
}

static Geometry*
CreateKnotGeometry(void)
{
	Geometry *geo = Geometry::create(NUMVERTS, NUMTRIS,
		Geometry::POSITIONS | Geometry::NORMALS |
		Geometry::LIGHT | Geometry::TEXTURED);
	assert(geo);

	MorphTarget *mt = &geo->morphTargets[0];
	V3d *verts = mt->vertices;
	V3d *norms = mt->normals;
	TexCoords *uv = geo->texCoords[0];

	float tuberad = 1.1f;
	int i, j, v;

	v = 0;
	for(i = 0; i <= SEGS; i++){
		float t = (float)i/SEGS*2.0f*3.14159265f;
		V3d c = knotCenter(t);
		V3d tan = normalize(sub(knotCenter(t+0.01f), knotCenter(t-0.01f)));
		static V3d worldup = { 0.0f, 0.0f, 1.0f };
		V3d n = normalize(cross(worldup, tan));
		V3d b = cross(tan, n);

		for(j = 0; j <= SIDES; j++){
			float a = (float)j/SIDES*2.0f*3.14159265f;
			V3d ring = add(scale(n, cosf(a)), scale(b, sinf(a)));
			verts[v] = add(c, scale(ring, tuberad));
			norms[v] = ring;
			uv[v].u = (float)i/SEGS*24.0f;
			uv[v].v = (float)j/SIDES;
			v++;
		}
	}

	Triangle *tri = geo->triangles;
	for(i = 0; i < SEGS; i++)
		for(j = 0; j < SIDES; j++){
			int r0 = i*(SIDES+1) + j;
			int r1 = (i+1)*(SIDES+1) + j;
			tri->v[0] = r0;   tri->v[1] = r0+1; tri->v[2] = r1;
			tri->matId = 0;
			tri++;
			tri->v[0] = r0+1; tri->v[1] = r1+1; tri->v[2] = r1;
			tri->matId = 0;
			tri++;
		}

	Material *mat = Material::create();
	gridTex = MakeGridTexture();
	if(gridTex)
		mat->setTexture(gridTex);
	envTex = MakeEnvTexture();
	MatFX::setEffects(mat, MatFX::ENVMAP);
	MatFX *mfx = MatFX::get(mat);
	if(envTex)
		mfx->setEnvTexture(envTex);
	mfx->setEnvFrame(Scene.camera->getFrame());
	mfx->setEnvCoefficient(0.6f);
	geo->matList.appendMaterial(mat);
	mat->destroy();	// list holds a ref now

	geo->calculateBoundingSphere();
	// strips are the well-trodden path on PS2 (and a test for the tristripper)
	geo->flags |= Geometry::TRISTRIP;
	geo->buildMeshes();
	return geo;
}

static Light*
MakeDirLight(float r, float g, float b, float yaw, float pitch)
{
	static V3d Xaxis = { 1.0f, 0.0f, 0.0f };
	static V3d Zaxis = { 0.0f, 0.0f, 1.0f };
	Light *light = Light::create(Light::DIRECTIONAL);
	light->setColor(r, g, b);
	Frame *f = Frame::create();
	f->rotate(&Xaxis, pitch, COMBINEREPLACE);
	f->rotate(&Zaxis, yaw, COMBINEPOSTCONCAT);
	light->setFrame(f);
	Scene.world->addLight(light);
	return light;
}

static void
KnotInit(void)
{
	Geometry *geo = CreateKnotGeometry();

	knotFrame = Frame::create();
	knotAtomic = Atomic::create();
	knotAtomic->setGeometry(geo, 0);
	geo->destroy();	// atomic holds a ref now
	knotAtomic->setFrame(knotFrame);
	MatFX::enableEffects(knotAtomic);
	Scene.world->addAtomic(knotAtomic);

	ambient = Light::create(Light::AMBIENT);
	ambient->setColor(0.15f, 0.15f, 0.2f);
	Scene.world->addLight(ambient);
	keyLight = MakeDirLight(1.0f, 0.85f, 0.6f, 30.0f, 120.0f);
	fillLight = MakeDirLight(0.3f, 0.4f, 0.9f, 200.0f, 60.0f);
}

static void
KnotTerm(void)
{
	if(knotAtomic){
		Scene.world->removeAtomic(knotAtomic);
		knotAtomic->destroy();
		knotAtomic = nil;
	}
	if(knotFrame){
		knotFrame->destroy();
		knotFrame = nil;
	}
	if(ambient){
		Scene.world->removeLight(ambient);
		ambient->destroy();
		ambient = nil;
	}
	if(keyLight){
		Scene.world->removeLight(keyLight);
		Frame *f = keyLight->getFrame();
		keyLight->setFrame(nil);
		f->destroy();
		keyLight->destroy();
		keyLight = nil;
	}
	if(fillLight){
		Scene.world->removeLight(fillLight);
		Frame *f = fillLight->getFrame();
		fillLight->setFrame(nil);
		f->destroy();
		fillLight->destroy();
		fillLight = nil;
	}
	if(envTex){
		envTex->destroy();
		envTex = nil;
	}
	if(gridTex){
		gridTex->destroy();
		gridTex = nil;
	}
}

static void
KnotUpdate(float dt)
{
	static V3d Xaxis = { 1.0f, 0.0f, 0.0f };
	static V3d Zaxis = { 0.0f, 0.0f, 1.0f };

	knotFrame->rotate(&Zaxis, dt*20.0f, COMBINEPOSTCONCAT);
	knotFrame->rotate(&Xaxis, dt*8.0f, COMBINEPRECONCAT);

	float ca = DemoTime*0.1f;
	V3d campos;
	campos.x = 24.0f*cosf(ca);
	campos.y = 24.0f*sinf(ca);
	campos.z = 8.0f*sinf(DemoTime*0.23f);
	V3d origin = { 0.0f, 0.0f, 0.0f };
	LookAt(Scene.camera->getFrame(), campos, origin);
}

static void
KnotRender(void)
{
	SetRenderState(ZTESTENABLE, 1);
	SetRenderState(ZWRITEENABLE, 1);
	SetRenderState(CULLMODE, CULLBACK);

	knotAtomic->render();
}

DemoScene KnotScene = {
	"Torus knot",
	KnotInit,
	KnotTerm,
	KnotUpdate,
	KnotRender
};
