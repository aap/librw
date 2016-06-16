#ifndef RW_PS2
#include <stdint.h>
#endif
#include <cmath>

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

#define nelem(A) (sizeof(A) / sizeof A[0])

struct RGBA
{
	uint8 red;
	uint8 green;
	uint8 blue;
	uint8 alpha;
};

struct RGBAf
{
	float32 red;
	float32 green;
	float32 blue;
	float32 alpha;
};

struct V2d
{
	float32 x, y;
	V2d(void) : x(0.0f), y(0.0f) {}
	V2d(float32 x, float32 y) : x(x), y(y) {}
	void set(float32 x, float32 y){
		this->x = x; this->y = y; }
};

inline V2d add(const V2d &a, const V2d &b) { return V2d(a.x+b.x, a.y+b.y); }
inline V2d sub(const V2d &a, const V2d &b) { return V2d(a.x-b.x, a.y-b.y); }
inline V2d scale(const V2d &a, float32 r) { return V2d(a.x*r, a.y*r); }
inline float32 length(const V2d &v) { return sqrt(v.x*v.x + v.y*v.y); }
inline V2d normalize(const V2d &v) { return scale(v, 1.0f/length(v)); }

struct V3d
{
	float32 x, y, z;
	V3d(void) : x(0.0f), y(0.0f), z(0.0f) {}
	V3d(float32 x, float32 y, float32 z) : x(x), y(y), z(z) {}
	void set(float32 x, float32 y, float32 z){
		this->x = x; this->y = y; this->z = z; }
};

inline V3d add(const V3d &a, const V3d &b) { return V3d(a.x+b.x, a.y+b.y, a.z+b.z); }
inline V3d sub(const V3d &a, const V3d &b) { return V3d(a.x-b.x, a.y-b.y, a.z-b.z); }
inline V3d scale(const V3d &a, float32 r) { return V3d(a.x*r, a.y*r, a.z*r); }
inline float32 length(const V3d &v) { return sqrt(v.x*v.x + v.y*v.y + v.z*v.z); }
inline V3d normalize(const V3d &v) { return scale(v, 1.0f/length(v)); }
inline V3d setlength(const V3d &v, float32 l) { return scale(v, l/length(v)); }
V3d cross(const V3d &a, const V3d &b);
inline float32 dot(const V3d &a, const V3d &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

struct Quat
{
	float32 w, x, y, z;
	Quat(void) : w(0.0f), x(0.0f), y(0.0f), z(0.0f) {}
	Quat(float32 w, float32 x, float32 y, float32 z) : w(w), x(x), y(y), z(z) {}
	Quat(float32 w, V3d vec) : w(w), x(vec.x), y(vec.y), z(vec.z) {}
	Quat(V3d vec) : w(0.0f), x(vec.x), y(vec.y), z(vec.z) {}
	static Quat rotation(float32 angle, const V3d &axis){
		return Quat(cos(angle/2.0f), scale(axis, sin(angle/2.0f))); }
	void set(float32 w, float32 x, float32 y, float32 z){
		this->w = w; this->x = x; this->y = y; this->z = z; }
	V3d vec(void){ return V3d(x, y, z); }
};

inline Quat add(const Quat &q, const Quat &p) { return Quat(q.w+p.w, q.x+p.x, q.y+p.y, q.z+p.z); }
inline Quat sub(const Quat &q, const Quat &p) { return Quat(q.w-p.w, q.x-p.x, q.y-p.y, q.z-p.z); }
inline Quat scale(const Quat &q, float32 r) { return Quat(q.w*r, q.x*r, q.y*r, q.z*r); }
inline float32 length(const Quat &q) { return sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z); }
inline Quat normalize(const Quat &q) { return scale(q, 1.0f/length(q)); }
inline Quat conj(const Quat &q) { return Quat(q.w, -q.x, -q.y, -q.z); }
Quat mult(const Quat &q, const Quat &p);
inline V3d rotate(const V3d &v, const Quat &q) { return mult(mult(q, Quat(v)), conj(q)).vec(); }

struct Matrix
{
	V3d right;
	float32 rightw;
	V3d up;
	float32 upw;
	V3d at;
	float32 atw;
	V3d pos;
	float32 posw;

	static Matrix makeRotation(const Quat &q);
	void setIdentity(void);
	V3d transPoint(const V3d &p);
	V3d transVec(const V3d &v);
	bool32 isIdentity(void);
	// not very pretty :/
	static void mult(Matrix *m1, Matrix *m2, Matrix *m3);
	static bool32 invert(Matrix *m1, Matrix *m2);
	static void transpose(Matrix *m1, Matrix *m2);
};

struct Matrix3
{
	V3d right, up, at;

	static Matrix3 makeRotation(const Quat &q);
	void setIdentity(void);
	V3d transVec(const V3d &v);
	bool32 isIdentity(void);
	float32 determinant(void);
	// not very pretty :/
	static void mult(Matrix3 *m1, Matrix3 *m2, Matrix3 *m3);
	static bool32 invert(Matrix3 *m1, Matrix3 *m2);
	static void transpose(Matrix3 *m1, Matrix3 *m2);
};

void matrixIdentity(float32 *mat);
int matrixEqual(float32 *m1, float32 *m2);
int matrixIsIdentity(float32 *mat);
void matrixMult(float32 *out, float32 *a, float32 *b);
void vecTrans(float32 *out, float32 *mat, float32 *vec);
void matrixTranspose(float32 *out, float32 *in);
bool32 matrixInvert(float32 *out, float32 *in);
void matrixPrint(float32 *mat);
bool32 equal(const Matrix &m1, const Matrix &m2);

struct Sphere
{
	V3d center;
	float32 radius;
};

/*
 * Streams
 */

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
	PLATFORM_NULL = 0,
	// D3D7
	PLATFORM_GL   = 2,
	// MAC
	PLATFORM_PS2  = 4,
	PLATFORM_XBOX = 5,
	// GAMECUBE
	// SOFTRAS
	PLATFORM_D3D8 = 8,
	PLATFORM_D3D9 = 9,

	// non-stock-RW platforms

	PLATFORM_WDGL = 10,	// WarDrum OpenGL
	PLATFORM_GL3  = 11,	// my GL3 implementation

	NUM_PLATFORMS,

	FOURCC_PS2 = 0x00325350		// 'PS2\0'
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

	// Toolkit
	ID_SKYMIPMAP     = 0x110,
	ID_SKIN          = 0x116,
	ID_HANIMPLUGIN   = 0x11E,
	ID_MATFX         = 0x120,
	ID_PDS           = 0x131,
	ID_ADC           = 0x134,
	ID_UVANIMATION   = 0x135,

	// World
	ID_MESH          = 0x50E,
	ID_NATIVEDATA    = 0x510,
	ID_VERTEXFMT     = 0x511,
};

extern int version;
extern int build;
extern int platform;
extern char *debugFile;

void initialize(void);

int strncmp_ci(const char *s1, const char *s2, int n);

// 0x04000000	3.1
// 0x08000000	3.2
// 0x0C000000	3.3
// 0x10000000	3.4
// 0x14000000	3.5
// 0x18000000	3.6
// 0x1C000000	3.7

inline uint32
libraryIDPack(int version, int build)
{
	if(version <= 0x31000)
		return version>>8;
	return (version-0x30000 & 0x3FF00) << 14 | (version&0x3F) << 16 |
	       (build & 0xFFFF);
}

inline int
libraryIDUnpackVersion(uint32 libid)
{
	if(libid & 0xFFFF0000)
		return (libid>>14 & 0x3FF00) + 0x30000 |
		       (libid>>16 & 0x3F);
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
