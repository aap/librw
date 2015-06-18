#ifndef RW_PS2
#include <stdint.h>
#endif

namespace rw {

#ifdef RW_PS2
	typedef char int8;
	typedef short int16;
	typedef int int32;
	typedef long long int64;
	typedef unsigned char uint8;
	typedef unsigned short uint16;
	typedef unsigned int uint32;
	typedef unsigned long long uint64;
	typedef unsigned int uintptr;
#else
	/* get rid of the stupid _t */
	typedef int8_t int8;
	typedef int16_t int16;
	typedef int32_t int32;
	typedef int64_t int64;
	typedef uint8_t uint8;
	typedef uint16_t uint16;
	typedef uint32_t uint32;
	typedef uint64_t uint64;
	typedef uintptr_t uintptr;
#endif

typedef float float32;
typedef int32 bool32;
typedef uint8 byte;
typedef uint32 uint;

class Stream
{
public:
	virtual void close(void) = 0;
	virtual uint32 write(const void *data, uint32 length) = 0;
	virtual uint32 read(void *data, uint32 length) = 0;
	virtual void seek(int32 offset, int32 whence = 1) = 0;
	virtual uint32 tell(void) = 0;
	virtual bool eof(void) = 0;
	int32   writeI8(int8 val);
	int32   writeU8(uint8 val);
	int32   writeI16(int16 val);
	int32   writeU16(uint16 val);
	int32   writeI32(int32 val);
	int32   writeU32(uint32 val);
	int32   writeF32(float32 val);
	int8    readI8(void);
	uint8   readU8(void);
	int16   readI16(void);
	uint16  readU16(void);
	int32   readI32(void);
	uint32  readU32(void);
	float32 readF32(void);
};

class StreamMemory : public Stream
{
	uint8 *data;
	uint32 length;
	uint32 capacity;
	uint32 position;
public:
	void close(void);
	uint32 write(const void *data, uint32 length);
	uint32 read(void *data, uint32 length);
	void seek(int32 offset, int32 whence = 1);
	uint32 tell(void);
	bool eof(void);
	StreamMemory *open(uint8 *data, uint32 length, uint32 capacity = 0);
	uint32 getLength(void);

	enum {
		S_EOF = 0xFFFFFFFF
	};
};

class StreamFile : public Stream
{
	FILE *file;
public:
	void close(void);
	uint32 write(const void *data, uint32 length);
	uint32 read(void *data, uint32 length);
	void seek(int32 offset, int32 whence = 1);
	uint32 tell(void);
	bool eof(void);
	StreamFile *open(const char *path, const char *mode);
};

enum Platform
{
	PLATFORM_OGL  = 2,
	PLATFORM_PS2  = 4,
	PLATFORM_XBOX = 5,
	PLATFORM_D3D8 = 8,
	PLATFORM_D3D9 = 9
};

enum PluginID
{
	// Core
	ID_NAOBJECT      = 0x0,
	ID_STRUCT        = 0x1,
	ID_STRING        = 0x2,
	ID_EXTENSION     = 0x3,
	ID_CAMERA        = 0x5,
	ID_TEXTURE       = 0x6,
	ID_MATERIAL      = 0x7,
	ID_MATLIST       = 0x8,
	ID_FRAMELIST     = 0xE,
	ID_GEOMETRY      = 0xF,
	ID_CLUMP         = 0x10,
	ID_LIGHT         = 0x12,
	ID_ATOMIC        = 0x14,
	ID_TEXTURENATIVE = 0x15,
	ID_TEXDICTIONARY = 0x16,
	ID_GEOMETRYLIST  = 0x1A,
	ID_ANIMANIMATION = 0x1B,
	ID_RIGHTTORENDER = 0x1F,
	ID_UVANIMDICT    = 0x2B,

	ID_SKIN          = 0x116,
	ID_HANIMPLUGIN   = 0x11E,
	ID_MATFX         = 0x120,
	ID_PDS           = 0x131,
	ID_ADC           = 0x134,
	ID_MESH          = 0x50E,
	ID_NATIVEDATA    = 0x510,
};

extern int version;
extern int build;
extern char *debugFile;

inline uint32
libraryIDPack(int version, int build)
{
	// TODO: check version in if statement
	if(build){
		version -= 0x30000;
		return (version&0xFFC0) << 14 | (version&0x3F) << 16 |
		       (build & 0xFFFF);
	}
	return version>>8;
}

inline int
libraryIDUnpackVersion(uint32 libid)
{
	if(libid & 0xFFFF0000)
		return (libid>>14 & 0x3FF00) |
		       (libid>>16 & 0x3F) |
		       0x30000;
	else
		return libid<<8;
}

inline int
libraryIDUnpackBuild(uint32 libid)
{
	if(libid & 0xFFFF0000)
		return libid & 0xFFFF;
	else
		return 0;
}

struct ChunkHeaderInfo
{
	uint32 type;
	uint32 length;
	uint32 version, build;
};

// TODO?: make these methods of ChunkHeaderInfo?
bool writeChunkHeader(Stream *s, int32 type, int32 size);
bool readChunkHeaderInfo(Stream *s, ChunkHeaderInfo *header);
bool findChunk(Stream *s, uint32 type, uint32 *length, uint32 *version);

int32 findPointer(void *p, void **list, int32 num);
uint8 *getFileContents(char *name, uint32 *len);
}
