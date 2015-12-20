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

Frame::Frame(void)
{
	this->parent = NULL;
	this->child = NULL;
	this->next = NULL;
	this->root = NULL;
	for(int i = 0; i < 16; i++)
		this->matrix[i] = 0.0f;
	this->matrix[0] = 1.0f;
	this->matrix[5] = 1.0f;
	this->matrix[10] = 1.0f;
	this->matrix[15] = 1.0f;
	this->dirty = true;
	constructPlugins();
}

Frame::Frame(Frame *f)
{
	// TODO
	copyPlugins(f);
}

Frame::~Frame(void)
{
	destructPlugins();
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
	child->parent = this;
	child->root = this->root;
	return this;
}

Frame*
Frame::removeChild(void)
{
	Frame *parent = (Frame*)this->parent;
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
	this->parent = NULL;
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
	Frame *parent = (Frame*)this->parent;
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

Clump::Clump(void)
{
	this->numAtomics = 0;
	this->numLights = 0;
	this->numCameras = 0;
	this->atomicList = NULL;
	this->lightList = NULL;
	this->constructPlugins();
}

Clump::Clump(Clump *c)
{
	this->numAtomics = c->numAtomics;
	this->numLights = c->numLights;
	this->numCameras = c->numCameras;
	// TODO: atomics and lights
	this->copyPlugins(c);
}

Clump::~Clump(void)
{
	this->destructPlugins();
}

Clump*
Clump::streamRead(Stream *stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;
	assert(findChunk(stream, ID_STRUCT, &length, &version));
	clump = new Clump;
	stream->read(buf, length);
	clump->numAtomics = buf[0];
	clump->numLights = 0;
	clump->numCameras = 0;
	if(version > 0x33000){
		clump->numLights = buf[1];
		clump->numCameras = buf[2];
	}

	// Frame list
	Frame **frameList;
	int32 numFrames;
	clump->frameListStreamRead(stream, &frameList, &numFrames);
	clump->parent = (void*)frameList[0];

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
	if(clump->numAtomics)
		clump->atomicList = new Atomic*[clump->numAtomics];
	for(int32 i = 0; i < clump->numAtomics; i++){
		assert(findChunk(stream, ID_ATOMIC, NULL, NULL));
		clump->atomicList[i] = Atomic::streamReadClump(stream,
			frameList, geometryList);
		clump->atomicList[i]->clump = clump;
	}

	// Lights
	if(clump->numLights)
		clump->lightList = new Light*[clump->numLights];
	for(int32 i = 0; i < clump->numLights; i++){
		int32 frm;
		assert(findChunk(stream, ID_STRUCT, NULL, NULL));
		frm = stream->readI32();
		assert(findChunk(stream, ID_LIGHT, NULL, NULL));
		clump->lightList[i] = Light::streamRead(stream);
		clump->lightList[i]->frame = frameList[frm];
		clump->lightList[i]->clump = clump;
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
	int buf[3] = { this->numAtomics, this->numLights, this->numCameras };
	size = version > 0x33000 ? 12 : 4;
	writeChunkHeader(stream, ID_STRUCT, size);
	stream->write(buf, size);

	int32 numFrames = ((Frame*)this->parent)->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList((Frame*)this->parent, flist);

	this->frameListStreamWrite(stream, flist, numFrames);

	if(rw::version >= 0x30400){
		size = 12+4;
		for(int32 i = 0; i < this->numAtomics; i++)
			size += 12 +
			        this->atomicList[i]->geometry->streamGetSize();
		writeChunkHeader(stream, ID_GEOMETRYLIST, size);
		writeChunkHeader(stream, ID_STRUCT, 4);
		stream->writeI32(this->numAtomics);	// same as numGeometries
		for(int32 i = 0; i < this->numAtomics; i++)
			this->atomicList[i]->geometry->streamWrite(stream);
	}

	for(int32 i = 0; i < this->numAtomics; i++)
		this->atomicList[i]->streamWriteClump(stream, flist, numFrames);

	for(int32 i = 0; i < this->numLights; i++){
		Light *l = this->lightList[i];
		int frm = findPointer((void*)l->frame, (void**)flist,numFrames);
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
	int32 numFrames = ((Frame*)this->parent)->count();
	size += 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB((Frame*)this->parent, (void*)&size);

	if(rw::version >= 0x30400){
		// geometry list
		size += 12 + 12 + 4;
		for(int32 i = 0; i < this->numAtomics; i++)
			size += 12 +
			        this->atomicList[i]->geometry->streamGetSize();
	}

	// atomics
	for(int32 i = 0; i < this->numAtomics; i++)
		size += 12 + this->atomicList[i]->streamGetSize();

	// light
	for(int32 i = 0; i < this->numLights; i++)
		size += 16 + 12 + this->lightList[i]->streamGetSize();

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
		frameList[i] = f = new Frame;
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
		buf.parent = findPointer((void*)f->parent, (void**)frameList,
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

Atomic::Atomic(void)
{
	this->frame = NULL;
	this->geometry = NULL;
	this->pipeline = NULL;
	constructPlugins();
}

Atomic::Atomic(Atomic *a)
{
	//TODO
	copyPlugins(a);
}

Atomic::~Atomic(void)
{
	//TODO
	destructPlugins();
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
	Atomic *atomic = new Atomic;
	atomic->frame = frameList[buf[0]];
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
	buf[0] = findPointer((void*)this->frame, (void**)frameList, numFrames);

	if(version < 0x30400){
		stream->write(buf, sizeof(int[3]));
		this->geometry->streamWrite(stream);
	}else{
		// TODO
		for(buf[1] = 0; buf[1] < c->numAtomics; buf[1]++)
			if(c->atomicList[buf[1]]->geometry == this->geometry)
				goto foundgeo;
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

Light::Light(void)
{
	this->frame = NULL;
	constructPlugins();
}

Light::Light(Light *l)
{
	// TODO
	copyPlugins(l);
}

Light::~Light(void)
{
	destructPlugins();
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
	Light *light = new Light;
	stream->read(&buf, sizeof(LightChunkData));
	light->radius = buf.radius;
	light->color[0] = buf.red;
	light->color[1] = buf.green;
	light->color[2] = buf.blue;
	light->color[3] = 1.0f;
	light->minusCosAngle = buf.minusCosAngle;
	light->flags = (uint8)buf.flags;
	light->subType = (uint8)buf.type;

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
	buf.flags = this->flags;
	buf.type = this->subType;
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
