#include <math.h>

#include "demoreel.h"

// Additive-blended particle vortex, camera-facing quads via im3d.
// The kind of thing the GS fillrate was made for.

using namespace rw;

#ifdef RW_PS2
#define NUMPARTS 600
#else
#define NUMPARTS 1200
#endif

struct Particle
{
	float angle;
	float radius;
	float height;
	float size;
	float phase;
};

static Particle parts[NUMPARTS];
static RWDEVICE::Im3DVertex partVerts[NUMPARTS*4];
static uint16 partIndices[NUMPARTS*6];
static Texture *glowTex;
static Matrix identMat;

// cheap deterministic pseudo-random, no libc rand()
static float
frand(int n)
{
	n = (n<<13) ^ n;
	n = n*(n*n*15731 + 789221) + 1376312589;
	return ((n & 0x7fffffff) / (float)0x7fffffff);
}

static void
ParticleInit(void)
{
	int i;

	glowTex = MakeGlowTexture();
	identMat.setIdentity();

	for(i = 0; i < NUMPARTS; i++){
		Particle *p = &parts[i];
		float u = (float)i/NUMPARTS;
		p->radius = 4.0f + 30.0f*powf(u, 0.7f);
		p->angle = i*2.3999632f + frand(i)*0.6f;	// golden angle spiral
		p->height = (frand(i*3+1) - 0.5f)*10.0f*expf(-p->radius/18.0f);
		p->size = 0.5f + frand(i*7+2)*1.1f;
		p->phase = frand(i*13+3)*6.28f;
	}

	for(i = 0; i < NUMPARTS; i++){
		partIndices[i*6+0] = i*4+0;
		partIndices[i*6+1] = i*4+1;
		partIndices[i*6+2] = i*4+2;
		partIndices[i*6+3] = i*4+0;
		partIndices[i*6+4] = i*4+2;
		partIndices[i*6+5] = i*4+3;
	}
}

static void
ParticleTerm(void)
{
	if(glowTex){
		glowTex->destroy();
		glowTex = nil;
	}
}

static void
ParticleUpdate(float dt)
{
	int i;

	// differential rotation: inner particles spin faster
	for(i = 0; i < NUMPARTS; i++){
		Particle *p = &parts[i];
		p->angle += dt*16.0f/(p->radius + 4.0f);
	}

	// slow orbiting camera
	float ca = DemoTime*0.12f;
	V3d campos;
	campos.x = 55.0f*cosf(ca);
	campos.y = 55.0f*sinf(ca);
	campos.z = 14.0f + 10.0f*sinf(DemoTime*0.2f);
	V3d origin = { 0.0f, 0.0f, 0.0f };
	LookAt(Scene.camera->getFrame(), campos, origin);

	// billboard the quads using the camera axes
	Matrix *cm = Scene.camera->getFrame()->getLTM();
	V3d right = cm->right;
	V3d up = cm->up;

	for(i = 0; i < NUMPARTS; i++){
		Particle *p = &parts[i];
		V3d pos;
		pos.x = p->radius*cosf(p->angle);
		pos.y = p->radius*sinf(p->angle);
		pos.z = p->height + 0.8f*sinf(DemoTime*2.0f + p->phase);

		float tw = 0.7f + 0.3f*sinf(DemoTime*5.0f + p->phase);
		float sz = p->size*tw;
		RGBA col = HSV(DemoTime*0.02f + p->radius*0.012f, 0.8f, tw, 255);

		RWDEVICE::Im3DVertex *v = &partVerts[i*4];
		V3d r = scale(right, sz);
		V3d u = scale(up, sz);
		V3d pu = add(pos, u);
		V3d pd = sub(pos, u);
		V3d c;

		c = sub(pu, r);
		v[0].setX(c.x); v[0].setY(c.y); v[0].setZ(c.z);
		v[0].setU(0.0f); v[0].setV(0.0f);
		c = add(pu, r);
		v[1].setX(c.x); v[1].setY(c.y); v[1].setZ(c.z);
		v[1].setU(1.0f); v[1].setV(0.0f);
		c = add(pd, r);
		v[2].setX(c.x); v[2].setY(c.y); v[2].setZ(c.z);
		v[2].setU(1.0f); v[2].setV(1.0f);
		c = sub(pd, r);
		v[3].setX(c.x); v[3].setY(c.y); v[3].setZ(c.z);
		v[3].setU(0.0f); v[3].setV(1.0f);

		int j;
		for(j = 0; j < 4; j++)
			v[j].setColor(col.red, col.green, col.blue, col.alpha);
	}
}

static void
ParticleRender(void)
{
	SetRenderState(ZTESTENABLE, 1);
	SetRenderState(ZWRITEENABLE, 0);
	SetRenderState(SRCBLEND, BLENDONE);
	SetRenderState(DESTBLEND, BLENDONE);
	SetRenderState(CULLMODE, CULLNONE);
	SetRenderState(TEXTUREFILTER, Texture::LINEAR);
	SetRenderStatePtr(TEXTURERASTER, glowTex ? glowTex->raster : nil);

	im3d::Transform(partVerts, NUMPARTS*4, &identMat, im3d::VERTEXUV);
	im3d::RenderIndexedPrimitive(PRIMTYPETRILIST, partIndices, NUMPARTS*6);
	im3d::End();

	SetRenderState(ZWRITEENABLE, 1);
	SetRenderState(SRCBLEND, BLENDSRCALPHA);
	SetRenderState(DESTBLEND, BLENDINVSRCALPHA);
}

DemoScene ParticleScene = {
	"Particle vortex",
	ParticleInit,
	ParticleTerm,
	ParticleUpdate,
	ParticleRender
};
