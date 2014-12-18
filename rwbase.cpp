#include <cstdio>
#include <cstdlib>
#include <cassert>

#include <iostream>
#include <fstream>

#include "rwbase.h"
#include "rwplugin.h"

using namespace std;

namespace Rw {

int Version = 0x36003;
int Build = 0xFFFF;

uint32
writeInt8(int8 tmp, ostream &rw)
{
	rw.write((char*)&tmp, sizeof(int8));
	return sizeof(int8);
}

uint32
writeUInt8(uint8 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(uint8));
        return sizeof(uint8);
}

uint32
writeInt16(int16 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(int16));
        return sizeof(int16);
}

uint32
writeUInt16(uint16 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(uint16));
        return sizeof(uint16);
}

uint32
writeInt32(int32 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(int32));
        return sizeof(int32);
}

uint32
writeUInt32(uint32 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(uint32));
        return sizeof(uint32);
}

uint32
writeFloat32(float32 tmp, ostream &rw)
{
        rw.write((char*)&tmp, sizeof(float32));
        return sizeof(float32);
}

uint8
readUInt8(istream &rw)
{
	uint8 tmp;
	rw.read((char*)&tmp, sizeof(uint8));
	return tmp;
}

int16
readInt16(istream &rw)
{
	int16 tmp;
	rw.read((char*)&tmp, sizeof(int16));
	return tmp;
}

uint16
readUInt16(istream &rw)
{
	uint16 tmp;
	rw.read((char*)&tmp, sizeof(uint16));
	return tmp;
}

int32
readInt32(istream &rw)
{
	int32 tmp;
	rw.read((char*)&tmp, sizeof(int32));
	return tmp;
}

uint32
readUInt32(istream &rw)
{
	uint32 tmp;
	rw.read((char*)&tmp, sizeof(uint32));
	return tmp;
}

float32
readFloat32(istream &rw)
{
	float32 tmp;
	rw.read((char*)&tmp, sizeof(float32));
	return tmp;
}

bool
WriteChunkHeader(ostream &s, int32 type, int32 size)
{
	struct {
		int32 type, size;
		uint32 id;
	} buf = { type, size, LibraryIDPack(Version, Build) };
	s.write((char*)&buf, 12);
	return true;
}

bool
ReadChunkHeaderInfo(istream &s, ChunkHeaderInfo *header)
{
	struct {
		int32 type, size;
		uint32 id;
	} buf;
	s.read((char*)&buf, 12);
	if(s.eof())
		return false;
	assert(header != NULL);
	header->type = buf.type;
	header->length = buf.size;
	header->version = LibraryIDUnpackVersion(buf.id);
	header->build = LibraryIDUnpackBuild(buf.id);
	return true;
}

bool
FindChunk(istream &s, uint32 type, uint32 *length, uint32 *version)
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
			return true;
		}
		s.seekg(header.length, ios::cur);
	}
	return false;
}

}
