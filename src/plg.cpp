#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <cctype>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"

namespace rw {

static void *defCtor(void *object, int32, int32) { return object; }
static void *defDtor(void *object, int32, int32) { return object; }
static void *defCopy(void *dst, void*, int32, int32) { return dst; }

void
PluginList::construct(void *object)
{
	for(Plugin *p = this->first; p; p = p->next)
		p->constructor(object, p->offset, p->size);
}

void
PluginList::destruct(void *object)
{
	for(Plugin *p = this->first; p; p = p->next)
		p->destructor(object, p->offset, p->size);
}

void
PluginList::copy(void *dst, void *src)
{
	for(Plugin *p = this->first; p; p = p->next)
		p->copy(dst, src, p->offset, p->size);
}

bool
PluginList::streamRead(Stream *stream, void *object)
{
	int32 length;
	ChunkHeaderInfo header;
	if(!findChunk(stream, ID_EXTENSION, (uint32*)&length, nil))
		return false;
	while(length > 0){
		if(!readChunkHeaderInfo(stream, &header))
			return false;
		length -= 12;
		for(Plugin *p = this->first; p; p = p->next)
			if(p->id == header.type && p->read){
				p->read(stream, header.length,
				        object, p->offset, p->size);
				goto cont;
			}
		stream->seek(header.length);
cont:
		length -= header.length;
	}
	return true;
}

void
PluginList::streamWrite(Stream *stream, void *object)
{
	int size = this->streamGetSize(object);
	writeChunkHeader(stream, ID_EXTENSION, size);
	for(Plugin *p = this->first; p; p = p->next){
		if(p->getSize == nil ||
		   (size = p->getSize(object, p->offset, p->size)) <= 0)
			continue;
		writeChunkHeader(stream, p->id, size);
		p->write(stream, size, object, p->offset, p->size);
	}
}

int
PluginList::streamGetSize(void *object)
{
	int32 size = 0;
	int32 plgsize;
	for(Plugin *p = this->first; p; p = p->next)
		if(p->getSize &&
		   (plgsize = p->getSize(object, p->offset, p->size)) > 0)
			size += 12 + plgsize;
	return size;
}

void
PluginList::assertRights(void *object, uint32 pluginID, uint32 data)
{
	for(Plugin *p = this->first; p; p = p->next)
		if(p->id == pluginID){
			if(p->rightsCallback)
				p->rightsCallback(object,
				                  p->offset, p->size, data);
			return;
		}
}


int32
PluginList::registerPlugin(int32 size, uint32 id,
	Constructor ctor, Destructor dtor, CopyConstructor copy)
{
	Plugin *p = (Plugin*)malloc(sizeof(Plugin));
	p->offset = this->size;
	this->size += size;
	int32 round = sizeof(void*)-1;
	this->size = (this->size + round)&~round;

	p->size = size;
	p->id = id;
	p->constructor = ctor ? ctor : defCtor;
	p->destructor = dtor ? dtor : defDtor;
	p->copy = copy ? copy : defCopy;
	p->read = nil;
	p->write = nil;
	p->getSize = nil;
	p->rightsCallback = nil;
	p->next = nil;
	p->prev = nil;

	if(this->first == nil){
		this->first = p;
		this->last = p;
	}else{
		this->last->next = p;
		p->prev = this->last;
		this->last = p;
	}
	return p->offset;
}

int32
PluginList::registerStream(uint32 id,
	StreamRead read, StreamWrite write, StreamGetSize getSize)
{
	for(Plugin *p = this->first; p; p = p->next)
		if(p->id == id){
			p->read = read;
			p->write = write;
			p->getSize = getSize;
			return p->offset;
		}
	return -1;
}

int32
PluginList::setStreamRightsCallback(uint32 id, RightsCallback cb)
{
	for(Plugin *p = this->first; p; p = p->next)
		if(p->id == id){
			p->rightsCallback = cb;
			return p->offset;
		}
	return -1;
}

int32
PluginList::getPluginOffset(uint32 id)
{
	for(Plugin *p = this->first; p; p = p->next)
		if(p->id == id)
			return p->offset;
	return -1;
}

}
