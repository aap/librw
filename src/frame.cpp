#include <cstdio>
#include <cstdlib>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"

#define PLUGIN_ID 0

namespace rw {

LinkList Frame::dirtyList;

Frame*
Frame::create(void)
{
	Frame *f = (Frame*)malloc(s_plglist.size);
	if(f == nil){
		RWERROR((ERR_ALLOC, s_plglist.size));
		return nil;
	}
	f->object.init(Frame::ID, 0);
	f->objectList.init();
	f->child = nil;
	f->next = nil;
	f->root = f;
	f->matrix.setIdentity();
	f->ltm.setIdentity();
	s_plglist.construct(f);
	return f;
}

Frame*
Frame::cloneHierarchy(void)
{
	Frame *frame = this->cloneAndLink(nil);
	frame->purgeClone();
	return frame;
}

void
Frame::destroy(void)
{
	s_plglist.destruct(this);
	if(this->getParent())
		this->removeChild();
	if(this->object.privateFlags & Frame::HIERARCHYSYNC)
		this->inDirtyList.remove();
	for(Frame *f = this->child; f; f = f->next)
		f->object.parent = nil;
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
	s_plglist.destruct(this);
	if(this->object.privateFlags & Frame::HIERARCHYSYNC)
		this->inDirtyList.remove();
	free(this);
}

Frame*
Frame::addChild(Frame *child, bool32 append)
{
	Frame *c;
	if(child->getParent())
		child->removeChild();
	if(append){
		if(this->child == nil)
			this->child = child;
		else{
			for(c = this->child; c->next; c = c->next);
			c->next = child;
		}
		child->next = nil;
	}else{
		child->next = this->child;
		this->child = child;
	}
	child->object.parent = this;
	child->root = this->root;
	for(c = child->child; c; c = c->next)
		c->setHierarchyRoot(this);
	// If the child was a root, remove from dirty list
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
	this->object.parent = this->next = nil;
	// give the hierarchy a new root
	this->setHierarchyRoot(this);
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

/*
 * Synching is a bit complicated. If anything in the hierarchy is not synched,
 * the root of the hierarchy is marked with the HIERARCHYSYNC flags.
 * Every unsynched frame is marked with the SUBTREESYNC flags.
 * If the LTM is not synched, the LTM flags are set.
 * If attached objects need synching, the OBJ flags are set.
 */

/* Synch just LTM matrices in a hierarchy */
static void
syncLTMRecurse(Frame *frame, uint8 hierarchyFlags)
{
	for(; frame; frame = frame->next){
		// If frame is dirty or any parent was dirty, update LTM
		hierarchyFlags |= frame->object.privateFlags;
		if(hierarchyFlags & Frame::SUBTREESYNCLTM){
			Matrix::mult(&frame->ltm, &frame->getParent()->ltm,
			                          &frame->matrix);
			frame->object.privateFlags &= ~Frame::SUBTREESYNCLTM;
		}
		// And synch all children
		syncLTMRecurse(frame->child, hierarchyFlags);
	}
}

/* Synch just objects in a hierarchy */
static void
syncObjRecurse(Frame *frame)
{
	for(; frame; frame = frame->next){
		// Synch attached objects
		FORLIST(lnk, frame->objectList)
			ObjectWithFrame::fromFrame(lnk)->sync();
		frame->object.privateFlags &= ~Frame::SUBTREESYNCOBJ;
		// And synch all children
		syncObjRecurse(frame->child);
	}
}

/* Synch LTM and objects */
static void
syncRecurse(Frame *frame, uint8 hierarchyFlags)
{
	for(; frame; frame = frame->next){
		// If frame is dirty or any parent was dirty, update LTM
		hierarchyFlags |= frame->object.privateFlags;
		if(hierarchyFlags & Frame::SUBTREESYNCLTM)
			Matrix::mult(&frame->ltm, &frame->getParent()->ltm,
			                          &frame->matrix);
		// Synch attached objects
		FORLIST(lnk, frame->objectList)
			ObjectWithFrame::fromFrame(lnk)->sync();
		frame->object.privateFlags &= ~Frame::SUBTREESYNC;
		// And synch all children
		syncRecurse(frame->child, hierarchyFlags);
	}
}

/* Sync the LTMs of the hierarchy of which 'this' is the root */
void
Frame::syncHierarchyLTM(void)
{
	// Sync root's LTM
	if(this->object.privateFlags & Frame::SUBTREESYNCLTM)
		this->ltm = this->matrix;
	// ...and children
	syncLTMRecurse(this->child, this->object.privateFlags);
	// all clean now
	this->object.privateFlags &= ~Frame::SYNCLTM;
}

Matrix*
Frame::getLTM(void)
{
	if(this->root->object.privateFlags & Frame::HIERARCHYSYNCLTM)
		this->root->syncHierarchyLTM();
	return &this->ltm;
}

/* Synch all dirty frames; LTMs and objects */
void
Frame::syncDirty(void)
{
	Frame *frame;
	FORLIST(lnk, Frame::dirtyList){
		frame = LLLinkGetData(lnk, Frame, inDirtyList);
		if(frame->object.privateFlags & Frame::HIERARCHYSYNCLTM){
			// Sync root's LTM
			if(frame->object.privateFlags & Frame::SUBTREESYNCLTM)
				frame->ltm = frame->matrix;
			// Synch attached objects
			FORLIST(lnk, frame->objectList)
				ObjectWithFrame::fromFrame(lnk)->sync();
			// ...and children
			syncRecurse(frame->child, frame->object.privateFlags);
		}else{
			// LTMs are clean, just synch objects
			FORLIST(lnk, frame->objectList)
				ObjectWithFrame::fromFrame(lnk)->sync();
			syncObjRecurse(frame->child);
		}
		// all clean now
		frame->object.privateFlags &= ~(Frame::SYNCLTM | Frame::SYNCOBJ);
	}
	Frame::dirtyList.init();
}

void
Frame::rotate(V3d *axis, float32 angle, CombineOp op)
{
	this->matrix.rotate(axis, angle, op);
	updateObjects();
}

void
Frame::translate(V3d *trans, CombineOp op)
{
	this->matrix.translate(trans, op);
	updateObjects();
}

void
Frame::scale(V3d *scl, CombineOp op)
{
	this->matrix.scale(scl, op);
	updateObjects();
}

void
Frame::updateObjects(void)
{
	// Mark root as dirty and insert into dirty list if necessary
	if((this->root->object.privateFlags & HIERARCHYSYNC) == 0)
		Frame::dirtyList.add(&this->root->inDirtyList);
	this->root->object.privateFlags |= HIERARCHYSYNC;
	// Mark subtree as dirty as well
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
	if(clonedroot == nil)
		clonedroot = frame;
	frame->object.copy(&this->object);
	frame->matrix = this->matrix;
	frame->root = clonedroot;
	this->root = frame;	// Remember cloned frame
	for(Frame *child = this->child; child; child = child->next){
		Frame *clonedchild = child->cloneAndLink(clonedroot);
		clonedchild->next = frame->child;
		frame->child = clonedchild;
		clonedchild->object.parent = frame;
	}
	s_plglist.copy(frame, this);
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
	this->frames = (Frame**)malloc(this->numFrames*sizeof(Frame*));
	if(this->frames == nil){
		RWERROR((ERR_ALLOC, this->numFrames*sizeof(Frame*)));
		return nil;
	}
	for(int32 i = 0; i < this->numFrames; i++){
		Frame *f;
		stream->read(&buf, sizeof(buf));
		this->frames[i] = f = Frame::create();
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
			this->frames[buf.parent]->addChild(f, rw::streamAppendFrames);
	}
	for(int32 i = 0; i < this->numFrames; i++)
		Frame::s_plglist.streamRead(stream, this->frames[i]);
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
		size += 12 + Frame::s_plglist.streamGetSize(this->frames[i]);

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
		Frame::s_plglist.streamWrite(stream, this->frames[i]);
}

static Frame*
sizeCB(Frame *f, void *size)
{
	*(int32*)size += Frame::s_plglist.streamGetSize(f);
	f->forAllChildren(sizeCB, size);
	return f;
}

uint32
FrameList_::streamGetSize(Frame *f)
{
	int32 numFrames = f->count();
	uint32 size = 12 + 12 + 4 + numFrames*(sizeof(FrameStreamData)+12);
	sizeCB(f, (void*)&size);
	return size;
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
