namespace Rw {

struct OGLAttrib
{
	// arguments to glVertexAttribPointer (should use OpenGL types here)
	// Vertex = 0, TexCoord, Normal, Color, Weight, Bone Index
	uint32 index;
	// float = 0, byte, ubyte, short, ushort
	int32 type;
	bool32 normalized;
	int32 size;
	uint32 stride;
	uint32 offset;
};

struct OGLInstanceDataHeader : InstanceDataHeader
{
	int32 numAttribs;
	OGLAttrib *attribs;
	uint32 dataSize;
	uint8 *data;
};

void *DestroyNativeDataOGL(void *object, int32, int32);
void ReadNativeDataOGL(std::istream &stream, int32 len, void *object, int32, int32);
void WriteNativeDataOGL(std::ostream &stream, int32 len, void *object, int32, int32);
int32 GetSizeNativeDataOGL(void *object, int32, int32);

}
