namespace Rw {
namespace Gl {

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

struct InstanceDataHeader : Rw::InstanceDataHeader
{
	int32 numAttribs;
	AttribDesc *attribs;
	uint32 dataSize;
	uint8 *data;

	// needed for rendering
	uint32 vbo;
	uint32 ibo;
};

void *DestroyNativeData(void *object, int32, int32);
void ReadNativeData(Stream *stream, int32 len, void *object, int32, int32);
void WriteNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 GetSizeNativeData(void *object, int32, int32);

void Instance(Atomic *atomic);

// only RW_OPENGL
void UploadGeo(Geometry *geo);
void SetAttribPointers(InstanceDataHeader *inst);

// Skin plugin

void ReadNativeSkin(Stream *stream, int32, void *object, int32 offset);
void WriteNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 GetSizeNativeSkin(void *object, int32 offset);

}
}
