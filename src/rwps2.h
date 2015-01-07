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
void RegisterNativeDataPlugin(void);

void walkDMA(InstanceData *inst, void (*f)(uint32 *data, int32 size));
void sizedebug(InstanceData *inst);

// only RW_PS2
void fixDmaOffsets(InstanceData *inst);
void unfixDmaOffsets(InstanceData *inst);

// Skin plugin

void ReadNativeSkin(Stream *stream, int32, void *object, int32 offset);
void WriteNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 GetSizeNativeSkin(void *object, int32 offset);

// ADC plugin

// The plugin is a little crippled due to lack of documentation

struct ADCData
{
	// only information we can get from GTA DFFs :/
	uint32 adcFormatted;
};

void RegisterADCPlugin(void);

}
}
