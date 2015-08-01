#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwps2.h"

using namespace std;

namespace rw {

Pipeline::Pipeline(uint32 platform)
{
	this->pluginID = 0;
	this->pluginData = 0;
	this->platform = platform;
}

Pipeline::Pipeline(Pipeline *)
{
	assert(0 && "Can't copy pipeline");
}

Pipeline::~Pipeline(void)
{
}

void
Pipeline::dump(void)
{
}

void
Pipeline::instance(Atomic *atomic)
{
	fprintf(stderr, "This pipeline can't instance\n");
}

void
Pipeline::uninstance(Atomic *atomic)
{
	fprintf(stderr, "This pipeline can't uninstance\n");
}

void
Pipeline::render(Atomic *atomic)
{
	fprintf(stderr, "This pipeline can't render\n");
}

}
