namespace Rw {

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef float float32;
typedef uint8 byte;
typedef uint32 uint;

uint32 writeInt8(int8 tmp, std::ostream &rw);
uint32 writeUInt8(uint8 tmp, std::ostream &rw);
uint32 writeInt16(int16 tmp, std::ostream &rw);
uint32 writeUInt16(uint16 tmp, std::ostream &rw);
uint32 writeInt32(int32 tmp, std::ostream &rw);
uint32 writeUInt32(uint32 tmp, std::ostream &rw);
uint32 writeFloat32(float32 tmp, std::ostream &rw);
int8 readInt8(std::istream &rw);
uint8 readUInt8(std::istream &rw);
int16 readInt16(std::istream &rw);
uint16 readUInt16(std::istream &rw);
int32 readInt32(std::istream &rw);
uint32 readUInt32(std::istream &rw);
float32 readFloat32(std::istream &rw);

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
};

extern int Version;
extern int Build;

inline uint32
LibraryIDPack(int version, int build)
{
	if(build){
		version -= 0x30000;
		return (version&0xFFC0) << 14 | (version&0x3F) << 16 |
		       (build & 0xFFFF);
	}
	return version>>8;
}

inline int
LibraryIDUnpackVersion(uint32 libid)
{
	if(libid & 0xFFFF0000)
		return (libid>>14 & 0x3FF00) |
		       (libid>>16 & 0x3F) |
		       0x30000;
	else
		return libid<<8;
}

inline int
LibraryIDUnpackBuild(uint32 libid)
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
bool WriteChunkHeader(std::ostream &s, int32 type, int32 size);
bool ReadChunkHeaderInfo(std::istream &s, ChunkHeaderInfo *header);
bool FindChunk(std::istream &s, uint32 type, uint32 *length, uint32 *version);

int32 findPointer(void *p, void **list, int32 num);
}
