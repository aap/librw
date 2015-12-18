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
#include "rwd3d.h"
#include "rwxbox.h"
#include "mdl.h"
#include "gtaplg.h"

using namespace std;

namespace rw {

int32
findPlatform(Clump *c)
{
	for(int32 i = 0; i < c->numAtomics; i++){
		Geometry *g = c->atomicList[i]->geometry;
		if(g->instData)
			return g->instData->platform;
	}
	return 0;
}

void
switchPipes(Clump *c, int32 platform)
{
	for(int32 i = 0; i < c->numAtomics; i++){
		Atomic *a = c->atomicList[i];
		if(a->pipeline && a->pipeline->platform != platform){
			uint32 plgid = a->pipeline->pluginID;
			switch(plgid){
			case ID_SKIN:
				a->pipeline = skinGlobals.pipelines[platform];
				break;
			case ID_MATFX:
				a->pipeline = matFXGlobals.pipelines[platform];
				break;
			}
		}
	}
}

}

namespace gta {

void
attachPlugins(void)
{
	rw::ps2::registerPDSPlugin(12);
	gta::registerPDSPipes();

	rw::ps2::registerNativeRaster();
	rw::xbox::registerNativeRaster();
	rw::d3d::registerNativeRaster();

	rw::registerMeshPlugin();
	rw::registerNativeDataPlugin();
	rw::registerAtomicRightsPlugin();
	rw::registerMaterialRightsPlugin();
	rw::xbox::registerVertexFormatPlugin();
	rw::registerSkinPlugin();
	rw::registerHAnimPlugin();
	gta::registerNodeNamePlugin();
	rw::registerMatFXPlugin();
	rw::registerUVAnimPlugin();
	rw::ps2::registerADCPlugin();
	gta::registerExtraNormalsPlugin();
	gta::registerExtraVertColorPlugin();
	gta::registerEnvSpecPlugin();
	gta::registerBreakableModelPlugin();
	gta::registerCollisionPlugin();
	gta::register2dEffectPlugin();
	gta::registerPipelinePlugin();

	registerRslPlugin();

	rw::Atomic::init();
}

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
	//printf("%s\n", name);
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
	return len > 0 ? len : 0;
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

// Extra normals

int32 extraNormalsOffset;

static void*
createExtraNormals(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(float*, object, offset) = NULL;
	return object;
}

static void*
destroyExtraNormals(void *object, int32 offset, int32)
{
	float *extranormals = *PLUGINOFFSET(float*, object, offset);
	delete[] extranormals;
	*PLUGINOFFSET(float*, object, offset) = NULL;
	return object;
}

static void
readExtraNormals(Stream *stream, int32, void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	float **plgp = PLUGINOFFSET(float*, object, offset);
	if(*plgp)
		delete[] *plgp;
	float *extranormals = *plgp = new float[geo->numVertices*3];
	stream->read(extranormals, geo->numVertices*3*4);

//	for(int i = 0; i < geo->numVertices; i++){
//		float *nx = extranormals+i*3;
//		float *n = geo->morphTargets[0].normals;
//		float len = n[0]*n[0] + n[1]*n[1] + n[2]*n[2];
//		printf("%f %f %f %f\n", n[0], n[1], n[2], len);
//		printf("%f %f %f\n", nx[0], nx[1], nx[2]);
//	}
}

static void
writeExtraNormals(Stream *stream, int32, void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	float *extranormals = *PLUGINOFFSET(float*, object, offset);
	assert(extranormals != NULL);
	stream->write(extranormals, geo->numVertices*3*4);
}

static int32
getSizeExtraNormals(void *object, int32 offset, int32)
{
	Geometry *geo = (Geometry*)object;
	if(*PLUGINOFFSET(float*, object, offset))
		return geo->numVertices*3*4;
	return 0;
}

void
registerExtraNormalsPlugin(void)
{
	extraNormalsOffset = Geometry::registerPlugin(sizeof(void*),
	                                              ID_EXTRANORMALS,
	                                              createExtraNormals,
	                                              destroyExtraNormals,
	                                              NULL);
	Geometry::registerPluginStream(ID_EXTRANORMALS,
	                               readExtraNormals,
	                               writeExtraNormals,
	                               getSizeExtraNormals);
}


// Extra colors

int32 extraVertColorOffset;

void
allocateExtraVertColors(Geometry *g)
{
	ExtraVertColors *colordata =
		PLUGINOFFSET(ExtraVertColors, g, extraVertColorOffset);
	colordata->nightColors = new uint8[g->numVertices*4];
	colordata->dayColors = new uint8[g->numVertices*4];
	colordata->balance = 1.0f;
}

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
	return 0;
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
	return env ? (int)sizeof(EnvStream) : 0;
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
	if(*specmat == NULL)
		return object;
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
	return spec ? (int)sizeof(SpecStream) : 0;
}

void
registerEnvSpecPlugin(void)
{
	envMatOffset = Material::registerPlugin(sizeof(EnvMat*), ID_ENVMAT,
	                                        createEnvMat,
                                                destroyEnvMat,
                                                copyEnvMat);
	Material::registerPluginStream(ID_ENVMAT, readEnvMat,
                                                  writeEnvMat,
                                                  getSizeEnvMat);
	specMatOffset = Material::registerPlugin(sizeof(SpecMat*), ID_SPECMAT,
	                                         createSpecMat,
                                                 destroySpecMat,
                                                 copySpecMat);
	Material::registerPluginStream(ID_SPECMAT, readSpecMat,
                                                   writeSpecMat,
                                                   getSizeSpecMat);
}

// Pipeline

int32 pipelineOffset;

static void*
createPipeline(void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, object, offset) = 0;
	return object;
}

static void*
copyPipeline(void *dst, void *src, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, dst, offset) = *PLUGINOFFSET(uint32, src, offset);
	return dst;
}

static void
readPipeline(Stream *stream, int32, void *object, int32 offset, int32)
{
	*PLUGINOFFSET(uint32, object, offset) = stream->readU32();
	printf("%x\n", *PLUGINOFFSET(uint32, object, offset));
}

static void
writePipeline(Stream *stream, int32, void *object, int32 offset, int32)
{
	stream->writeU32(*PLUGINOFFSET(uint32, object, offset));
}

static int32
getSizePipeline(void *object, int32 offset, int32)
{
	if(*PLUGINOFFSET(uint32, object, offset))
		return 4;
	return 0;
}

void
registerPipelinePlugin(void)
{
	pipelineOffset = Atomic::registerPlugin(sizeof(uint32), ID_PIPELINE,
	                                        createPipeline,
                                                NULL,
                                                copyPipeline);
	Atomic::registerPluginStream(ID_PIPELINE, readPipeline,
	                             writePipeline, getSizePipeline);
}

uint32
getPipelineID(Atomic *atomic)
{
	return *PLUGINOFFSET(uint32, atomic, pipelineOffset);
}

void
setPipelineID(Atomic *atomic, uint32 id)
{
	*PLUGINOFFSET(uint32, atomic, pipelineOffset) = id;
}

// 2dEffect

struct SizedData
{
	uint32 size;
	uint8 *data;
};

int32 twodEffectOffset;

static void*
create2dEffect(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	data->size = 0;
	data->data = NULL;
	return object;
}

static void*
destroy2dEffect(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	delete[] data->data;
	data->data = NULL;
	data->size = 0;
	return object;
}

static void*
copy2dEffect(void *dst, void *src, int32 offset, int32)
{
	SizedData *srcdata, *dstdata;
	dstdata = PLUGINOFFSET(SizedData, dst, offset);
	srcdata = PLUGINOFFSET(SizedData, src, offset);
	dstdata->size = srcdata->size;
	if(dstdata->size != 0){
		dstdata->data = new uint8[dstdata->size];
		memcpy(dstdata->data, srcdata->data, dstdata->size);
	}
	return dst;
}

static void
read2dEffect(Stream *stream, int32 size, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	data->size = size;
	data->data = new uint8[data->size];
	stream->read(data->data, data->size);
}

static void
write2dEffect(Stream *stream, int32, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	stream->write(data->data, data->size);
}

static int32
getSize2dEffect(void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	return data->size;
}

void
register2dEffectPlugin(void)
{
	twodEffectOffset = Geometry::registerPlugin(sizeof(SizedData), ID_2DEFFECT,
	                                            create2dEffect,
                                                    destroy2dEffect,
                                                    copy2dEffect);
	Geometry::registerPluginStream(ID_2DEFFECT, read2dEffect,
	                               write2dEffect, getSize2dEffect);
}

// Collision

int32 collisionOffset;

static void*
createCollision(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	data->size = 0;
	data->data = NULL;
	return object;
}

static void*
destroyCollision(void *object, int32 offset, int32)
{
	SizedData *data;
	data = PLUGINOFFSET(SizedData, object, offset);
	delete[] data->data;
	data->data = NULL;
	data->size = 0;
	return object;
}

static void*
copyCollision(void *dst, void *src, int32 offset, int32)
{
	SizedData *srcdata, *dstdata;
	dstdata = PLUGINOFFSET(SizedData, dst, offset);
	srcdata = PLUGINOFFSET(SizedData, src, offset);
	dstdata->size = srcdata->size;
	if(dstdata->size != 0){
		dstdata->data = new uint8[dstdata->size];
		memcpy(dstdata->data, srcdata->data, dstdata->size);
	}
	return dst;
}

static void
readCollision(Stream *stream, int32 size, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	data->size = size;
	data->data = new uint8[data->size];
	stream->read(data->data, data->size);
}

static void
writeCollision(Stream *stream, int32, void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	stream->write(data->data, data->size);
}

static int32
getSizeCollision(void *object, int32 offset, int32)
{
	SizedData *data = PLUGINOFFSET(SizedData, object, offset);
	return data->size;
}

void
registerCollisionPlugin(void)
{
	collisionOffset = Clump::registerPlugin(sizeof(SizedData), ID_COLLISION,
	                                        createCollision,
                                                destroyCollision,
                                                copyCollision);
	Clump::registerPluginStream(ID_COLLISION, readCollision,
	                            writeCollision, getSizeCollision);
}

/*
 * PS2
 */

using namespace ps2;

PipeAttribute saXYZADC = {
	"saXYZADC",
	AT_V4_16 | AT_RW
};

PipeAttribute saUV = {
	"saUV",
	AT_V2_16 | AT_RW
};

PipeAttribute saUV2 = {
	"saUV2",
	AT_V4_16 | AT_RW
};

PipeAttribute saRGBA = {
	"saRGBA",
	AT_V4_8 | AT_UNSGN | AT_RW
};

PipeAttribute saRGBA2 = {
	"saRGBA2",
	AT_V4_16 | AT_UNSGN | AT_RW
};

PipeAttribute saNormal = {
	"saNormal",
	AT_V4_8 | AT_RW
};

PipeAttribute saWeights = {
	"saWeights",
	AT_V4_32 | AT_RW
};

static bool hasTex2(uint32 id)
{
	return id == 0x53f2008b;
}
static bool hasNormals(uint32 id)
{
	return id == 0x53f20085 || id == 0x53f20087 || id == 0x53f20089 ||
		id == 0x53f2008b || id == 0x53f2008d || id == 0x53f2008f;
}
static bool hasColors(uint32 id)
{
	return id == 0x53f20081 || id == 0x53f20083 || id == 0x53f2008d || id == 0x53f2008f;
}
static bool hasColors2(uint32 id)
{
	return id == 0x53f20083 || id == 0x53f2008f;
}

struct SaVert {
	float32 p[3];
	float32 n[3];
	float32 t0[2];
	float32 t1[2];
	uint8   c0[4];
	uint8   c1[4];
	float32 w[4];
	uint8   i[4];
};

static void
saPreCB(MatPipeline *p, Geometry *geo)
{
	// allocate ADC, extra colors, skin
	allocateADC(geo);
	if(hasColors2(p->pluginData) && extraVertColorOffset)
		allocateExtraVertColors(geo);
	if(p->pluginData == 0x53f20089)
		skinPreCB(p, geo);
}

static void
saPostCB(MatPipeline *p, Geometry *geo)
{
	skinPostCB(p, geo);
}

int32
findSAVertex(Geometry *g, uint32 flags[], uint32 mask, SaVert *v)
{
	Skin *skin = *PLUGINOFFSET(Skin*, g, skinGlobals.offset);
	float32 *wghts = NULL;
	uint8 *inds    = NULL;
	if(skin){
		wghts = skin->weights;
		inds = skin->indices;
	}
	float32 *verts = g->morphTargets[0].vertices;
	float32 *tex0  = g->texCoords[0];
	float32 *tex1  = g->texCoords[1];
	float32 *norms = g->morphTargets[0].normals;
	uint8 *cols0   = g->colors;
	uint8 *cols1   = NULL;
	if(extraVertColorOffset)
		cols1 = PLUGINOFFSET(ExtraVertColors, g, extraVertColorOffset)->nightColors;

	for(int32 i = 0; i < g->numVertices; i++){
		if(mask & flags[i] & 0x1 &&
		   !(verts[0] == v->p[0] && verts[1] == v->p[1] && verts[2] == v->p[2]))
			goto cont;
		if(mask & flags[i] & 0x10 &&
		   !(norms[0] == v->n[0] && norms[1] == v->n[1] && norms[2] == v->n[2]))
			goto cont;
		if(mask & flags[i] & 0x100 &&
		   !(cols0[0] == v->c0[0] && cols0[1] == v->c0[1] &&
		     cols0[2] == v->c0[2] && cols0[3] == v->c0[3]))
			goto cont;
		if(mask & flags[i] & 0x200 &&
		   !(cols1[0] == v->c1[0] && cols1[1] == v->c1[1] &&
		     cols1[2] == v->c1[2] && cols1[3] == v->c1[3]))
			goto cont;
		if(mask & flags[i] & 0x1000 &&
		   !(tex0[0] == v->t0[0] && tex0[1] == v->t0[1]))
			goto cont;
		if(mask & flags[i] & 0x2000 &&
		   !(tex1[0] == v->t1[0] && tex1[1] == v->t1[1]))
			goto cont;
		if(mask & flags[i] & 0x10000 &&
		   !(wghts[0] == v->w[0] && wghts[1] == v->w[1] &&
		     wghts[2] == v->w[2] && wghts[3] == v->w[3] &&
		     inds[0] == v->i[0] && inds[1] == v->i[1] &&
		     inds[2] == v->i[2] && inds[3] == v->i[3]))
			goto cont;
		return i;
	cont:
		verts += 3;
		tex0 += 2;
		tex1 += 2;
		norms += 3;
		cols0 += 4;
		cols1 += 4;
		wghts += 4;
		inds += 4;
	}
	return -1;
}

void
insertSAVertex(Geometry *geo, int32 i, uint32 mask, SaVert *v)
{
	insertVertex(geo, i, mask, v->p, v->t0, v->t1, v->c0, v->n);
	if(mask & 0x200 && extraVertColorOffset){
		uint8 *cols1 =
		 &PLUGINOFFSET(ExtraVertColors, geo, extraVertColorOffset)->nightColors[i*4];
		cols1[0] = v->c1[0];
		cols1[1] = v->c1[1];
		cols1[2] = v->c1[2];
		cols1[3] = v->c1[3];
	}
	if(mask & 0x10000 && skinGlobals.offset){
		Skin *skin = *PLUGINOFFSET(Skin*, geo, skinGlobals.offset);
		memcpy(&skin->weights[i*4], v->w, 16);
		memcpy(&skin->indices[i*4], v->i, 4);
	}
}

static void
saUninstanceCB(ps2::MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[])
{
	uint32 id = pipe->pluginData;
	int16 *verts       = (int16*)data[0];
	int16 *texcoords   = (int16*)data[1];
	uint8 *colors      = (uint8*)data[2];
	int8 *norms        = (int8*)data[id == 0x53f20089 ? 2 : 3];
	uint32 *wghts      = (uint32*)data[3];
	float vertScale = 1.0f/128.0f;
	if(id == 0x53f20085 || id == 0x53f20087 ||
	   id == 0x53f20089 || id == 0x53f2008b)
		vertScale = 1.0f/1024.0f;
	uint32 mask = 0x1;	// vertices
	int cinc = 4;
	int tinc = 2;
	if((geo->geoflags & Geometry::NORMALS) && hasNormals(id))
		mask |= 0x10;
	if((geo->geoflags & Geometry::PRELIT) && hasColors(id))
		mask |= 0x100;
	if(hasColors2(id)){
		mask |= 0x200;
		cinc *= 2;
	}
	if(geo->numTexCoordSets > 0)
		mask |= 0x1000;
	if(geo->numTexCoordSets > 0 && hasTex2(id)){
		mask |= 0x2000;
		tinc *= 2;
	}
	if(id == 0x53f20089)
		mask |= 0x10000;
	SaVert v;
	int32 idxstart = 0;
	for(Mesh *m = geo->meshHeader->mesh; m < mesh; m++)
		idxstart += m->numIndices;
	int8 *adc = PLUGINOFFSET(ADCData, geo, adcOffset)->adcBits;
	for(uint32 i = 0; i < mesh->numIndices; i++){
		v.p[0] = verts[0]*vertScale;
		v.p[1] = verts[1]*vertScale;
		v.p[2] = verts[2]*vertScale;
		if(mask & 0x10){
			v.n[0] = norms[0]/127.0f;
			v.n[1] = norms[1]/127.0f;
			v.n[2] = norms[2]/127.0f;
		}
		if(mask & 0x200){
			v.c0[0] = colors[0];
			v.c0[1] = colors[2];
			v.c0[2] = colors[4];
			v.c0[3] = colors[6];
			v.c1[0] = colors[1];
			v.c1[1] = colors[3];
			v.c1[2] = colors[5];
			v.c1[3] = colors[7];
		}else if(mask & 0x100){
			v.c0[0] = colors[0];
			v.c0[1] = colors[1];
			v.c0[2] = colors[2];
			v.c0[3] = colors[3];
		}
		if(mask & 0x1000){
			v.t0[0] = texcoords[0]/4096.0f;
			v.t0[1] = texcoords[1]/4096.0f;
		}
		if(mask & 0x2000){
			v.t1[0] = texcoords[2]/4096.0f;
			v.t1[1] = texcoords[3]/4096.0f;
		}
		if(mask & 0x10000){
			for(int j = 0; j < 4; j++){
				((uint32*)v.w)[j] = wghts[j] & ~0x3FF;
				v.i[j] = (wghts[j] & 0x3FF) >> 2;
				if(v.i[j]) v.i[j]--;
				if(v.w[j] == 0.0f) v.i[j] = 0;
			}
		}
		int32 idx = findSAVertex(geo, flags, mask, &v);
		if(idx < 0)
			idx = geo->numVertices++;
		mesh->indices[i] = idx;
		adc[idxstart+i] = !!verts[3];
		flags[idx] = mask;
		insertSAVertex(geo, idx, mask, &v);

		verts += 4;
		texcoords += tinc;
		colors += cinc;
		norms += 4;
		wghts += 4;
	}

}

static void
saInstanceCB(MatPipeline *, Geometry *g, Mesh *m, uint8 **data, int32 n)
{
}

void
registerPDSPipes(void)
{
	Pipeline *pipe;
	MatPipeline *mpipe;

	// Atomic pipes

	pipe = new ps2::ObjPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_PDS;
	pipe->pluginData = 0x53f20080;
	ps2::registerPDSPipe(pipe);

	pipe = new ps2::ObjPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_PDS;
	pipe->pluginData = 0x53f20082;
	ps2::registerPDSPipe(pipe);

	pipe = new ps2::ObjPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_PDS;
	pipe->pluginData = 0x53f20084;
	ps2::registerPDSPipe(pipe);

	pipe = new ps2::ObjPipeline(PLATFORM_PS2);
	pipe->pluginID = ID_PDS;
	pipe->pluginData = 0x53f20088;
	ps2::registerPDSPipe(pipe);

	// Material pipes

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f20081;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[2] = &saRGBA;
	uint32 vertCount = MatPipeline::getVertCount(VU_Lights, 3, 3, 2);
	mpipe->setTriBufferSizes(3, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f20083;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[2] = &saRGBA2;
	vertCount = MatPipeline::getVertCount(VU_Lights, 3, 3, 2);
	mpipe->setTriBufferSizes(3, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f20085;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[3] = &saNormal;
	vertCount = MatPipeline::getVertCount(VU_Lights, 4, 3, 2);
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f20087;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[3] = &saNormal;
	vertCount = MatPipeline::getVertCount(0x3BD, 4, 3, 3);
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f20089;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[2] = &saNormal;
	mpipe->attribs[3] = &saWeights;
//	these values give vertCount = 0x33 :/
//	vertCount = MatPipeline::getVertCount(0x2D0, 4, 3, 2);
	vertCount = 0x30;
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	mpipe->postUninstCB = saPostCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f2008b;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV2;
	mpipe->attribs[3] = &saNormal;
	vertCount = MatPipeline::getVertCount(0x3BD, 4, 3, 3);
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f2008d;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[2] = &saRGBA;
	mpipe->attribs[3] = &saNormal;
	vertCount = MatPipeline::getVertCount(0x3BD, 4, 3, 3);
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);

	mpipe = new MatPipeline(PLATFORM_PS2);
	mpipe->pluginID = ID_PDS;
	mpipe->pluginData = 0x53f2008f;
	mpipe->attribs[0] = &saXYZADC;
	mpipe->attribs[1] = &saUV;
	mpipe->attribs[2] = &saRGBA2;
	mpipe->attribs[3] = &saNormal;
	vertCount = MatPipeline::getVertCount(0x3BD, 4, 3, 3);
	mpipe->setTriBufferSizes(4, vertCount);
	mpipe->vifOffset = mpipe->inputStride*vertCount;
	mpipe->instanceCB = saInstanceCB;
	mpipe->preUninstCB = saPreCB;
	mpipe->uninstanceCB = saUninstanceCB;
	ps2::registerPDSPipe(mpipe);
}

}
