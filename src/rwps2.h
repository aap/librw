namespace rw {
namespace ps2 {

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

struct InstanceDataHeader : rw::InstanceDataHeader
{
	uint32 numMeshes;
	InstanceData *instanceMeshes;
};

enum {
	VU_Lights	= 0x3d0
};

void *destroyNativeData(void *object, int32, int32);
void readNativeData(Stream *stream, int32 len, void *object, int32, int32);
void writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

void printDMA(InstanceData *inst);
void walkDMA(InstanceData *inst, void (*f)(uint32 *data, int32 size));
void sizedebug(InstanceData *inst);

// only RW_PS2
void fixDmaOffsets(InstanceData *inst);
void unfixDmaOffsets(InstanceData *inst);
//

struct Pipeline : rw::Pipeline
{
	uint32 vifOffset;
	uint32 inputStride;
	uint32 triStripCount, triListCount;

	static uint32 getVertCount(uint32 top, uint32 inAttribs,
	                           uint32 outAttribs, uint32 outBufs) {
		return (top-outBufs)/(inAttribs*2+outAttribs*outBufs);
	}

	Pipeline(uint32 platform);
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
//	virtual void render(Atomic *atomic);
	void setTriBufferSizes(uint32 inputStride,
	                       uint32 stripCount, uint32 listCount);
};

Pipeline *makeDefaultPipeline(void);
Pipeline *makeSkinPipeline(void);
Pipeline *makeMatFXPipeline(void);
void dumpPipeline(rw::Pipeline *pipe);

// Skin plugin

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

// ADC plugin

// The plugin is a little crippled due to lack of documentation

struct ADCData
{
	// only information we can get from GTA DFFs :/
	uint32 adcFormatted;
};

void registerADCPlugin(void);

}
}
