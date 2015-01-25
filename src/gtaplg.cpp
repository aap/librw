#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"
#include "gtaplg.h"

using namespace std;

namespace gta {

//
// Frame
//

// Node Name

int32 nodeNameOffset;

static void*
createNodeName(void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	name[0] = '\0';
	return object;
}

static void*
copyNodeName(void *dst, void *src, int32 offset, int32)
{
	char *dstname = PLUGINOFFSET(char, dst, offset);
	char *srcname = PLUGINOFFSET(char, src, offset);
	strncpy(dstname, srcname, 23);
	return dst;
}

static void*
destroyNodeName(void *object, int32, int32)
{
	return object;
}

static void
readNodeName(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream->read(name, len);
	name[len] = '\0';
}

static void
writeNodeName(Stream *stream, int32 len, void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	stream->write(name, len);
}

static int32
getSizeNodeName(void *object, int32 offset, int32)
{
	char *name = PLUGINOFFSET(char, object, offset);
	int32 len = strlen(name);
	return len > 0 ? len : -1;
}


void
registerNodeNamePlugin(void)
{
	nodeNameOffset = Frame::registerPlugin(24, ID_NODENAME,
	                                       createNodeName,
	                                       destroyNodeName,
	                                       copyNodeName);
	Frame::registerPluginStream(ID_NODENAME,
	                            readNodeName,
	                            writeNodeName,
	                            getSizeNodeName);
}

//
// Geometry
//

// Breakable Model

int32 breakableOffset;

static void*
createBreakableModel(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint8*, object, offset) = 0;
	return object;
}

static void*
destroyBreakableModel(void *object, int32 offset, int32)
{
	uint8 *p = *PLUGINOFFSET(uint8*, object, offset);
	delete[] p;
	return object;
}

static void
readBreakableModel(Stream *stream, int32, void *object, int32 o, int32)
{
	uint32 header[13];
	uint32 hasBreakable = stream->readU32();
	if(hasBreakable == 0)
		return;
	stream->read(header, 13*4);
	uint32 size = header[1]*(12+8+4) + header[5]*(6+2) +
	              header[8]*(32+32+12);
	uint8 *p = new uint8[sizeof(Breakable)+size];
	Breakable *breakable = (Breakable*)p;
	*PLUGINOFFSET(Breakable*, object, o) = breakable;
	breakable->position     = header[0];
	breakable->numVertices  = header[1];
	breakable->numFaces     = header[5];
	breakable->numMaterials = header[8];
	p += sizeof(Breakable);
	stream->read(p, size);
	breakable->vertices = (float*)p;
	p += breakable->numVertices*12;
	breakable->texCoords = (float*)p;
	p += breakable->numVertices*8;
	breakable->colors = (uint8*)p;
	p += breakable->numVertices*4;
	breakable->faces = (uint16*)p;
	p += breakable->numFaces*6;
	breakable->matIDs = (uint16*)p;
	p += breakable->numFaces*2;
	breakable->texNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->maskNames = (char(*)[32])p;
	p += breakable->numMaterials*32;
	breakable->surfaceProps = (float32(*)[3])p;
}

static void
writeBreakableModel(Stream *stream, int32, void *object, int32 o, int32)
{
	uint32 header[13];
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, o);
	uint8 *p = (uint8*)breakable;
	if(breakable == NULL){
		stream->writeU32(0);
		return;
	}
	stream->writeU32(1);
	memset((char*)header, 0, 13*4);
	header[0] = breakable->position;
	header[1] = breakable->numVertices;
	header[5] = breakable->numFaces;
	header[8] = breakable->numMaterials;
	stream->write(header, 13*4);
	p += sizeof(Breakable);
	stream->write(p, breakable->numVertices*(12+8+4) +
	                       breakable->numFaces*(6+2) +
	                       breakable->numMaterials*(32+32+12));
}

static int32
getSizeBreakableModel(void *object, int32 offset, int32)
{
	Breakable *breakable = *PLUGINOFFSET(Breakable*, object, offset);
	if(breakable == NULL)
		return 4;
	return 56 + breakable->numVertices*(12+8+4) +
	            breakable->numFaces*(6+2) +
	            breakable->numMaterials*(32+32+12);
}

void
registerBreakableModelPlugin(void)
{
	breakableOffset = Geometry::registerPlugin(sizeof(Breakable*),
	                                           ID_BREAKABLE,
	                                           createBreakableModel,
	                                           destroyBreakableModel, NULL);
	Geometry::registerPluginStream(ID_BREAKABLE,
	                               readBreakableModel,
	                               writeBreakableModel,
	                               getSizeBreakableModel);
}

// Extra colors

int32 extraVertColorOffset;

static void*
createExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	colordata->nightColors = NULL;
	colordata->dayColors = NULL;
	colordata->balance = 0.0f;
	return object;
}

static void*
destroyExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	delete[] colordata->nightColors;
	delete[] colordata->dayColors;
	return object;
}

static void
readExtraVertColors(Stream *stream, int32, void *object, int32 offset, int32)
{
	uint32 hasData;
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	hasData = stream->readU32();
	if(!hasData)
		return;
	Geometry *geometry = (Geometry*)object;
	colordata->nightColors = new uint8[geometry->numVertices*4];
	colordata->dayColors = new uint8[geometry->numVertices*4];
	colordata->balance = 1.0f;
	stream->read(colordata->nightColors, geometry->numVertices*4);
	if(geometry->colors)
		memcpy(colordata->dayColors, geometry->colors,
		       geometry->numVertices*4);
}

static void
writeExtraVertColors(Stream *stream, int32, void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	stream->writeU32(colordata->nightColors != NULL);
	if(colordata->nightColors){
		Geometry *geometry = (Geometry*)object;
		stream->write(colordata->nightColors, geometry->numVertices*4);
	}
}

static int32
getSizeExtraVertColors(void *object, int32 offset, int32)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, object, offset);
	Geometry *geometry = (Geometry*)object;
	if(colordata->nightColors)
		return 4 + geometry->numVertices*4;
	return -1;
}

void
registerExtraVertColorPlugin(void)
{
	extraVertColorOffset = Geometry::registerPlugin(sizeof(ExtraVertColors),
	                                                ID_EXTRAVERTCOLORS,
	                                                createExtraVertColors,
	                                                destroyExtraVertColors,
	                                                NULL);
	Geometry::registerPluginStream(ID_EXTRAVERTCOLORS,
	                               readExtraVertColors,
	                               writeExtraVertColors,
	                               getSizeExtraVertColors);
}

// Environment mat

int32 envMatOffset;

static void*
createEnvMat(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(EnvMat*, object, offset) = NULL;
	return object;
}

static void*
destroyEnvMat(void *object, int32 offset, int32)
{
	EnvMat **envmat = PLUGINOFFSET(EnvMat*, object, offset);
	delete *envmat;
	*envmat = NULL;
	return object;
}

static void*
copyEnvMat(void *dst, void *src, int32 offset, int32)
{
	EnvMat *srcenv = *PLUGINOFFSET(EnvMat*, src, offset);
	if(srcenv == NULL)
		return dst;
	EnvMat *dstenv = new EnvMat;
	dstenv->scaleX = srcenv->scaleX;
	dstenv->scaleY = srcenv->scaleY;
	dstenv->transScaleX = srcenv->transScaleX;
	dstenv->transScaleY = srcenv->transScaleY;
	dstenv->shininess = srcenv->shininess;
	dstenv->texture = NULL;
	*PLUGINOFFSET(EnvMat*, dst, offset) = dstenv;
	return dst;
}

struct EnvStream {
	float scaleX, scaleY;
	float transScaleX, transScaleY;
	float shininess;
	int32 zero;
};

static void
readEnvMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	EnvStream buf;
	EnvMat *env = new EnvMat;
	*PLUGINOFFSET(EnvMat*, object, offset) = env;
	stream->read(&buf, sizeof(buf));
	env->scaleX = (int8)(buf.scaleX*8.0f);
	env->scaleY = (int8)(buf.scaleY*8.0f);
	env->transScaleX = (int8)(buf.transScaleX*8.0f);
	env->transScaleY = (int8)(buf.transScaleY*8.0f);
	env->shininess = (int8)(buf.shininess*255.0f);
	env->texture = NULL;
}

static void
writeEnvMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	EnvStream buf;
	EnvMat *env = *PLUGINOFFSET(EnvMat*, object, offset);
	buf.scaleX = env->scaleX/8.0f;
	buf.scaleY = env->scaleY/8.0f;
	buf.transScaleX = env->transScaleX/8.0f;
	buf.transScaleY = env->transScaleY/8.0f;
	buf.shininess = env->shininess/8.0f;
	buf.zero = 0;
	stream->write(&buf, sizeof(buf));
}

static int32
getSizeEnvMat(void *object, int32 offset, int32)
{
	EnvMat *env = *PLUGINOFFSET(EnvMat*, object, offset);
	return env ? sizeof(EnvStream) : -1;
}

// Specular mat

int32 specMatOffset;

static void*
createSpecMat(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(SpecMat*, object, offset) = NULL;
	return object;
}

static void*
destroySpecMat(void *object, int32 offset, int32)
{
	SpecMat **specmat = PLUGINOFFSET(SpecMat*, object, offset);
	if((*specmat)->texture)
		(*specmat)->texture->decRef();
	delete *specmat;
	*specmat = NULL;
	return object;
}

static void*
copySpecMat(void *dst, void *src, int32 offset, int32)
{
	SpecMat *srcspec = *PLUGINOFFSET(SpecMat*, src, offset);
	if(srcspec == NULL)
		return dst;
	SpecMat *dstspec = new SpecMat;
	*PLUGINOFFSET(SpecMat*, dst, offset) = dstspec;
	dstspec->specularity = srcspec->specularity;
	dstspec->texture = srcspec->texture;
	dstspec->texture->refCount++;
	return dst;
}

struct SpecStream {
	float specularity;
	char texname[24];
};

static void
readSpecMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	SpecStream buf;
	SpecMat *spec = new SpecMat;
	*PLUGINOFFSET(SpecMat*, object, offset) = spec;
	stream->read(&buf, sizeof(buf));
	spec->specularity = buf.specularity;
	spec->texture = new Texture;
	strncpy(spec->texture->name, buf.texname, 24);
}

static void
writeSpecMat(Stream *stream, int32, void *object, int32 offset, int32)
{
	SpecStream buf;
	SpecMat *spec = *PLUGINOFFSET(SpecMat*, object, offset);
	buf.specularity = spec->specularity;
	strncpy(buf.texname, spec->texture->name, 24);
	stream->write(&buf, sizeof(buf));
}

static int32
getSizeSpecMat(void *object, int32 offset, int32)
{
	SpecMat *spec = *PLUGINOFFSET(SpecMat*, object, offset);
	return spec ? sizeof(SpecStream) : -1;
}

void
registerEnvSpecPlugin(void)
{
	specMatOffset = Material::registerPlugin(sizeof(SpecMat*), ID_SPECMAT,
	                                         createSpecMat,
                                                 destroySpecMat,
                                                 copySpecMat);
	Material::registerPluginStream(ID_SPECMAT, readSpecMat,
                                                   writeSpecMat,
                                                   getSizeSpecMat);
	envMatOffset = Material::registerPlugin(sizeof(EnvMat*), ID_ENVMAT,
	                                        createEnvMat,
                                                destroyEnvMat,
                                                copyEnvMat);
	Material::registerPluginStream(ID_ENVMAT, readEnvMat,
                                                  writeEnvMat,
                                                  getSizeEnvMat);
}

}
