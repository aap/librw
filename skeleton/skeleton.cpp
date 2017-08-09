#include <rw.h>
#include "skeleton.h"

namespace sk {

Globals globals;

bool
InitRW(void)
{
	if(!rw::Engine::init())
		return false;
	if(AppEventHandler(sk::PLUGINATTACH, nil) == EVENTERROR)
		return false;
	if(!rw::Engine::open())
		return false;
	if(!rw::Engine::start(&engineStartParams))
		return false;
	rw::engine->loadTextures = 1;

	rw::TexDictionary::setCurrent(rw::TexDictionary::create());
	rw::Image::setSearchPath(".");
	return true;
}

void
TerminateRW(void)
{
	// TODO: delete all tex dicts
	rw::Engine::stop();
	rw::Engine::close();
	rw::Engine::term();
}

EventStatus
EventHandler(Event e, void *param)
{
	EventStatus s;
	s = AppEventHandler(e, param);
	if(e == QUIT){
		globals.quit = 1;
		return EVENTPROCESSED;
	}
	if(s == EVENTNOTPROCESSED)
		switch(e){
		case RWINITIALIZE:
			return InitRW() ? EVENTPROCESSED : EVENTERROR;
		case RWTERMINATE:
			TerminateRW();
			return EVENTPROCESSED;
		default:
			break;
		}
	return s;
}

}
