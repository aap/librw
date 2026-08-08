#include <math.h>

#include "demoreel.h"

// Classic endless tunnel: fly along a wobbling closed loop,
// vertices regenerated on the CPU every frame, im3d rendering.

using namespace rw;

#define SIDES 16
#define RINGS 48
#define NUMVERTS ((SIDES+1)*(RINGS+1))
#define NUMINDICES (SIDES*RINGS*6)

#define LOOPRADIUS 60.0f

static RWDEVICE::Im3DVertex tunnelVerts[NUMVERTS];
static uint16 tunnelIndices[NUMINDICES];
static Texture *gridTex;
static Matrix identMat;
static float pathPos;

// closed loop in space with some vertical wobble
static V3d
tunnelPath(float s)
{
	V3d p;
	p.x = LOOPRADIUS*cosf(s) + 8.0f*cosf(3.0f*s);
	p.y = LOOPRADIUS*sinf(s) + 8.0f*sinf(2.0f*s);
	p.z = 14.0f*sinf(2.0f*s) + 5.0f*sinf(5.0f*s);
	return p;
}

static float
tunnelRadius(float s)
{
	return 6.0f + 1.5f*sinf(5.0f*s + DemoTime*2.0f);
}

static void
pathFrame(float s, V3d *pos, V3d *t, V3d *n, V3d *b)
{
	static V3d worldup = { 0.0f, 0.0f, 1.0f };
	*pos = tunnelPath(s);
	*t = normalize(sub(tunnelPath(s+0.01f), *pos));
	*n = normalize(cross(worldup, *t));
	*b = cross(*t, *n);
}

static void
TunnelInit(void)
{
	int i, j, v;

	gridTex = MakeGridTexture();
	identMat.setIdentity();
	pathPos = 0.0f;

	v = 0;
	for(i = 0; i < RINGS; i++)
		for(j = 0; j < SIDES; j++){
			int r0 = i*(SIDES+1) + j;
			int r1 = (i+1)*(SIDES+1) + j;
			tunnelIndices[v++] = r0;
			tunnelIndices[v++] = r1;
			tunnelIndices[v++] = r0+1;
			tunnelIndices[v++] = r0+1;
			tunnelIndices[v++] = r1;
			tunnelIndices[v++] = r1+1;
		}
}

static void
TunnelTerm(void)
{
	if(gridTex){
		gridTex->destroy();
		gridTex = nil;
	}
}

static void
TunnelUpdate(float dt)
{
	pathPos += dt*0.30f;

	// camera rides the tunnel, slightly off center
	V3d pos, t, n, b;
	pathFrame(pathPos, &pos, &t, &n, &b);
	V3d target = tunnelPath(pathPos + 0.15f);
	pos = add(pos, scale(b, -1.5f*sinf(DemoTime*0.7f)));
	pos = add(pos, scale(n, 1.5f*cosf(DemoTime*0.5f)));
	LookAt(Scene.camera->getFrame(), pos, target);

	// rebuild the tube
	float ds = 0.028f;
	float hue = DemoTime*0.03f;
	// V texcoords must stay small: the GS wraps texel
	// coordinates beyond ~1024, so shift out the integer part
	float vscale = 4.0f;
	float vbase = floorf(pathPos*vscale);
	int i, j, v;

	v = 0;
	for(i = 0; i <= RINGS; i++){
		float s = pathPos - ds + i*ds;
		V3d c, tt, nn, bb;
		pathFrame(s, &c, &tt, &nn, &bb);
		float rad = tunnelRadius(s);

		// brightness: fade out towards the far end, pulses running along
		float fade = 1.0f - (float)i/RINGS;
		fade = fade*fade;
		float pulse = 0.55f + 0.45f*sinf(s*18.0f - DemoTime*6.0f);
		float bright = fade*(0.35f + 0.65f*pulse);
		RGBA col = HSV(hue + s*0.02f, 0.55f, bright, 255);

		float twist = DemoTime*0.4f;
		for(j = 0; j <= SIDES; j++){
			float a = (float)j/SIDES*2.0f*3.14159265f + twist;
			V3d p = add(c, add(scale(nn, rad*cosf(a)), scale(bb, rad*sinf(a))));
			RWDEVICE::Im3DVertex *vert = &tunnelVerts[v++];
			vert->setX(p.x);
			vert->setY(p.y);
			vert->setZ(p.z);
			vert->setU((float)j/SIDES*4.0f);
			vert->setV(s*vscale - vbase);
			vert->setColor(col.red, col.green, col.blue, col.alpha);
		}
	}
}

static void
TunnelRender(void)
{
	SetRenderState(ZTESTENABLE, 1);
	SetRenderState(ZWRITEENABLE, 1);
	SetRenderState(SRCBLEND, BLENDSRCALPHA);
	SetRenderState(DESTBLEND, BLENDINVSRCALPHA);
	SetRenderState(CULLMODE, CULLNONE);
	SetRenderState(TEXTUREFILTER, Texture::LINEAR);
	SetRenderStatePtr(TEXTURERASTER, gridTex ? gridTex->raster : nil);

	im3d::Transform(tunnelVerts, NUMVERTS, &identMat, im3d::VERTEXUV|im3d::ALLOPAQUE);
	im3d::RenderIndexedPrimitive(PRIMTYPETRILIST, tunnelIndices, NUMINDICES);
	im3d::End();
}

DemoScene TunnelScene = {
	"Tunnel",
	TunnelInit,
	TunnelTerm,
	TunnelUpdate,
	TunnelRender
};
