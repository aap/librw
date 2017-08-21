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

PluginList Clump::s_plglist = { sizeof(Clump), sizeof(Clump), nil, nil };
PluginList Atomic::s_plglist = { sizeof(Atomic), sizeof(Atomic), nil, nil };

//
// Clump
//

Clump*
Clump::create(void)
{
	Clump *clump = (Clump*)malloc(s_plglist.size);
	if(clump == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	clump->object.init(Clump::ID, 0);
	clump->atomics.init();
	clump->lights.init();
	clump->cameras.init();
	s_plglist.construct(clump);
	return clump;
}

Clump*
Clump::clone(void)
{
	Clump *clump = Clump::create();
	Frame *root = this->getFrame()->cloneHierarchy();
	clump->setFrame(root);
	FORLIST(lnk, this->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		Atomic *atomic = a->clone();
		atomic->setFrame(a->getFrame()->root);
		clump->addAtomic(atomic);
	}
	root->purgeClone();
	s_plglist.copy(clump, this);
	return clump;
}

void
Clump::destroy(void)
{
	Frame *f;
	s_plglist.destruct(this);
	FORLIST(lnk, this->atomics)
		Atomic::fromClump(lnk)->destroy();
	FORLIST(lnk, this->lights)
		Light::fromClump(lnk)->destroy();
	FORLIST(lnk, this->cameras)
		Camera::fromClump(lnk)->destroy();
	if(f = this->getFrame(), f)
		f->destroyHierarchy();
	free(this);
}

Clump*
Clump::streamRead(Stream *stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;
	int32 numGeometries;
	Geometry **geometryList;

	if(!findChunk(stream, ID_STRUCT, &length, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	stream->read(buf, length);
	int32 numAtomics = buf[0];
	int32 numLights = 0;
	int32 numCameras = 0;
	if(version > 0x33000){
		numLights = buf[1];
		numCameras = buf[2];
	}
	clump = Clump::create();
	if(clump == nil)
		return nil;

	// Frame list
	FrameList_ frmlst;
	frmlst.frames = nil;
	if(!findChunk(stream, ID_FRAMELIST, nil, nil)){
		RWERROR((ERR_CHUNK, "FRAMELIST"));
		goto fail;
	}
	if(frmlst.streamRead(stream) == nil)
		goto fail;
	clump->setFrame(frmlst.frames[0]);

	// Geometry list
	numGeometries = 0;
	geometryList = nil;
	if(version >= 0x30400){
		if(!findChunk(stream, ID_GEOMETRYLIST, nil, nil)){
			RWERROR((ERR_CHUNK, "GEOMETRYLIST"));
			goto fail;
		}
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			goto fail;
		}
		numGeometries = stream->readI32();
		if(numGeometries){
			size_t sz = numGeometries*sizeof(Geometry*);
			geometryList = (Geometry**)malloc(sz);
			if(geometryList == nil){
				RWERROR((ERR_ALLOC, sz));
				goto fail;
			}
			memset(geometryList, 0, sz);
		}
		for(int32 i = 0; i < numGeometries; i++){
			if(!findChunk(stream, ID_GEOMETRY, nil, nil)){
				RWERROR((ERR_CHUNK, "GEOMETRY"));
				goto failgeo;
			}
			geometryList[i] = Geometry::streamRead(stream);
			if(geometryList[i] == nil)
				goto failgeo;
		}
	}

	// Atomics
	Atomic *a;
	for(int32 i = 0; i < numAtomics; i++){
		if(!findChunk(stream, ID_ATOMIC, nil, nil)){
			RWERROR((ERR_CHUNK, "ATOMIC"));
			goto failgeo;
		}
		a = Atomic::streamReadClump(stream, &frmlst, geometryList);
		if(a == nil)
			goto failgeo;
		clump->addAtomic(a);
	}

	// Lights
	int32 frm;
	Light *l;
	for(int32 i = 0; i < numLights; i++){
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			goto failgeo;
		}
		frm = stream->readI32();
		if(!findChunk(stream, ID_LIGHT, nil, nil)){
			RWERROR((ERR_CHUNK, "LIGHT"));
			goto failgeo;
		}
		l = Light::streamRead(stream);
		if(l == nil)
			goto failgeo;
		l->setFrame(frmlst.frames[frm]);
		clump->addLight(l);
	}

	// Cameras
	Camera *cam;
	for(int32 i = 0; i < numCameras; i++){
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			goto failgeo;
		}
		frm = stream->readI32();
		if(!findChunk(stream, ID_CAMERA, nil, nil)){
			RWERROR((ERR_CHUNK, "CAMERA"));
			goto failgeo;
		}
		cam = Camera::streamRead(stream);
		if(cam == nil)
			goto failgeo;
		cam->setFrame(frmlst.frames[frm]);
		clump->addCamera(cam);
	}

	for(int32 i = 0; i < numGeometries; i++)
		if(geometryList[i])
			geometryList[i]->destroy();
	free(geometryList);
	free(frmlst.frames);
	if(s_plglist.streamRead(stream, clump))
		return clump;

failgeo:
	for(int32 i = 0; i < numGeometries; i++)
		if(geometryList[i])
			geometryList[i]->destroy();
	free(geometryList);
fail:
	free(frmlst.frames);
	clump->destroy();
	return nil;
}

bool
Clump::streamWrite(Stream *stream)
{
	int size = this->streamGetSize();
	writeChunkHeader(stream, ID_CLUMP, size);
	int32 numAtomics = this->countAtomics();
	int32 numLights = this->countLights();
	int32 numCameras = this->countCameras();
	int buf[3] = { numAtomics, numLights, numCameras };
	size = version > 0x33000 ? 12 : 4;
	writeChunkHeader(stream, ID_STRUCT, size);
	stream->write(buf, size);

	FrameList_ frmlst;
	frmlst.numFrames = this->getFrame()->count();
	frmlst.frames = (Frame**)malloc(frmlst.numFrames*sizeof(Frame*));
	makeFrameList(this->getFrame(), frmlst.frames);
	frmlst.streamWrite(stream);

	if(rw::version >= 0x30400){
		size = 12+4;
		FORLIST(lnk, this->atomics)
			size += 12 + Atomic::fromClump(lnk)->geometry->streamGetSize();
		writeChunkHeader(stream, ID_GEOMETRYLIST, size);
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(numAtomics);	// same as numGeometries
		FORLIST(lnk, this->atomics)
			Atomic::fromClump(lnk)->geometry->streamWrite(stream);
	}

	FORLIST(lnk, this->atomics)
		Atomic::fromClump(lnk)->streamWriteClump(stream, &frmlst);

	FORLIST(lnk, this->lights){
		Light *l = Light::fromClump(lnk);
		int frm = findPointer(l->getFrame(), (void**)frmlst.frames, frmlst.numFrames);
		if(frm < 0)
			return false;
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(frm);
		l->streamWrite(stream);
	}

	FORLIST(lnk, this->cameras){
		Camera *c = Camera::fromClump(lnk);
		int frm = findPointer(c->getFrame(), (void**)frmlst.frames, frmlst.numFrames);
		if(frm < 0)
			return false;
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(frm);
		c->streamWrite(stream);
	}

	free(frmlst.frames);

	s_plglist.streamWrite(stream, this);
	return true;
}

uint32
Clump::streamGetSize(void)
{
	uint32 size = 0;
	size += 12;	// Struct
	size += 4;	// numAtomics
	if(version > 0x33000)
		size += 8;	// numLights, numCameras

	// Frame list
	size += FrameList_::streamGetSize(this->getFrame());

	if(rw::version >= 0x30400){
		// Geometry list
		size += 12 + 12 + 4;
		FORLIST(lnk, this->atomics)
			size += 12 + Atomic::fromClump(lnk)->geometry->streamGetSize();
	}

	// Atomics
	FORLIST(lnk, this->atomics)
		size += 12 + Atomic::fromClump(lnk)->streamGetSize();

	// Lights
	FORLIST(lnk, this->lights)
		size += 16 + 12 + Light::fromClump(lnk)->streamGetSize();

	// Cameras
	FORLIST(lnk, this->cameras)
		size += 16 + 12 + Camera::fromClump(lnk)->streamGetSize();

	size += 12 + s_plglist.streamGetSize(this);
	return size;
}

void
Clump::render(void)
{
	Atomic *a;
	FORLIST(lnk, this->atomics){
		a = Atomic::fromClump(lnk);
		if(a->object.object.flags & Atomic::RENDER)
			a->render();
	}
}

//
// Atomic
//

static void
atomicSync(ObjectWithFrame *obj)
{
	// TODO: interpolate
	obj->object.privateFlags |= Atomic::WORLDBOUNDDIRTY;
}


static void
worldAtomicSync(ObjectWithFrame *obj)
{
	Atomic *atomic = (Atomic*)obj;
	atomic->originalSync(obj);
}

Atomic*
Atomic::create(void)
{
	Atomic *atomic = (Atomic*)malloc(s_plglist.size);
	if(atomic == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	atomic->object.object.init(Atomic::ID, 0);
	atomic->object.syncCB = atomicSync;
	atomic->geometry = nil;
	atomic->boundingSphere.center.set(0.0f, 0.0f, 0.0f);
	atomic->boundingSphere.radius = 0.0f;
	atomic->worldBoundingSphere.center.set(0.0f, 0.0f, 0.0f);
	atomic->worldBoundingSphere.radius = 0.0f;
	atomic->setFrame(nil);
	atomic->object.object.privateFlags |= WORLDBOUNDDIRTY;
	atomic->clump = nil;
	atomic->pipeline = nil;
	atomic->renderCB = Atomic::defaultRenderCB;
	atomic->object.object.flags = Atomic::COLLISIONTEST | Atomic::RENDER;
	// TODO: interpolator

	// World extension
	atomic->world = nil;
	atomic->originalSync = atomic->object.syncCB;
	atomic->object.syncCB = worldAtomicSync;

	s_plglist.construct(atomic);
	return atomic;
}

Atomic*
Atomic::clone()
{
	Atomic *atomic = Atomic::create();
	if(atomic == nil)
		return nil;
	atomic->object.object.copy(&this->object.object);
	atomic->object.object.privateFlags |= WORLDBOUNDDIRTY;
	if(this->geometry)
		atomic->setGeometry(this->geometry, 0);
	atomic->pipeline = this->pipeline;
	s_plglist.copy(atomic, this);
	return atomic;
}

void
Atomic::destroy(void)
{
	s_plglist.destruct(this);
	if(this->geometry)
		this->geometry->destroy();
	if(this->clump)
		this->inClump.remove();
	this->setFrame(nil);
	free(this);
}

void
Atomic::removeFromClump(void)
{
	if(this->clump){
		this->inClump.remove();
		this->clump = nil;
	}
}

void
Atomic::setGeometry(Geometry *geo, uint32 flags)
{
	if(this->geometry)
		this->geometry->destroy();
	if(geo)
		geo->refCount++;
	this->geometry = geo;
	if(flags & SAMEBOUNDINGSPHERE)
		return;
	if(geo){
		this->boundingSphere = geo->morphTargets[0].boundingSphere;
		if(this->getFrame())	// TODO: && getWorld???
			this->getFrame()->updateObjects();
	}
}

Sphere*
Atomic::getWorldBoundingSphere(void)
{
	Sphere *s = &this->worldBoundingSphere;
	// TODO: if we ever support morphing, check interpolation
	if(!this->getFrame()->dirty() &&
	   (this->object.object.privateFlags & WORLDBOUNDDIRTY) == 0)
		return s;
	Matrix *ltm = this->getFrame()->getLTM();
	// TODO: support scaling
	V3d::transformPoints(&s->center, &this->boundingSphere.center, 1, ltm);
	s->radius = this->boundingSphere.radius;
	this->object.object.privateFlags &= ~WORLDBOUNDDIRTY;
	return s;
}

static uint32 atomicRights[2];

Atomic*
Atomic::streamReadClump(Stream *stream,
                        FrameList_ *frameList, Geometry **geometryList)
{
	int32 buf[4];
	uint32 version;
	if(!findChunk(stream, ID_STRUCT, nil, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	stream->read(buf, version < 0x30400 ? 12 : 16);
	Atomic *atomic = Atomic::create();
	if(atomic == nil)
		return nil;
	atomic->setFrame(frameList->frames[buf[0]]);
	Geometry *g;
	if(version < 0x30400){
		if(!findChunk(stream, ID_GEOMETRY, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			goto fail;
		}
		g = Geometry::streamRead(stream);
		if(g == nil)
			goto fail;
		atomic->setGeometry(g, 0);
		g->destroy();
	}else
		atomic->setGeometry(geometryList[buf[1]], 0);
	atomic->object.object.flags = buf[2];

	atomicRights[0] = 0;
	if(!s_plglist.streamRead(stream, atomic))
		goto fail;
	if(atomicRights[0])
		s_plglist.assertRights(atomic, atomicRights[0], atomicRights[1]);
	return atomic;

fail:
	atomic->destroy();
	return nil;
}

bool
Atomic::streamWriteClump(Stream *stream, FrameList_ *frmlst)
{
	int32 buf[4] = { 0, 0, 0, 0 };
	Clump *c = this->clump;
	if(c == nil)
		return false;
	writeChunkHeader(stream, ID_ATOMIC, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, rw::version < 0x30400 ? 12 : 16);
	buf[0] = findPointer(this->getFrame(), (void**)frmlst->frames, frmlst->numFrames);

	if(version < 0x30400){
		buf[1] = this->object.object.flags;
		stream->write(buf, sizeof(int[3]));
		this->geometry->streamWrite(stream);
	}else{
		buf[1] = 0;
		FORLIST(lnk, c->atomics){
			if(Atomic::fromClump(lnk)->geometry == this->geometry)
				goto foundgeo;
			buf[1]++;
		}
		return false;
	foundgeo:
		buf[2] = this->object.object.flags;
		stream->write(buf, sizeof(buf));
	}

	s_plglist.streamWrite(stream, this);
	return true;
}

uint32
Atomic::streamGetSize(void)
{
	uint32 size = 12 + 12 + 12 + s_plglist.streamGetSize(this);
	if(rw::version < 0x30400)
		size += 12 + this->geometry->streamGetSize();
	else
		size += 4;
	return size;
}

ObjPipeline*
Atomic::getPipeline(void)
{
	return this->pipeline ?
		this->pipeline :
		engine->driver[platform]->defaultPipeline;
}

void
Atomic::instance(void)
{
	if(this->geometry->flags & Geometry::NATIVE)
		return;
	this->getPipeline()->instance(this);
	this->geometry->flags |= Geometry::NATIVE;
}

void
Atomic::uninstance(void)
{
	if(!(this->geometry->flags & Geometry::NATIVE))
		return;
	this->getPipeline()->uninstance(this);
	// this should be done by the CB already, just make sure
	this->geometry->flags &= ~Geometry::NATIVE;
}

void
Atomic::defaultRenderCB(Atomic *atomic)
{
	atomic->getPipeline()->render(atomic);
}

// Atomic Rights plugin

static Stream*
readAtomicRights(Stream *stream, int32, void *, int32, int32)
{
	stream->read(atomicRights, 8);
	return stream;
}

static Stream*
writeAtomicRights(Stream *stream, int32, void *object, int32, int32)
{
	Atomic *atomic = (Atomic*)object;
	uint32 buffer[2];
	buffer[0] = atomic->pipeline->pluginID;
	buffer[1] = atomic->pipeline->pluginData;
	stream->write(buffer, 8);
	return stream;
}

static int32
getSizeAtomicRights(void *object, int32, int32)
{
	Atomic *atomic = (Atomic*)object;
	if(atomic->pipeline == nil || atomic->pipeline->pluginID == 0)
		return 0;
	return 8;
}

void
registerAtomicRightsPlugin(void)
{
	Atomic::registerPlugin(0, ID_RIGHTTORENDER, nil, nil, nil);
	Atomic::registerPluginStream(ID_RIGHTTORENDER,
	                             readAtomicRights,
	                             writeAtomicRights,
	                             getSizeAtomicRights);
}

}
