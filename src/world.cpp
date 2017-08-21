#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID 2

namespace rw {

PluginList World::s_plglist = { sizeof(World), sizeof(World), nil, nil };

World*
World::create(void)
{
	World *world = (World*)malloc(s_plglist.size);
	if(world == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	world->object.init(World::ID, 0);
	world->lights.init();
	world->directionalLights.init();
	return world;
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
World::addCamera(Camera *cam)
{
	cam->world = this;
	if(cam->getFrame())
		cam->getFrame()->updateObjects();
}

}
