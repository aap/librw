#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>

#include "rwbase.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"
#include "rwps2.h"
#include "rwxbox.h"
#include "rwd3d8.h"
#include "rwd3d9.h"
#include "rwwdgl.h"

using namespace std;

namespace rw {

LinkList Frame::dirtyList;

Frame*
Frame::create(void)
{
	Frame *f = (Frame*)malloc(PluginBase::s_size);
	assert(f != NULL);
	f->object.init(Frame::ID, 0);
	f->objectList.init();
	f->child = NULL;
	f->next = NULL;
	f->root = f;
	f->matrix.setIdentity();
	f->ltm.setIdentity();
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
Frame::addChild(Frame *child, bool32 append)
{
	Frame *c;
	if(child->getParent())
		child->removeChild();
	if(append){
		if(this->child == NULL)
			this->child = child;
		else{
			for(c = this->child; c->next; c = c->next);
			c->next = child;
		}
		child->next = NULL;
	}else{
		child->next = this->child;
		this->child = child;
	}
	child->object.parent = this;
	child->root = this->root;
	for(c = child->child; c; c = c->next)
		c->setHierarchyRoot(this);
	if(child->object.privateFlags & Frame::HIERARCHYSYNC){
		child->inDirtyList.remove();
		child->object.privateFlags &= ~Frame::HIERARCHYSYNC;
	}
	this->updateObjects();
	return this;
}

Frame*
Frame::removeChild(void)
{
	Frame *parent = this->getParent();
	Frame *child = parent->child;
	if(child == this)
		parent->child = this->next;
	else{
		while(child->next != this)
			child = child->next;
		child->next = this->next;
	}
	this->object.parent = this->next = NULL;
	this->root = this;
	for(child = this->child; child; child = child->next)
		child->setHierarchyRoot(this);
	this->updateObjects();
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

static void
syncRecurse(Frame *frame, uint8 flags)
{
	uint8 flg;
	for(; frame; frame = frame->next){
		flg = flags | frame->object.privateFlags;
		if(flg & Frame::SUBTREESYNCLTM){
			Matrix::mult(&frame->ltm, &frame->getParent()->ltm, &frame->matrix);
			frame->object.privateFlags &= ~Frame::SUBTREESYNCLTM;
		}
		syncRecurse(frame->child, flg);
	}
}

void
Frame::syncHierarchyLTM(void)
{
	Frame *child;
	uint8 flg;
	if(this->object.privateFlags & Frame::SUBTREESYNCLTM)
		this->ltm = this->matrix;
	for(child = this->child; child; child = child->next){
		flg = this->object.privateFlags | child->object.privateFlags;
		if(flg & Frame::SUBTREESYNCLTM){
			Matrix::mult(&child->ltm, &this->ltm, &child->matrix);
			child->object.privateFlags &= ~Frame::SUBTREESYNCLTM;
		}
		syncRecurse(child, flg);
	}
	this->object.privateFlags &= ~Frame::SYNCLTM;
}

Matrix*
Frame::getLTM(void)
{
	if(this->root->object.privateFlags & Frame::HIERARCHYSYNCLTM)
		this->root->syncHierarchyLTM();
	return &this->ltm;
}

void
Frame::updateObjects(void)
{
	if((this->root->object.privateFlags & HIERARCHYSYNC) == 0)
		Frame::dirtyList.add(&this->inDirtyList);
	this->root->object.privateFlags |= HIERARCHYSYNC;
	this->object.privateFlags |= SUBTREESYNC;
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
	frame->matrix = this->matrix;
	//memcpy(frame->matrix, this->matrix, sizeof(this->matrix));
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
	assert(clump != NULL);
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
	assert(findChunk(stream, ID_STRUCT, &length, &version));
	clump = Clump::create();
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

	// Cameras
	for(int32 i = 0; i < numCameras; i++){
		int32 frm;
		assert(findChunk(stream, ID_STRUCT, NULL, NULL));
		frm = stream->readI32();
		assert(findChunk(stream, ID_CAMERA, NULL, NULL));
		Camera *cam = Camera::streamRead(stream);
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
	assert(atomic != NULL);
	atomic->object.object.init(Atomic::ID, 0);
	atomic->geometry = NULL;
	atomic->worldBoundingSphere.center.set(0.0f, 0.0f, 0.0f);
	atomic->worldBoundingSphere.radius = 0.0f;
	atomic->setFrame(NULL);
	atomic->clump = NULL;
	atomic->pipeline = NULL;
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
	this->setFrame(NULL);
	free(this);
}

void
Atomic::removeFromClump(void)
{
	if(this->clump){
		this->inClump.remove();
		this->clump = NULL;
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
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(buf, version < 0x30400 ? 12 : 16);
	Atomic *atomic = Atomic::create();
	atomic->setFrame(frameList[buf[0]]);
	if(version < 0x30400){
		assert(findChunk(stream, ID_GEOMETRY, NULL, NULL));
		atomic->geometry = Geometry::streamRead(stream);
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
	if(c == NULL)
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
	assert(light != NULL);
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
	assert(findChunk(stream, ID_STRUCT, NULL, &version));
	stream->read(&buf, sizeof(LightChunkData));
	Light *light = Light::create(buf.type);
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

//
// Camera
//

Camera*
Camera::create(void)
{
	Camera *cam = (Camera*)malloc(PluginBase::s_size);
	cam->object.object.init(Camera::ID, 0);
	cam->viewWindow.set(1.0f, 1.0f);
	cam->viewOffset.set(0.0f, 0.0f);
	cam->nearPlane = 0.05f;
	cam->farPlane = 10.0f;
	cam->fogPlane = 5.0f;
	cam->projection = Camera::PERSPECTIVE;
	cam->clump = NULL;
	cam->inClump.init();
	cam->constructPlugins();
	return cam;
}

Camera*
Camera::clone(void)
{
	Camera *cam = Camera::create();
	cam->object.object.copy(&this->object.object);
	cam->setFrame(this->getFrame());
	cam->viewWindow = this->viewWindow;
	cam->viewOffset = this->viewOffset;
	cam->nearPlane = this->nearPlane;
	cam->farPlane = this->farPlane;
	cam->fogPlane = this->fogPlane;
	cam->projection = this->projection;
	cam->copyPlugins(this);
	return cam;
}

void
Camera::destroy(void)
{
	this->destructPlugins();
	if(this->clump)
		this->inClump.remove();
	free(this);
}

struct CameraChunkData
{
	V2d viewWindow;
	V2d viewOffset;
	float32 nearPlane, farPlane;
	float32 fogPlane;
	int32 projection;
};

Camera*
Camera::streamRead(Stream *stream)
{
	CameraChunkData buf;
	assert(findChunk(stream, ID_STRUCT, NULL, NULL));
	stream->read(&buf, sizeof(CameraChunkData));
	Camera *cam = Camera::create();
	cam->viewWindow = buf.viewWindow;
	cam->viewOffset = buf.viewOffset;
	cam->nearPlane = buf.nearPlane;
	cam->farPlane = buf.farPlane;
	cam->fogPlane = buf.fogPlane;
	cam->projection = buf.projection;
	cam->streamReadPlugins(stream);
	return cam;

}

bool
Camera::streamWrite(Stream *stream)
{
	CameraChunkData buf;
	writeChunkHeader(stream, ID_CAMERA, this->streamGetSize());
	writeChunkHeader(stream, ID_STRUCT, sizeof(CameraChunkData));
	buf.viewWindow = this->viewWindow;
	buf.viewOffset = this->viewOffset;
	buf.nearPlane = this->nearPlane;
	buf.farPlane  = this->farPlane;
	buf.fogPlane = this->fogPlane;
	buf.projection = this->projection;
	stream->write(&buf, sizeof(CameraChunkData));
	this->streamWritePlugins(stream);
	return true;
}

uint32
Camera::streamGetSize(void)
{
	return 12 + sizeof(CameraChunkData) + 12 + this->streamGetPluginSize();
}

void
Camera::updateProjectionMatrix(void)
{
	float32 invwx = 1.0f/this->viewWindow.x;
	float32 invwy = 1.0f/this->viewWindow.y;
	float32 invz = 1.0f/(this->farPlane-this->nearPlane);
	if(rw::platform == PLATFORM_D3D8 || rw::platform == PLATFORM_D3D9 ||
	   rw::platform == PLATFORM_XBOX){
		// is this all really correct?
		this->projMat[0] = invwx;
		this->projMat[1] = 0.0f;
		this->projMat[2] = 0.0f;
		this->projMat[3] = 0.0f;

		this->projMat[4] = 0.0f;
		this->projMat[5] = invwy;
		this->projMat[6] = 0.0f;
		this->projMat[7] = 0.0f;

		if(this->projection == PERSPECTIVE){
			this->projMat[8] = this->viewOffset.x*invwx;
			this->projMat[9] = this->viewOffset.y*invwy;
			this->projMat[10] = this->farPlane*invz;
			this->projMat[11] = 1.0f;

			this->projMat[12] = 0.0f;
			this->projMat[13] = 0.0f;
			this->projMat[14] = -this->nearPlane*this->projMat[10];
			this->projMat[15] = 0.0f;
		}else{
			this->projMat[8] = 0.0f;
			this->projMat[9] = 0.0f;
			this->projMat[10] = invz;
			this->projMat[11] = 0.0f;

			this->projMat[12] = this->viewOffset.x*invwx;
			this->projMat[13] = this->viewOffset.y*invwy;
			this->projMat[14] = -this->nearPlane*this->projMat[10];
			this->projMat[15] = 1.0f;
		}
	}
}

void
Camera::setFOV(float32 fov, float32 ratio)
{
	float32 a = tan(fov*3.14159f/360.0f);
	this->viewWindow.x = a;
	this->viewWindow.y = a/ratio;
	this->viewOffset.set(0.0f, 0.0f);
}

}
