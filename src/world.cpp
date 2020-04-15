#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID ID_WORLD

namespace rw {

PluginList World::s_plglist = { sizeof(World), sizeof(World), nil, nil };

World*
World::create(void)
{
	World *world = (World*)rwMalloc(s_plglist.size, MEMDUR_EVENT | ID_WORLD);
	if(world == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	world->object.init(World::ID, 0);
	world->lights.init();
	world->directionalLights.init();
	s_plglist.construct(world);
	return world;
}

void
World::destroy(void)
{
	s_plglist.destruct(this);
	rwFree(this);
}

void
World::addLight(Light *light)
{
	light->world = this;
	if(light->getType() < Light::POINT){
		this->directionalLights.append(&light->inWorld);
	}else{
		this->lights.append(&light->inWorld);
		if(light->getFrame())
			light->getFrame()->updateObjects();
	}
}

void
World::removeLight(Light *light)
{
	if(light->world == this)
		light->inWorld.remove();
}

void
World::addCamera(Camera *cam)
{
	cam->world = this;
	if(cam->getFrame())
		cam->getFrame()->updateObjects();
}

void
World::removeCamera(Camera *cam)
{
	if(cam->world == this)
		cam->world = nil;
}

}
