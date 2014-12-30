namespace Rw {
namespace Ps2 {

struct InstanceData
{
	// 0 - addresses in ref tags need fixing
	// 1 - no ref tags, so no fixing
	// set by the program:
	// 2 - ref tags are fixed, need to unfix before stream write
	uint32 arePointersFixed;
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

// only RW_PS2
void fixDmaOffsets(InstanceData *inst);
void unfixDmaOffsets(InstanceData *inst);

}
}
