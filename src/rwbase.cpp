#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <new>

#include "rwbase.h"
#include "rwplugin.h"

using namespace std;

namespace rw {

int32 version = 0x36003;
int32 build = 0xFFFF;
#ifdef RW_PS2
	int32 platform = PLATFORM_PS2;
#elif RW_OPENGL
	int32 platform = PLATFORM_OGL;
#elif RW_D3D9
	int32 platform = PLATFORM_D3D9;
#else
	int32 platform = PLATFORM_NULL;
#endif
char *debugFile = NULL;

void
matrixMult(float32 *out, float32 *a, float32 *b)
{
	// TODO: replace with platform optimized code
	#define L(i,j) out[i*4+j]
	#define A(i,j) a[i*4+j]
	#define B(i,j) b[i*4+j]
			for(int i = 0; i < 4; i++)
				for(int j = 0; j < 4; j++)
					L(i,j) = A(0,j)*B(i,0)
					       + A(1,j)*B(i,1)
					       + A(2,j)*B(i,2)
					       + A(3,j)*B(i,3);
	#undef L
	#undef A
	#undef B
}

void
matrixInvert(float32 *out, float32 *m)
{
	float32 inv[16], det;
	int i;

	inv[0] = m[5]  * m[10] * m[15] - 
	         m[5]  * m[11] * m[14] - 
	         m[9]  * m[6]  * m[15] + 
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] - 
	         m[13] * m[7]  * m[10];
	inv[4] = -m[4]  * m[10] * m[15] + 
	          m[4]  * m[11] * m[14] + 
	          m[8]  * m[6]  * m[15] - 
	          m[8]  * m[7]  * m[14] - 
	          m[12] * m[6]  * m[11] + 
	          m[12] * m[7]  * m[10];
	inv[8] = m[4]  * m[9] * m[15] - 
	         m[4]  * m[11] * m[13] - 
	         m[8]  * m[5] * m[15] + 
	         m[8]  * m[7] * m[13] + 
	         m[12] * m[5] * m[11] - 
	         m[12] * m[7] * m[9];
	inv[12] = -m[4]  * m[9] * m[14] + 
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] - 
	           m[8]  * m[6] * m[13] - 
	           m[12] * m[5] * m[10] + 
	           m[12] * m[6] * m[9];
	inv[1] = -m[1]  * m[10] * m[15] + 
	          m[1]  * m[11] * m[14] + 
	          m[9]  * m[2] * m[15] - 
	          m[9]  * m[3] * m[14] - 
	          m[13] * m[2] * m[11] + 
	          m[13] * m[3] * m[10];
	inv[5] = m[0]  * m[10] * m[15] - 
	         m[0]  * m[11] * m[14] - 
	         m[8]  * m[2] * m[15] + 
	         m[8]  * m[3] * m[14] + 
	         m[12] * m[2] * m[11] - 
	         m[12] * m[3] * m[10];
	inv[9] = -m[0]  * m[9] * m[15] + 
	          m[0]  * m[11] * m[13] + 
	          m[8]  * m[1] * m[15] - 
	          m[8]  * m[3] * m[13] - 
	          m[12] * m[1] * m[11] + 
	          m[12] * m[3] * m[9];
	inv[13] = m[0]  * m[9] * m[14] - 
	          m[0]  * m[10] * m[13] - 
	          m[8]  * m[1] * m[14] + 
	          m[8]  * m[2] * m[13] + 
	          m[12] * m[1] * m[10] - 
	          m[12] * m[2] * m[9];
	inv[2] = m[1]  * m[6] * m[15] - 
	         m[1]  * m[7] * m[14] - 
	         m[5]  * m[2] * m[15] + 
	         m[5]  * m[3] * m[14] + 
	         m[13] * m[2] * m[7] - 
	         m[13] * m[3] * m[6];
	inv[6] = -m[0]  * m[6] * m[15] + 
	          m[0]  * m[7] * m[14] + 
	          m[4]  * m[2] * m[15] - 
	          m[4]  * m[3] * m[14] - 
	          m[12] * m[2] * m[7] + 
	          m[12] * m[3] * m[6];
	inv[10] = m[0]  * m[5] * m[15] - 
	          m[0]  * m[7] * m[13] - 
	          m[4]  * m[1] * m[15] + 
	          m[4]  * m[3] * m[13] + 
	          m[12] * m[1] * m[7] - 
	          m[12] * m[3] * m[5];
	inv[14] = -m[0]  * m[5] * m[14] + 
	           m[0]  * m[6] * m[13] + 
	           m[4]  * m[1] * m[14] - 
	           m[4]  * m[2] * m[13] - 
	           m[12] * m[1] * m[6] + 
	           m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + 
	          m[1] * m[7] * m[10] + 
	          m[5] * m[2] * m[11] - 
	          m[5] * m[3] * m[10] - 
	          m[9] * m[2] * m[7] + 
	          m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - 
	         m[0] * m[7] * m[10] - 
	         m[4] * m[2] * m[11] + 
	         m[4] * m[3] * m[10] + 
	         m[8] * m[2] * m[7] - 
	         m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + 
	           m[0] * m[7] * m[9] + 
	           m[4] * m[1] * m[11] - 
	           m[4] * m[3] * m[9] - 
	           m[8] * m[1] * m[7] + 
	           m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - 
	          m[0] * m[6] * m[9] - 
	          m[4] * m[1] * m[10] + 
	          m[4] * m[2] * m[9] + 
	          m[8] * m[1] * m[6] - 
	          m[8] * m[2] * m[5];
	det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if(det == 0)
		return;
	det = 1.0f / det;
	for(i = 0; i < 16; i++)
		out[i] = inv[i] * det;
}

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
	return fwrite(data, length, 1, this->file);
}

uint32
StreamFile::read(void *data, uint32 length)
{
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
writeChunkHeader(Stream *s, int32 type, int32 size)
{
	struct {
		int32 type, size;
		uint32 id;
	} buf = { type, size, libraryIDPack(version, build) };
	s->write(&buf, 12);
	return true;
}

bool
readChunkHeaderInfo(Stream *s, ChunkHeaderInfo *header)
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
	header->version = libraryIDUnpackVersion(buf.id);
	header->build = libraryIDUnpackBuild(buf.id);
	return true;
}

bool
findChunk(Stream *s, uint32 type, uint32 *length, uint32 *version)
{
	ChunkHeaderInfo header;
	while(readChunkHeaderInfo(s, &header)){
		if(header.type == ID_NAOBJECT)
			return false;
		if(header.type == type){
			if(length)
				*length = header.length;
			if(version)
				*version = header.version;
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

uint8*
getFileContents(char *name, uint32 *len)
{
        FILE *cf = fopen(name, "rb");
        assert(cf != NULL);
        fseek(cf, 0, SEEK_END);
        *len = ftell(cf);
        fseek(cf, 0, SEEK_SET);
        uint8 *data = new uint8[*len];
        fread(data, *len, 1, cf);
        fclose(cf);
        return data;
}

}
