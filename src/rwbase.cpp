#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"

using namespace std;

namespace Rw {

int Version = 0x36003;
int Build = 0xFFFF;
char *DebugFile = NULL;

int32
Stream::writeI8(int8 val)
{
	return write(&val, sizeof(int8));
}

int32
Stream::writeU8(uint8 val)
{
	return write(&val, sizeof(uint8));
}

int32
Stream::writeI16(int16 val)
{
	return write(&val, sizeof(int16));
}

int32
Stream::writeU16(uint16 val)
{
	return write(&val, sizeof(uint16));
}

int32
Stream::writeI32(int32 val)
{
	return write(&val, sizeof(int32));
}

int32
Stream::writeU32(uint32 val)
{
	return write(&val, sizeof(uint32));
}

int32
Stream::writeF32(float32 val)
{
	return write(&val, sizeof(float32));
}

int8
Stream::readI8(void)
{
	int8 tmp;
	read(&tmp, sizeof(int8));
	return tmp;
}

uint8
Stream::readU8(void)
{
	uint8 tmp;
	read(&tmp, sizeof(uint8));
	return tmp;
}

int16
Stream::readI16(void)
{
	int16 tmp;
	read(&tmp, sizeof(int16));
	return tmp;
}

uint16
Stream::readU16(void)
{
	uint16 tmp;
	read(&tmp, sizeof(uint16));
	return tmp;
}

int32
Stream::readI32(void)
{
	int32 tmp;
	read(&tmp, sizeof(int32));
	return tmp;
}

uint32
Stream::readU32(void)
{
	uint32 tmp;
	read(&tmp, sizeof(uint32));
	return tmp;
}

float32
Stream::readF32(void)
{
	float32 tmp;
	read(&tmp, sizeof(float32));
	return tmp;
}



void
StreamMemory::close(void)
{
}

uint32
StreamMemory::write(const void *data, uint32 len)
{
	if(this->eof())
		return 0;
	uint32 l = len;
	if(this->position+l > this->length){
		if(this->position+l > this->capacity)
			l = this->capacity-this->position;
		this->length = this->position+l;
	}
	memcpy(&this->data[this->position], data, l);
	this->position += l;
	if(len != l)
		this->position = S_EOF;
	return l;
}

uint32
StreamMemory::read(void *data, uint32 len)
{
	if(this->eof())
		return 0;
	uint32 l = len;
	if(this->position+l > this->length)
		l = this->length-this->position;
	memcpy(data, &this->data[this->position], l);
	this->position += l;
	if(len != l)
		this->position = S_EOF;
	return l;
}

void
StreamMemory::seek(int32 offset, int32 whence)
{
	if(whence == 0)
		this->position = offset;
	else if(whence == 1)
		this->position += offset;
	else
		this->position = this->length-offset;
	if(this->position > this->length){
		// TODO: ideally this would depend on the mode
		if(this->position > this->capacity)
			this->position = S_EOF;
		else
			this->length = this->position;
	}
}

uint32
StreamMemory::tell(void)
{
	return this->position;
}

bool
StreamMemory::eof(void)
{
	return this->position == S_EOF;
}

StreamMemory*
StreamMemory::open(uint8 *data, uint32 length, uint32 capacity)
{
	this->data = data;
	this->capacity = capacity;
	this->length = length;
	if(this->capacity < this->length)
		this->capacity = this->length;
	this->position = 0;
	return this;
}

uint32
StreamMemory::getLength(void)
{
	return this->length;
}


StreamFile*
StreamFile::open(const char *path, const char *mode)
{
	this->file = fopen(path, mode);
	return this->file ? this : NULL;
}

void
StreamFile::close(void)
{
	fclose(this->file);
}

uint32
StreamFile::write(const void *data, uint32 length)
{
//printf("write %d bytes @ %x\n", length, this->tell());
	return fwrite(data, length, 1, this->file);
}

uint32
StreamFile::read(void *data, uint32 length)
{
//printf("read %d bytes @ %x\n", length, this->tell());
	return fread(data, length, 1, this->file);
}

void
StreamFile::seek(int32 offset, int32 whence)
{
	fseek(this->file, offset, whence);
}

uint32
StreamFile::tell(void)
{
	return ftell(this->file);
}

bool
StreamFile::eof(void)
{
	return feof(this->file);
}

bool
WriteChunkHeader(Stream *s, int32 type, int32 size)
{
	struct {
		int32 type, size;
		uint32 id;
	} buf = { type, size, LibraryIDPack(Version, Build) };
//printf("- write chunk %x @ %x\n", buf.type, s->tell());
	s->write(&buf, 12);
	return true;
}

bool
ReadChunkHeaderInfo(Stream *s, ChunkHeaderInfo *header)
{
	struct {
		int32 type, size;
		uint32 id;
	} buf;
	s->read(&buf, 12);
	if(s->eof())
		return false;
	assert(header != NULL);
	header->type = buf.type;
	header->length = buf.size;
	header->version = LibraryIDUnpackVersion(buf.id);
	header->build = LibraryIDUnpackBuild(buf.id);
	return true;
}

bool
FindChunk(Stream *s, uint32 type, uint32 *length, uint32 *version)
{
	ChunkHeaderInfo header;
	while(ReadChunkHeaderInfo(s, &header)){
		if(header.type == ID_NAOBJECT)
			return false;
		if(header.type == type){
			if(length)
				*length = header.length;
			if(version)
				*version = header.version;
//printf("- chunk %x @ %x\n", header.type, s->tell()-12);
			return true;
		}
		s->seek(header.length);
	}
	return false;
}

int32
findPointer(void *p, void **list, int32 num)
{
	int i;
	for(i = 0; i < num; i++)
		if(list[i] == p)
			goto found;
	return -1;
found:
	return i;
}

}
