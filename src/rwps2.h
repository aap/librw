namespace Rw {

struct PS2InstanceData
{
	uint32 noRefChain;
	uint32 dataSize;
	uint8 *data;
	Material *material;
};

struct PS2InstanceDataHeader : InstanceDataHeader
{
	uint32 numMeshes;
	PS2InstanceData *instanceMeshes;
};

void *DestroyNativeDataPS2(void *object, int32, int32);
void ReadNativeDataPS2(std::istream &stream, int32 len, void *object, int32, int32);
void WriteNativeDataPS2(std::ostream &stream, int32 len, void *object, int32, int32);
int32 GetSizeNativeDataPS2(void *object, int32, int32);

}
