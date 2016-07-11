#ifndef RW_PS2
#include <stdint.h>
#endif
#include <cmath>

#ifdef RW_GL3
#define RW_OPENGL
#endif

#ifdef RW_WDGL
#define RW_OPENGL
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

#define nil NULL

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

inline void convColor(RGBA *i, RGBAf *f){
	int32 c;
	c = (int32)(f->red*255.0f + 0.5f);
	i->red   = (uint8)c;
	c = (int32)(f->green*255.0f + 0.5f);
	i->green = (uint8)c;
	c = (int32)(f->blue*255.0f + 0.5f);
	i->blue  = (uint8)c;
	c = (int32)(f->alpha*255.0f + 0.5f);
	i->alpha = (uint8)c;
}

inline void convColor(RGBAf *f, RGBA *i){
	f->red   = i->red/255.0f;
	f->green = i->green/255.0f;
	f->blue  = i->blue/255.0f;
	f->alpha = i->alpha/255.0f;
}

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
inline V3d lerp(const V3d &a, const V3d &b, float32 r){
	return V3d(a.x + r*(b.x - a.x),
	           a.y + r*(b.y - a.y),
	           a.z + r*(b.z - a.z));
};

struct Quat
{
	// order is important for streaming
	float32 x, y, z, w;
	Quat(void) : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
	Quat(float32 w, float32 x, float32 y, float32 z) : x(x), y(y), z(z), w(w) {}
	Quat(float32 w, V3d vec) : x(vec.x), y(vec.y), z(vec.z), w(w) {}
	Quat(V3d vec) : x(vec.x), y(vec.y), z(vec.z), w(0.0f) {}
	static Quat rotation(float32 angle, const V3d &axis){
		return Quat(cos(angle/2.0f), scale(axis, sin(angle/2.0f))); }
	void set(float32 w, float32 x, float32 y, float32 z){
		this->w = w; this->x = x; this->y = y; this->z = z; }
	V3d vec(void){ return V3d(x, y, z); }
};

inline Quat add(const Quat &q, const Quat &p) { return Quat(q.w+p.w, q.x+p.x, q.y+p.y, q.z+p.z); }
inline Quat sub(const Quat &q, const Quat &p) { return Quat(q.w-p.w, q.x-p.x, q.y-p.y, q.z-p.z); }
inline Quat negate(const Quat &q) { return Quat(-q.w, -q.x, -q.y, -q.z); }
inline float32 dot(const Quat &q, const Quat &p) { return q.w*p.w + q.x*p.x + q.y*p.y + q.z*p.z; }
inline Quat scale(const Quat &q, float32 r) { return Quat(q.w*r, q.x*r, q.y*r, q.z*r); }
inline float32 length(const Quat &q) { return sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z); }
inline Quat normalize(const Quat &q) { return scale(q, 1.0f/length(q)); }
inline Quat conj(const Quat &q) { return Quat(q.w, -q.x, -q.y, -q.z); }
Quat mult(const Quat &q, const Quat &p);
inline V3d rotate(const V3d &v, const Quat &q) { return mult(mult(q, Quat(v)), conj(q)).vec(); }
Quat lerp(const Quat &q, const Quat &p, float32 r);
Quat slerp(const Quat &q, const Quat &p, float32 a);


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
	void pointInDirection(const V3d &d, const V3d &up);
	V3d transPoint(const V3d &p);
	V3d transVec(const V3d &v);
	bool32 isIdentity(void);
	// not very pretty :/
	static void mult(Matrix *m1, Matrix *m2, Matrix *m3);
	static bool32 invert(Matrix *m1, Matrix *m2);
	static void invertOrthonormal(Matrix *m1, Matrix *m2);
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

#define MAKEPLUGINID(v, id) (((v & 0xFFFFFF) << 8) | (id & 0xFF))

enum VendorID
{
	VEND_CORE           = 0,
	VEND_CRITERIONTK    = 1,
	VEND_CRITERIONINT   = 4,
	VEND_CRITERIONWORLD = 5,
	// Used for platform-specific stuff like rasters
	VEND_PLATFORM       = 10,
};

enum PluginID
{
	// Core
	ID_NAOBJECT      = MAKEPLUGINID(VEND_CORE, 0x00),
	ID_STRUCT        = MAKEPLUGINID(VEND_CORE, 0x01),
	ID_STRING        = MAKEPLUGINID(VEND_CORE, 0x02),
	ID_EXTENSION     = MAKEPLUGINID(VEND_CORE, 0x03),
	ID_CAMERA        = MAKEPLUGINID(VEND_CORE, 0x05),
	ID_TEXTURE       = MAKEPLUGINID(VEND_CORE, 0x06),
	ID_MATERIAL      = MAKEPLUGINID(VEND_CORE, 0x07),
	ID_MATLIST       = MAKEPLUGINID(VEND_CORE, 0x08),
	ID_FRAMELIST     = MAKEPLUGINID(VEND_CORE, 0x0E),
	ID_GEOMETRY      = MAKEPLUGINID(VEND_CORE, 0x0F),
	ID_CLUMP         = MAKEPLUGINID(VEND_CORE, 0x10),
	ID_LIGHT         = MAKEPLUGINID(VEND_CORE, 0x12),
	ID_ATOMIC        = MAKEPLUGINID(VEND_CORE, 0x14),
	ID_TEXTURENATIVE = MAKEPLUGINID(VEND_CORE, 0x15),
	ID_TEXDICTIONARY = MAKEPLUGINID(VEND_CORE, 0x16),
	ID_GEOMETRYLIST  = MAKEPLUGINID(VEND_CORE, 0x1A),
	ID_ANIMANIMATION = MAKEPLUGINID(VEND_CORE, 0x1B),
	ID_RIGHTTORENDER = MAKEPLUGINID(VEND_CORE, 0x1F),
	ID_UVANIMDICT    = MAKEPLUGINID(VEND_CORE, 0x2B),

	// Toolkit
	ID_SKYMIPMAP     = MAKEPLUGINID(VEND_CRITERIONTK, 0x10),
	ID_SKIN          = MAKEPLUGINID(VEND_CRITERIONTK, 0x16),
	ID_HANIMPLUGIN   = MAKEPLUGINID(VEND_CRITERIONTK, 0x1E),
	ID_MATFX         = MAKEPLUGINID(VEND_CRITERIONTK, 0x20),
	ID_PDS           = MAKEPLUGINID(VEND_CRITERIONTK, 0x31),
	ID_ADC           = MAKEPLUGINID(VEND_CRITERIONTK, 0x34),
	ID_UVANIMATION   = MAKEPLUGINID(VEND_CRITERIONTK, 0x35),

	// World
	ID_MESH          = MAKEPLUGINID(VEND_CRITERIONWORLD, 0x0E),
	ID_NATIVEDATA    = MAKEPLUGINID(VEND_CRITERIONWORLD, 0x10),
	ID_VERTEXFMT     = MAKEPLUGINID(VEND_CRITERIONWORLD, 0x11),

	// custom native raster
	ID_RASTERGL      = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_GL),
	ID_RASTERPS2     = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_PS2),
	ID_RASTERXBOX    = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_XBOX),
	ID_RASTERD3D8    = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_D3D8),
	ID_RASTERD3D9    = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_D3D9),
	ID_RASTERWDGL    = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_WDGL),
	ID_RASTERGL3     = MAKEPLUGINID(VEND_PLATFORM, PLATFORM_GL3),
};

#define ECODE(c, s) c,

enum Errors
{
	ERR_NONE = 0x80000000,
#include "base.err"
};

#undef ECODE

extern int version;
extern int build;
extern int platform;
extern char *debugFile;

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
