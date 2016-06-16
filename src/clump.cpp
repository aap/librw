#include <cstdio>
#include <cstdlib>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

using namespace std;

#define PLUGIN_ID 2

namespace rw {

//
// Clump
//

Clump*
Clump::create(void)
{
	Clump *clump = (Clump*)malloc(PluginBase::s_size);
	if(clump == nil){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return nil;
	}
	clump->object.init(Clump::ID, 0);
	clump->atomics.init();
	clump->lights.init();
	clump->cameras.init();
	clump->constructPlugins();
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
	clump->copyPlugins(this);
	return clump;
}

void
Clump::destroy(void)
{
	Frame *f;
	this->destructPlugins();
	FORLIST(lnk, this->atomics)
		Atomic::fromClump(lnk)->destroy();
	FORLIST(lnk, this->lights)
		Light::fromClump(lnk)->destroy();
	FORLIST(lnk, this->cameras)
		Camera::fromClump(lnk)->destroy();
	if(f = this->getFrame())
		f->destroyHierarchy();
	free(this);
}

Clump*
Clump::streamRead(Stream *stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;

	if(!findChunk(stream, ID_STRUCT, &length, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	clump = Clump::create();
	if(clump == nil)
		return nil;
	stream->read(buf, length);
	int32 numAtomics = buf[0];
	int32 numLights = 0;
	int32 numCameras = 0;
	if(version > 0x33000){
		numLights = buf[1];
		numCameras = buf[2];
	}

	// Frame list
	Frame **frameList;
	int32 numFrames;
	clump->frameListStreamRead(stream, &frameList, &numFrames);
	clump->setFrame(frameList[0]);

	Geometry **geometryList = 0;
	if(version >= 0x30400){
		// Geometry list
		int32 numGeometries = 0;
		if(!findChunk(stream, ID_GEOMETRYLIST, nil, nil)){
			RWERROR((ERR_CHUNK, "GEOMETRYLIST"));
			// TODO: free
			return nil;
		}
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			// TODO: free
			return nil;
		}
		numGeometries = stream->readI32();
		if(numGeometries)
			geometryList = new Geometry*[numGeometries];
		for(int32 i = 0; i < numGeometries; i++){
			if(!findChunk(stream, ID_GEOMETRY, nil, nil)){
				RWERROR((ERR_CHUNK, "GEOMETRY"));
				// TODO: free
				return nil;
			}
			geometryList[i] = Geometry::streamRead(stream);
			if(geometryList[i] == nil){
				// TODO: free
				return nil;
			}
		}
	}

	// Atomics
	Atomic *a;
	for(int32 i = 0; i < numAtomics; i++){
		if(!findChunk(stream, ID_ATOMIC, nil, nil)){
			RWERROR((ERR_CHUNK, "ATOMIC"));
			// TODO: free
			return nil;
		}
		a = Atomic::streamReadClump(stream, frameList, geometryList);
		if(a == nil){
			// TODO: free
			return nil;
		}
		clump->addAtomic(a);
	}

	// Lights
	for(int32 i = 0; i < numLights; i++){
		int32 frm;
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			// TODO: free
			return nil;
		}
		frm = stream->readI32();
		if(!findChunk(stream, ID_LIGHT, nil, nil)){
			RWERROR((ERR_CHUNK, "LIGHT"));
			// TODO: free
			return nil;
		}
		Light *l = Light::streamRead(stream);
		if(l == nil){
			// TODO: free
			return nil;
		}
		l->setFrame(frameList[frm]);
		clump->addLight(l);
	}

	// Cameras
	for(int32 i = 0; i < numCameras; i++){
		int32 frm;
		if(!findChunk(stream, ID_STRUCT, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			// TODO: free
			return nil;
		}
		frm = stream->readI32();
		if(!findChunk(stream, ID_CAMERA, nil, nil)){
			RWERROR((ERR_CHUNK, "CAMERA"));
			// TODO: free
			return nil;
		}
		Camera *cam = Camera::streamRead(stream);
		if(cam == nil){
			// TODO: free
			return nil;
		}
		cam->setFrame(frameList[frm]);
		clump->addCamera(cam);
	}

	delete[] frameList;

	clump->streamReadPlugins(stream);
	return clump;
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

	int32 numFrames = this->getFrame()->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList(this->getFrame(), flist);
	this->frameListStreamWrite(stream, flist, numFrames);

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
		Atomic::fromClump(lnk)->streamWriteClump(stream, flist, numFrames);

	FORLIST(lnk, this->lights){
		Light *l = Light::fromClump(lnk);
		int frm = findPointer(l->getFrame(), (void**)flist, numFrames);
		if(frm < 0)
			return false;
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(frm);
		l->streamWrite(stream);
	}

	FORLIST(lnk, this->cameras){
		Camera *c = Camera::fromClump(lnk);
		int frm = findPointer(c->getFrame(), (void**)flist, numFrames);
		if(frm < 0)
			return false;
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(frm);
		c->streamWrite(stream);
	}

	delete[] flist;

	this->streamWritePlugins(stream);
	return true;
}

struct FrameStreamData
{
	V3d right, up, at, pos;
	int32 parent;
	int32 matflag;
};

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += f->streamGetPluginSize();
	f->forAllChildren(sizeCB, size);
	return f;
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
	int32 numFrames = this->getFrame()->count();
	size += 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB(this->getFrame(), (void*)&size);

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

	size += 12 + this->streamGetPluginSize();
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

bool
Clump::frameListStreamRead(Stream *stream, Frame ***flp, int32 *nf)
{
	FrameStreamData buf;
	int32 numFrames = 0;
	if(!findChunk(stream, ID_FRAMELIST, nil, nil)){
		RWERROR((ERR_CHUNK, "FRAMELIST"));
		return 0;
	}
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return 0;
	}
	numFrames = stream->readI32();
	Frame **frameList = new Frame*[numFrames];
	for(int32 i = 0; i < numFrames; i++){
		Frame *f;
		frameList[i] = f = Frame::create();
		stream->read(&buf, sizeof(buf));
		f->matrix.right = buf.right;
		f->matrix.rightw = 0.0f;
		f->matrix.up = buf.up;
		f->matrix.upw = 0.0f;
		f->matrix.at = buf.at;
		f->matrix.atw = 0.0f;
		f->matrix.pos = buf.pos;
		f->matrix.posw = 1.0f;
		//f->matflag = buf.matflag;
		if(buf.parent >= 0)
			frameList[buf.parent]->addChild(f);
	}
	for(int32 i = 0; i < numFrames; i++)
		frameList[i]->streamReadPlugins(stream);
	*nf = numFrames;
	*flp = frameList;
	return 1;
}

void
Clump::frameListStreamWrite(Stream *stream, Frame **frameList, int32 numFrames)
{
	FrameStreamData buf;

	int size = 0, structsize = 0;
	structsize = 4 + numFrames*sizeof(FrameStreamData);
	size += 12 + structsize;
	for(int32 i = 0; i < numFrames; i++)
		size += 12 + frameList[i]->streamGetPluginSize();

	writeChunkHeader(stream, ID_FRAMELIST, size);
	writeChunkHeader(stream, ID_STRUCT, structsize);
	stream->writeU32(numFrames);
	for(int32 i = 0; i < numFrames; i++){
		Frame *f = frameList[i];
		buf.right = f->matrix.right;
		buf.up = f->matrix.up;
		buf.at = f->matrix.at;
		buf.pos = f->matrix.pos;
		buf.parent = findPointer(f->getParent(), (void**)frameList,
		                         numFrames);
		buf.matflag = 0; //f->matflag;
		stream->write(&buf, sizeof(buf));
	}
	for(int32 i = 0; i < numFrames; i++)
		frameList[i]->streamWritePlugins(stream);
}

//
// Atomic
//

Atomic*
Atomic::create(void)
{
	Atomic *atomic = (Atomic*)malloc(PluginBase::s_size);
	if(atomic == nil){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return nil;
	}
	atomic->object.object.init(Atomic::ID, 0);
	atomic->geometry = nil;
	atomic->worldBoundingSphere.center.set(0.0f, 0.0f, 0.0f);
	atomic->worldBoundingSphere.radius = 0.0f;
	atomic->setFrame(nil);
	atomic->clump = nil;
	atomic->pipeline = nil;
	atomic->renderCB = Atomic::defaultRenderCB;
	atomic->object.object.flags = Atomic::COLLISIONTEST | Atomic::RENDER;
	atomic->constructPlugins();
	return atomic;
}

Atomic*
Atomic::clone()
{
	Atomic *atomic = Atomic::create();
	atomic->object.object.copy(&this->object.object);
	atomic->object.object.privateFlags |= 1;
	if(this->geometry){
		atomic->geometry = this->geometry;
		atomic->geometry->refCount++;
	}
	atomic->pipeline = this->pipeline;
	atomic->copyPlugins(this);
	return atomic;
}

void
Atomic::destroy(void)
{
	this->destructPlugins();
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

Sphere*
Atomic::getWorldBoundingSphere(void)
{
	Sphere *s = &this->worldBoundingSphere;
	if(!this->getFrame()->dirty() &&
	   (this->object.object.privateFlags & WORLDBOUNDDIRTY) == 0)
		return s;
	Matrix *ltm = this->getFrame()->getLTM();
	// TODO: support scaling
	// TODO: if we ever support morphing, fix this:
	s->center = ltm->transPoint(this->geometry->morphTargets[0].boundingSphere.center);
	s->radius = this->geometry->morphTargets[0].boundingSphere.radius;
	this->object.object.privateFlags &= ~WORLDBOUNDDIRTY;
	return s;
}

static uint32 atomicRights[2];

Atomic*
Atomic::streamReadClump(Stream *stream,
                        Frame **frameList, Geometry **geometryList)
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
	atomic->setFrame(frameList[buf[0]]);
	if(version < 0x30400){
		if(!findChunk(stream, ID_GEOMETRY, nil, nil)){
			RWERROR((ERR_CHUNK, "STRUCT"));
			// TODO: free
			return nil;
		}
		atomic->geometry = Geometry::streamRead(stream);
		if(atomic->geometry == nil){
			// TODO: free
			return nil;
		}
	}else
		atomic->geometry = geometryList[buf[1]];
	atomic->object.object.flags = buf[2];

	atomicRights[0] = 0;
	atomic->streamReadPlugins(stream);
	if(atomicRights[0])
		atomic->assertRights(atomicRights[0], atomicRights[1]);
	return atomic;
}

bool
Atomic::streamWriteClump(Stream *stream, Frame **frameList, int32 numFrames)
{
	int32 buf[4] = { 0, 0, 0, 0 };
	Clump *c = this->clump;
	if(c == nil)
		return false;
	writeChunkHeader(stream, ID_ATOMIC, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, rw::version < 0x30400 ? 12 : 16);
	buf[0] = findPointer(this->getFrame(), (void**)frameList, numFrames);

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

	this->streamWritePlugins(stream);
	return true;
}

uint32
Atomic::streamGetSize(void)
{
	uint32 size = 12 + 12 + 12 + this->streamGetPluginSize();
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
		engine[platform].defaultPipeline;
}

void
Atomic::defaultRenderCB(Atomic *atomic)
{
	atomic->getPipeline()->render(atomic);
}

// Atomic Rights plugin

static void
readAtomicRights(Stream *stream, int32, void *, int32, int32)
{
	stream->read(atomicRights, 8);
}

static void
writeAtomicRights(Stream *stream, int32, void *object, int32, int32)
{
	Atomic *atomic = (Atomic*)object;
	uint32 buffer[2];
	buffer[0] = atomic->pipeline->pluginID;
	buffer[1] = atomic->pipeline->pluginData;
	stream->write(buffer, 8);
}

static int32
getSizeAtomicRights(void *object, int32, int32)
{
	Atomic *atomic = (Atomic*)object;
	if(atomic->pipeline == nil || atomic->pipeline->pluginID == 0)
		return -1;
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
