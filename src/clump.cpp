#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"
#include "rwobjects.h"

using namespace std;

namespace Rw {

Frame::Frame(void)
{
	this->child = NULL;
	this->next = NULL;
	this->root = NULL;
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

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += f->streamGetPluginSize();
	f->forAllChildren(sizeCB, size);
	return f;
}

static Frame**
makeFrameList(Frame *frame, Frame **flist)
{
	*flist++ = frame;
	if(frame->next)
		flist = makeFrameList(frame->next, flist);
	if(frame->child)
		flist = makeFrameList(frame->child, flist);
	return flist;
}



Clump::Clump(void)
{
	this->numAtomics = 0;
	this->numLights = 0;
	this->numCameras = 0;
	constructPlugins();
}

Clump::Clump(Clump *c)
{
	this->numAtomics = c->numAtomics;
	this->numLights = c->numLights;
	this->numCameras = c->numCameras;
	copyPlugins(c);
}

Clump::~Clump(void)
{
	destructPlugins();
}

Clump*
Clump::streamRead(istream &stream)
{
	uint32 length, version;
	int32 buf[3];
	Clump *clump;
	assert(FindChunk(stream, ID_STRUCT, &length, &version));
	clump = new Clump;
	stream.read((char*)buf, length);
	clump->numAtomics = buf[0];
	clump->numLights = 0;
	clump->numCameras = 0;
	if(version >= 0x33000){
		clump->numLights = buf[1];
		clump->numCameras = buf[2];
	}

	// Frame list
	Frame **frameList;
	int32 numFrames;
	clump->frameListStreamRead(stream, &frameList, &numFrames);
	clump->parent = (void*)frameList[0];

	// Geometry list
	int32 numGeometries = 0;
	assert(FindChunk(stream, ID_GEOMETRYLIST, NULL, NULL));
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	numGeometries = readInt32(stream);
	Geometry **geometryList = new Geometry*[numGeometries];
	for(int32 i = 0; i < numGeometries; i++){
		assert(FindChunk(stream, ID_GEOMETRY, NULL, NULL));
		geometryList[i] = Geometry::streamRead(stream);
	}

	// Atomics
	clump->atomicList = new Atomic*[clump->numAtomics];
	for(int32 i = 0; i < clump->numAtomics; i++){
		assert(FindChunk(stream, ID_ATOMIC, NULL, NULL));
		clump->atomicList[i] = Atomic::streamReadClump(stream,
			frameList, geometryList);
		clump->atomicList[i]->clump = clump;
	}

	// Lights
	clump->lightList = new Light*[clump->numLights];
	for(int32 i = 0; i < clump->numLights; i++){
		int32 frm;
		assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
		frm = readInt32(stream);
		assert(FindChunk(stream, ID_LIGHT, NULL, NULL));
		clump->lightList[i] = Light::streamRead(stream);
		clump->lightList[i]->frame = frameList[frm];
		clump->lightList[i]->clump = clump;
	}

	delete[] frameList;

	clump->streamReadPlugins(stream);
	return clump;
}

bool
Clump::streamWrite(ostream &stream)
{
	int size = this->streamGetSize();
	WriteChunkHeader(stream, ID_CLUMP, size);
	int buf[3] = { this->numAtomics, this->numLights, this->numCameras };
	size = Version >= 0x33000 ? 12 : 4;
	WriteChunkHeader(stream, ID_STRUCT, size);
	stream.write((char*)buf, size);

	int32 numFrames = ((Frame*)this->parent)->count();
	Frame **flist = new Frame*[numFrames];
	makeFrameList((Frame*)this->parent, flist);

	this->frameListStreamWrite(stream, flist, numFrames);

	size = 12+4;
	for(int32 i = 0; i < this->numAtomics; i++)
		size += 12 + this->atomicList[i]->geometry->streamGetSize();
	WriteChunkHeader(stream, ID_GEOMETRYLIST, size);
	WriteChunkHeader(stream, ID_STRUCT, 4);
	writeInt32(this->numAtomics, stream);	// same as numGeometries
	for(int32 i = 0; i < this->numAtomics; i++)
		this->atomicList[i]->geometry->streamWrite(stream);

	for(int32 i = 0; i < this->numAtomics; i++)
		this->atomicList[i]->streamWriteClump(stream, flist, numFrames);

	for(int32 i = 0; i < this->numLights; i++){
		Light *l = this->lightList[i];
		int frm = findPointer((void*)l->frame, (void**)flist,numFrames);
		if(frm < 0)
			return false;
		WriteChunkHeader(stream, ID_STRUCT, 4);
		writeInt32(frm, stream);
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
	if(Version >= 0x33000)
		size += 8;	// numLights, numCameras

	// frame list
	int32 numFrames = ((Frame*)this->parent)->count();
	size += 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB((Frame*)this->parent, (void*)&size);

	// geometry list
	size += 12 + 12 + 4;
	for(int32 i = 0; i < this->numAtomics; i++)
		size += 12 + this->atomicList[i]->geometry->streamGetSize();

	// atomics
	for(int32 i = 0; i < this->numAtomics; i++)
		size += 12 + this->atomicList[i]->streamGetSize();

	// light
	for(int32 i = 0; i < this->numAtomics; i++)
		size += 16 + 12 + this->lightList[i]->streamGetSize();

	size += 12 + this->streamGetPluginSize();
	return size;
}


void
Clump::frameListStreamRead(istream &stream, Frame ***flp, int32 *nf)
{
	FrameStreamData buf;
	int32 numFrames = 0;
	assert(FindChunk(stream, ID_FRAMELIST, NULL, NULL));
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	numFrames = readInt32(stream);
	Frame **frameList = new Frame*[numFrames];
	for(int32 i = 0; i < numFrames; i++){
		Frame *f;
		frameList[i] = f = new Frame;
		stream.read((char*)&buf, sizeof(buf));
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
		if(buf.parent >= 0)
			frameList[buf.parent]->addChild(f);
		f->matflag = buf.matflag;
	}
	for(int32 i = 0; i < numFrames; i++)
		frameList[i]->streamReadPlugins(stream);
	*nf = numFrames;
	*flp = frameList;
}

void
Clump::frameListStreamWrite(ostream &stream, Frame **frameList, int32 numFrames)
{
	FrameStreamData buf;

	int size = 0, structsize = 0;
	structsize = 4 + numFrames*sizeof(FrameStreamData);
	size += 12 + structsize;
	for(int32 i = 0; i < numFrames; i++)
		size += 12 + frameList[i]->streamGetPluginSize();

	WriteChunkHeader(stream, ID_FRAMELIST, size);
	WriteChunkHeader(stream, ID_STRUCT, structsize);
	writeUInt32(numFrames, stream);
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
		buf.parent = findPointer((void*)f, (void**)frameList,numFrames);
		buf.matflag = f->matflag;
		stream.write((char*)&buf, sizeof(buf));
	}
	for(int32 i = 0; i < numFrames; i++)
		frameList[i]->streamWritePlugins(stream);
}


Atomic::Atomic(void)
{
	this->frame = NULL;
	this->geometry = NULL;
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

Atomic*
Atomic::streamReadClump(istream &stream,
                        Frame **frameList, Geometry **geometryList)
{
	int32 buf[4];
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	stream.read((char*)buf, 16);
	Atomic *atomic = new Atomic;
	atomic->frame = frameList[buf[0]];
	atomic->geometry = geometryList[buf[1]];
	atomic->streamReadPlugins(stream);
	return atomic;
}

bool
Atomic::streamWriteClump(ostream &stream, Frame **frameList, int32 numFrames)
{
	int32 buf[4] = { 0, 0, 5, 0 };
	Clump *c = this->clump;
	if(c == NULL)
		return false;
	WriteChunkHeader(stream, ID_ATOMIC, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, 16);
	buf[0] = findPointer((void*)this->frame, (void**)frameList, numFrames);

// TODO
	for(buf[1] = 0; buf[1] < c->numAtomics; buf[1]++)
		if(c->atomicList[buf[1]]->geometry == this->geometry)
			goto foundgeo;
	return false;
foundgeo:

	stream.write((char*)buf, sizeof(buf));
	this->streamWritePlugins(stream);
	return true;
}

uint32
Atomic::streamGetSize(void)
{
	return 12 + 16 + 12 + this->streamGetPluginSize();
}


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
Light::streamRead(istream &stream)
{
	LightChunkData buf;
	assert(FindChunk(stream, ID_STRUCT, NULL, NULL));
	Light *light = new Light;
	stream.read((char*)&buf, sizeof(LightChunkData));
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
Light::streamWrite(ostream &stream)
{
	LightChunkData buf;
	WriteChunkHeader(stream, ID_LIGHT, this->streamGetSize());
	WriteChunkHeader(stream, ID_STRUCT, sizeof(LightChunkData));
	buf.radius = this->radius;
	buf.red   = this->color[0];
	buf.green = this->color[1];
	buf.blue  = this->color[2];
	buf.minusCosAngle = this->minusCosAngle;
	buf.flags = this->flags;
	buf.type = this->subType;
	stream.write((char*)&buf, sizeof(LightChunkData));

	this->streamWritePlugins(stream);
	return true;
}

uint32
Light::streamGetSize(void)
{
	return 12 + sizeof(LightChunkData) + 12 + this->streamGetPluginSize();
}

}
