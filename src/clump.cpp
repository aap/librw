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
#include "rwogl.h"
#include "rwxbox.h"
#include "rwd3d8.h"
#include "rwd3d9.h"

using namespace std;

namespace rw {

Frame*
Frame::create(void)
{
	Frame *f = (Frame*)malloc(PluginBase::s_size);
	f->object.init(0, 0);
	f->objectList.init();
	f->child = NULL;
	f->next = NULL;
	f->root = NULL;
	for(int i = 0; i < 16; i++)
		f->matrix[i] = 0.0f;
	f->matrix[0] = 1.0f;
	f->matrix[5] = 1.0f;
	f->matrix[10] = 1.0f;
	f->matrix[15] = 1.0f;
	f->matflag = 0;
	f->dirty = true;
	f->constructPlugins();
	return f;
}

Frame*
Frame::cloneHierarchy(void)
{
	Frame *frame = this->cloneAndLink(NULL);
	frame->purgeClone();
	return frame;
}

void
Frame::destroy(void)
{
	this->destructPlugins();
	Frame *parent = this->getParent();
	Frame *child;
	if(parent){
		// remove from child list
		child = parent->child;
		if(child == this)
			parent->child = this->next;
		else{
			for(child = child->next; child != this; child = child->next)
				;
			child->next = this->next;
		}
		this->object.parent = NULL;
		// Doesn't seem to make much sense, blame criterion.
		this->setHierarchyRoot(this);
	}
	for(Frame *f = this->child; f; f = f->next)
		f->object.parent = NULL;
	free(this);
}

void
Frame::destroyHierarchy(void)
{
	Frame *next;
	for(Frame *child = this->child; child; child = next){
		next = child->next;
		child->destroyHierarchy();
	}
	this->destructPlugins();
	free(this);
}

Frame*
Frame::addChild(Frame *child)
{
	if(this->child == NULL)
		this->child = child;
	else{
		Frame *f;
		for(f = this->child; f->next; f = f->next);
		f->next = child;
	}
	child->next = NULL;
	child->object.parent = this;
	child->root = this->root;
	return this;
}

Frame*
Frame::removeChild(void)
{
	Frame *parent = (Frame*)this->object.parent;
	if(parent->child == this)
		parent->child = this->next;
	else{
		Frame *f;
		for(f = parent->child; f; f = f->next)
			if(f->next == this)
				goto found;
		// not found
found:
		f->next = f->next->next;
	}
	this->object.parent = NULL;
	this->next = this->root = NULL;
	return this;
}

Frame*
Frame::forAllChildren(Callback cb, void *data)
{
	for(Frame *f = this->child; f; f = f->next)
		cb(f, data);
	return this;
}

static Frame*
countCB(Frame *f, void *count)
{
	(*(int32*)count)++;
	f->forAllChildren(countCB, count);
	return f;
}

int32
Frame::count(void)
{
	int32 count = 1;
	this->forAllChildren(countCB, (void*)&count);
	return count;
}

void
Frame::updateLTM(void)
{
	if(!this->dirty)
		return;
	Frame *parent = (Frame*)this->object.parent;
	if(parent){
		parent->updateLTM();
		matrixMult(this->ltm, parent->ltm, this->matrix);
		this->dirty = false;
	}else{
		memcpy(this->ltm, this->matrix, 16*4);
		this->dirty = false;
	}
}

static Frame*
dirtyCB(Frame *f, void *)
{
	f->dirty = true;
	f->forAllChildren(dirtyCB, NULL);
	return f;
}

void
Frame::setDirty(void)
{
	this->dirty = true;
	this->forAllChildren(dirtyCB, NULL);
}

void
Frame::setHierarchyRoot(Frame *root)
{
	this->root = root;
	for(Frame *child = this->child; child; child = child->next)
		child->setHierarchyRoot(root);
}

// Clone a frame hierarchy. Link cloned frames into Frame::root of the originals.
Frame*
Frame::cloneAndLink(Frame *clonedroot)
{
	Frame *frame = Frame::create();
	if(clonedroot == NULL)
		clonedroot = frame;
	frame->object.copy(&this->object);
	memcpy(frame->matrix, this->matrix, sizeof(this->matrix));
	frame->root = clonedroot;
	this->root = frame;	// Remember cloned frame
	for(Frame *child = this->child; child; child = child->next){
		Frame *clonedchild = child->cloneAndLink(clonedroot);
		clonedchild->next = frame->child;
		frame->child = clonedchild;
		clonedchild->object.parent = frame;
	}
	frame->copyPlugins(this);
	return frame;
}

// Remove links to cloned frames from hierarchy.
void
Frame::purgeClone(void)
{
	Frame *parent = this->getParent();
	this->setHierarchyRoot(parent ? parent->root : this);
}

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += f->streamGetPluginSize();
	f->forAllChildren(sizeCB, size);
	return f;
}

Frame**
makeFrameList(Frame *frame, Frame **flist)
{
	*flist++ = frame;
	if(frame->next)
		flist = makeFrameList(frame->next, flist);
	if(frame->child)
		flist = makeFrameList(frame->child, flist);
	return flist;
}

//
// Clump
//

Clump*
Clump::create(void)
{
	Clump *clump = (Clump*)malloc(PluginBase::s_size);
	clump->object.init(2, 0);
	clump->atomics.init();
	clump->lights.init();
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
	if(f = this->getFrame())
		f->destroyHierarchy();
	free(this);
}

int32
Clump::countAtomics(void)
{
	int32 n = 0;
	FORLIST(l, this->atomics)
		n++;
	return n;
}

int32
Clump::countLights(void)
{
	int32 n = 0;
	FORLIST(l, this->lights)
		n++;
	return n;
}

Clump*
Clump::streamRead(Stream *stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;
	assert(findChunk(stream, ID_STRUCT, &length, &version));
	clump = Clump::create();
	stream->read(buf, length);
	int32 numAtomics = buf[0];
	int32 numLights = 0;
	if(version > 0x33000)
		numLights = buf[1];
	// ignore cameras

	// Frame list
	Frame **frameList;
	int32 numFrames;
	clump->frameListStreamRead(stream, &frameList, &numFrames);
	clump->object.parent = (void*)frameList[0];

	Geometry **geometryList = 0;
	if(version >= 0x30400){
		// Geometry list
		int32 numGeometries = 0;
		assert(findChunk(stream, ID_GEOMETRYLIST, NULL, NULL));
		assert(findChunk(stream, ID_STRUCT, NULL, NULL));
		numGeometries = stream->readI32();
		if(numGeometries)
			geometryList = new Geometry*[numGeometries];
		for(int32 i = 0; i < numGeometries; i++){
			assert(findChunk(stream, ID_GEOMETRY, NULL, NULL));
			geometryList[i] = Geometry::streamRead(stream);
		}
	}

	// Atomics
	for(int32 i = 0; i < numAtomics; i++){
		assert(findChunk(stream, ID_ATOMIC, NULL, NULL));
		Atomic *a = Atomic::streamReadClump(stream, frameList, geometryList);
		clump->addAtomic(a);
	}

	// Lights
	for(int32 i = 0; i < numLights; i++){
		int32 frm;
		assert(findChunk(stream, ID_STRUCT, NULL, NULL));
		frm = stream->readI32();
		assert(findChunk(stream, ID_LIGHT, NULL, NULL));
		Light *l = Light::streamRead(stream);
		l->setFrame(frameList[frm]);
		clump->addLight(l);
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
	int buf[3] = { numAtomics, numLights, 0 };
	size = version > 0x33000 ? 12 : 4;
	writeChunkHeader(stream, ID_STRUCT, size);
	stream->write(buf, size);

	int32 numFrames = ((Frame*)this->object.parent)->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList((Frame*)this->object.parent, flist);

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
		int frm = findPointer((void*)l->object.parent, (void**)flist, numFrames);
		if(frm < 0)
			return false;
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(frm);
		l->streamWrite(stream);
	}

	delete[] flist;

	this->streamWritePlugins(stream);
	return true;
}

struct FrameStreamData
{
	float32 mat[9];
	float32 pos[3];
	int32 parent;
	int32 matflag;
};

uint32
Clump::streamGetSize(void)
{
	uint32 size = 0;
	size += 12;	// Struct
	size += 4;	// numAtomics
	if(version > 0x33000)
		size += 8;	// numLights, numCameras

	// frame list
	int32 numFrames = ((Frame*)this->object.parent)->count();
	size += 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB((Frame*)this->object.parent, (void*)&size);

	if(rw::version >= 0x30400){
		// geometry list
		size += 12 + 12 + 4;
		FORLIST(lnk, this->atomics)
			size += 12 + Atomic::fromClump(lnk)->geometry->streamGetSize();
	}

	// atomics
	FORLIST(lnk, this->atomics)
		size += 12 + Atomic::fromClump(lnk)->streamGetSize();

	// light
	FORLIST(lnk, this->lights)
		size += 16 + 12 + Light::fromClump(lnk)->streamGetSize();

	size += 12 + this->streamGetPluginSize();
	return size;
}


void
Clump::frameListStreamRead(Stream *stream, Frame ***flp, int32 *nf)
{
	FrameStreamData buf;
	int32 numFrames = 0;
	assert(findChunk(stream, ID_FRAMELIST, NULL, NULL));
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	numFrames = stream->readI32();
	Frame **frameList = new Frame*[numFrames];
	for(int32 i = 0; i < numFrames; i++){
		Frame *f;
		frameList[i] = f = Frame::create();
		stream->read(&buf, sizeof(buf));
		f->matrix[0] = buf.mat[0];
		f->matrix[1] = buf.mat[1];
		f->matrix[2] = buf.mat[2];
		f->matrix[3] = 0.0f;
		f->matrix[4] = buf.mat[3];
		f->matrix[5] = buf.mat[4];
		f->matrix[6] = buf.mat[5];
		f->matrix[7] = 0.0f;
		f->matrix[8] = buf.mat[6];
		f->matrix[9] = buf.mat[7];
		f->matrix[10] = buf.mat[8];
		f->matrix[11] = 0.0f;
		f->matrix[12] = buf.pos[0];
		f->matrix[13] = buf.pos[1];
		f->matrix[14] = buf.pos[2];
		f->matrix[15] = 1.0f;
		f->matflag = buf.matflag;
		if(buf.parent >= 0)
			frameList[buf.parent]->addChild(f);
	}
	for(int32 i = 0; i < numFrames; i++)
		frameList[i]->streamReadPlugins(stream);
	*nf = numFrames;
	*flp = frameList;
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
		buf.mat[0] = f->matrix[0];
		buf.mat[1] = f->matrix[1];
		buf.mat[2] = f->matrix[2];
		buf.mat[3] = f->matrix[4];
		buf.mat[4] = f->matrix[5];
		buf.mat[5] = f->matrix[6];
		buf.mat[6] = f->matrix[8];
		buf.mat[7] = f->matrix[9];
		buf.mat[8] = f->matrix[10];
		buf.pos[0] = f->matrix[12];
		buf.pos[1] = f->matrix[13];
		buf.pos[2] = f->matrix[14];
		buf.parent = findPointer((void*)f->object.parent, (void**)frameList,
		                         numFrames);
		buf.matflag = f->matflag;
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
	atomic->object.init(1, 0);
	atomic->geometry = NULL;
	atomic->pipeline = NULL;
	atomic->constructPlugins();
	return atomic;
}

Atomic*
Atomic::clone()
{
	Atomic *atomic = Atomic::create();
	atomic->object.copy(&this->object);
	atomic->object.privateFlags |= 1;
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
	this->setFrame(NULL);
	free(this);
}

static uint32 atomicRights[2];

Atomic*
Atomic::streamReadClump(Stream *stream,
                        Frame **frameList, Geometry **geometryList)
{
	int32 buf[4];
	uint32 version;
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(buf, version < 0x30400 ? 12 : 16);
	Atomic *atomic = Atomic::create();
	atomic->setFrame(frameList[buf[0]]);
	if(version < 0x30400){
		assert(findChunk(stream, ID_GEOMETRY, NULL, NULL));
		atomic->geometry = Geometry::streamRead(stream);
	}else
		atomic->geometry = geometryList[buf[1]];

	atomicRights[0] = 0;
	atomic->streamReadPlugins(stream);
	if(atomicRights[0])
		atomic->assertRights(atomicRights[0], atomicRights[1]);
	return atomic;
}

bool
Atomic::streamWriteClump(Stream *stream, Frame **frameList, int32 numFrames)
{
	int32 buf[4] = { 0, 0, 5, 0 };
	Clump *c = this->clump;
	if(c == NULL)
		return false;
	writeChunkHeader(stream, ID_ATOMIC, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, rw::version < 0x30400 ? 12 : 16);
	buf[0] = findPointer((void*)this->object.parent, (void**)frameList, numFrames);

	if(version < 0x30400){
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

ObjPipeline *defaultPipelines[NUM_PLATFORMS];

ObjPipeline*
Atomic::getPipeline(void)
{
	return this->pipeline ?
		this->pipeline :
		defaultPipelines[platform];
}

void
Atomic::init(void)
{
	ObjPipeline *defpipe = new ObjPipeline(PLATFORM_NULL);
	for(uint i = 0; i < nelem(matFXGlobals.pipelines); i++)
		defaultPipelines[i] = defpipe;
	defaultPipelines[PLATFORM_PS2] =
		ps2::makeDefaultPipeline();
	defaultPipelines[PLATFORM_OGL] =
		gl::makeDefaultPipeline();
	defaultPipelines[PLATFORM_XBOX] =
		xbox::makeDefaultPipeline();
	defaultPipelines[PLATFORM_D3D8] =
		d3d8::makeDefaultPipeline();
	defaultPipelines[PLATFORM_D3D9] =
		d3d9::makeDefaultPipeline();
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
	if(atomic->pipeline == NULL || atomic->pipeline->pluginID == 0)
		return -1;
	return 8;
}

void
registerAtomicRightsPlugin(void)
{
	Atomic::registerPlugin(0, ID_RIGHTTORENDER, NULL, NULL, NULL);
	Atomic::registerPluginStream(ID_RIGHTTORENDER,
	                             readAtomicRights,
	                             writeAtomicRights,
	                             getSizeAtomicRights);
}


//
// Light
//

Light*
Light::create(int32 type)
{
	Light *light = (Light*)malloc(PluginBase::s_size);
	light->object.init(3, type);
	light->radius = 0.0f;
	light->color[0] = 1.0f;
	light->color[1] = 1.0f;
	light->color[2] = 1.0f;
	light->color[3] = 1.0f;
	light->minusCosAngle = 1.0f;
	light->object.privateFlags = 1;
	light->object.flags = 1 | 2;
	light->inClump.init();
	light->constructPlugins();
	return light;
}

void
Light::destroy(void)
{
	this->destructPlugins();
	free(this);
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
	LightChunkData buf;
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	stream->read(&buf, sizeof(LightChunkData));
	Light *light = Light::create(buf.type);
	light->radius = buf.radius;
	light->color[0] = buf.red;
	light->color[1] = buf.green;
	light->color[2] = buf.blue;
	light->color[3] = 1.0f;
	light->minusCosAngle = buf.minusCosAngle;
	light->object.flags = (uint8)buf.flags;

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
	buf.red   = this->color[0];
	buf.green = this->color[1];
	buf.blue  = this->color[2];
	buf.minusCosAngle = this->minusCosAngle;
	buf.flags = this->object.flags;
	buf.type = this->object.subType;
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
