namespace rw {
namespace gl {

struct AttribDesc
{
	// arguments to glVertexAttribPointer (should use OpenGL types here)
	// Vertex = 0, TexCoord, Normal, Color, Weight, Bone Index, Extra Color
	uint32 index;
	// float = 0, byte, ubyte, short, ushort
	int32 type;
	bool32 normalized;
	int32 size;
	uint32 stride;
	uint32 offset;
};

struct InstanceDataHeader : rw::InstanceDataHeader
{
	int32 numAttribs;
	AttribDesc *attribs;
	uint32 dataSize;
	uint8 *data;

	// needed for rendering
	uint32 vbo;
	uint32 ibo;
};

void *destroyNativeData(void *object, int32, int32);
void readNativeData(Stream *stream, int32 len, void *object, int32, int32);
void writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);

void instance(Atomic *atomic);

// only RW_OPENGL
void uploadGeo(Geometry *geo);
void setAttribPointers(InstanceDataHeader *inst);

// Skin plugin

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

// Raster

struct Texture : rw::Texture
{
	void upload(void);
	void bind(int n);
};

extern int32 nativeRasterOffset;
void registerNativeRaster(void);

}
}
