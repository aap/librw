#include <cstdio>
#include <cstdlib>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"

#define PLUGIN_ID 2

namespace rw {

Light*
Light::create(int32 type)
{
	Light *light = (Light*)malloc(PluginBase::s_size);
	if(light == NULL){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return NULL;
	}
	light->object.object.init(Light::ID, type);
	light->radius = 0.0f;
	light->color.red = 1.0f;
	light->color.green = 1.0f;
	light->color.blue = 1.0f;
	light->color.alpha = 1.0f;
	light->minusCosAngle = 1.0f;
	light->object.object.privateFlags = 1;
	light->object.object.flags = LIGHTATOMICS | LIGHTWORLD;
	light->clump = NULL;
	light->inClump.init();
	light->constructPlugins();
	return light;
}

void
Light::destroy(void)
{
	this->destructPlugins();
	if(this->clump)
		this->inClump.remove();
	free(this);
}

void
Light::setAngle(float32 angle)
{
	this->minusCosAngle = -cos(angle);
}

float32
Light::getAngle(void)
{
	return acos(-this->minusCosAngle);
}

void
Light::setColor(float32 r, float32 g, float32 b)
{
	this->color.red = r;
	this->color.green = g;
	this->color.blue = b;
	this->object.object.privateFlags = r == g && r == b;
}

struct LightChunkData
{
	float32 radius;
	float32 red, green, blue;
	float32 minusCosAngle;
	uint16 flags;
	uint16 type;
};

Light*
Light::streamRead(Stream *stream)
{
	uint32 version;
	LightChunkData buf;

	if(!findChunk(stream, ID_STRUCT, NULL, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return NULL;
	}
	stream->read(&buf, sizeof(LightChunkData));
	Light *light = Light::create(buf.type);
	if(light == NULL)
		return NULL;
	light->radius = buf.radius;
	light->setColor(buf.red, buf.green, buf.blue);
	float32 a = buf.minusCosAngle;
	if(version >= 0x30300)
		light->minusCosAngle = a;
	else
		// tan -> -cos
		light->minusCosAngle = -1.0f/sqrt(a*a+1.0f);
	light->object.object.flags = (uint8)buf.flags;
	light->streamReadPlugins(stream);
	return light;
}

bool
Light::streamWrite(Stream *stream)
{
	LightChunkData buf;
	writeChunkHeader(stream, ID_LIGHT, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, sizeof(LightChunkData));
	buf.radius = this->radius;
	buf.red   = this->color.red;
	buf.green = this->color.green;
	buf.blue  = this->color.blue;
	if(version >= 0x30300)
		buf.minusCosAngle = this->minusCosAngle;
	else
		buf.minusCosAngle = tan(acos(-this->minusCosAngle));
	buf.flags = this->object.object.flags;
	buf.type = this->object.object.subType;
	stream->write(&buf, sizeof(LightChunkData));

	this->streamWritePlugins(stream);
	return true;
}

uint32
Light::streamGetSize(void)
{
	return 12 + sizeof(LightChunkData) + 12 + this->streamGetPluginSize();
}

}
