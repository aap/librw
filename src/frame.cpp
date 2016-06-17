#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID 0

namespace rw {

LinkList Frame::dirtyList;

Frame*
Frame::create(void)
{
	Frame *f = (Frame*)malloc(PluginBase::s_size);
	if(f == NULL){
		RWERROR((ERR_ALLOC, PluginBase::s_size));
		return NULL;
	}
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
			Matrix::mult(&frame->ltm, &frame->getParent()->ltm,
			                          &frame->matrix);
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

struct FrameStreamData
{
	V3d right, up, at, pos;
	int32 parent;
	int32 matflag;
};

FrameList_*
FrameList_::streamRead(Stream *stream)
{
	FrameStreamData buf;
	this->numFrames = 0;
	this->frames = nil;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	this->numFrames = stream->readI32();
	Frame **frameList = (Frame**)malloc(this->numFrames*sizeof(Frame*));
	if(frameList == nil){
		RWERROR((ERR_ALLOC, this->numFrames*sizeof(Frame*)));
		return nil;
	}
	for(int32 i = 0; i < this->numFrames; i++){
		Frame *f;
		stream->read(&buf, sizeof(buf));
		frameList[i] = f = Frame::create();
		if(f == nil){
			// TODO: clean up frames?
			free(this->frames);
			return nil;
		}
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
	for(int32 i = 0; i < this->numFrames; i++)
		frameList[i]->streamReadPlugins(stream);
	return this;
}

void
FrameList_::streamWrite(Stream *stream)
{
	FrameStreamData buf;

	int size = 0, structsize = 0;
	structsize = 4 + this->numFrames*sizeof(FrameStreamData);
	size += 12 + structsize;
	for(int32 i = 0; i < this->numFrames; i++)
		size += 12 + this->frames[i]->streamGetPluginSize();

	writeChunkHeader(stream, ID_FRAMELIST, size);
	writeChunkHeader(stream, ID_STRUCT, structsize);
	stream->writeU32(this->numFrames);
	for(int32 i = 0; i < this->numFrames; i++){
		Frame *f = this->frames[i];
		buf.right = f->matrix.right;
		buf.up = f->matrix.up;
		buf.at = f->matrix.at;
		buf.pos = f->matrix.pos;
		buf.parent = findPointer(f->getParent(), (void**)this->frames,
		                         this->numFrames);
		buf.matflag = 0; //f->matflag;
		stream->write(&buf, sizeof(buf));
	}
	for(int32 i = 0; i < this->numFrames; i++)
		this->frames[i]->streamWritePlugins(stream);
}

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += f->streamGetPluginSize();
	f->forAllChildren(sizeCB, size);
	return f;
}

uint32
FrameList_::streamGetSize(Frame *f)
{
	int32 numFrames = f->count();
	uint32 size = 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB(f, (void*)&size);
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

}
