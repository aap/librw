#ifndef DEMOREEL_H
#define DEMOREEL_H

#include <rw.h>
#include <skeleton.h>

// A demo scene. Plain function pointers and static data only;
// scene code is meant to stay compilable for PS2 (old gcc, no STL).
struct DemoScene
{
	const char *name;
	void (*init)(void);
	void (*term)(void);
	void (*update)(float dt);
	void (*render)(void);
};

struct SceneGlobals
{
	rw::World *world;
	rw::Camera *camera;
};
extern SceneGlobals Scene;
extern float DemoTime;	// seconds since scene start

// main.cpp
void LookAt(rw::Frame *frame, rw::V3d pos, rw::V3d target);
rw::RGBA HSV(float h, float s, float v, rw::uint8 alpha);

// proctex.cpp
rw::Texture *MakeGlowTexture(void);
rw::Texture *MakeGridTexture(void);
rw::Texture *MakeEnvTexture(void);

extern DemoScene TunnelScene;
extern DemoScene ParticleScene;
extern DemoScene KnotScene;

#endif
