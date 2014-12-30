namespace Rw {
namespace Ps2 {

struct InstanceData
{
	uint32 noRefChain;
	uint32 dataSize;
	uint8 *data;
	Material *material;
};

struct InstanceDataHeader : Rw::InstanceDataHeader
{
	uint32 numMeshes;
	InstanceData *instanceMeshes;
};

void *DestroyNativeData(void *object, int32, int32);
void ReadNativeData(Stream *stream, int32 len, void *object, int32, int32);
void WriteNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 GetSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

}
}
