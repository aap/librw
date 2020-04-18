#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID ID_WORLD

namespace rw {

int32 World::numAllocated = 0;

PluginList World::s_plglist = { sizeof(World), sizeof(World), nil, nil };

World*
World::create(void)
{
	World *world = (World*)rwMalloc(s_plglist.size, MEMDUR_EVENT | ID_WORLD);
	if(world == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	numAllocated++;
	world->object.init(World::ID, 0);
	world->lights.init();
	world->directionalLights.init();
	world->clumps.init();
	s_plglist.construct(world);
	return world;
}

void
World::destroy(void)
{
	s_plglist.destruct(this);
	rwFree(this);
	numAllocated--;
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
	assert(cam->world == nil);
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

void
World::addAtomic(Atomic *atomic)
{
	assert(atomic->world == nil);
	atomic->world = this;
	if(atomic->getFrame())
		atomic->getFrame()->updateObjects();
}

void
World::removeAtomic(Atomic *atomic)
{
	assert(atomic->world == this);
	atomic->world = nil;
}

void
World::addClump(Clump *clump)
{
	assert(clump->world == nil);
	clump->world = this;
	this->clumps.add(&clump->inWorld);
	FORLIST(lnk, clump->atomics)
		this->addAtomic(Atomic::fromClump(lnk));
	FORLIST(lnk, clump->lights)
		this->addLight(Light::fromClump(lnk));
	FORLIST(lnk, clump->cameras)
		this->addCamera(Camera::fromClump(lnk));

	if(clump->getFrame()){
		clump->getFrame()->matrix.optimize();
		clump->getFrame()->updateObjects();
	}
}

void
World::removeClump(Clump *clump)
{
	assert(clump->world == this);
	clump->inWorld.remove();
	FORLIST(lnk, clump->atomics)
		this->removeAtomic(Atomic::fromClump(lnk));
	FORLIST(lnk, clump->lights)
		this->removeLight(Light::fromClump(lnk));
	FORLIST(lnk, clump->cameras)
		this->removeCamera(Camera::fromClump(lnk));
	clump->world = nil;
}

void
World::render(void)
{
	// this is very wrong, we really want world sectors
	FORLIST(lnk, this->clumps)
		Clump::fromWorld(lnk)->render();
}

}
