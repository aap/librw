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

class MatPipeline : public rw::Pipeline
{
public:
	uint32 vifOffset;
	uint32 inputStride;
	uint32 triStripCount, triListCount;
	PipeAttribute *attribs[10];
	void (*instanceCB)(MatPipeline*, Geometry*, Mesh*, uint8**, int32);

	static uint32 getVertCount(uint32 top, uint32 inAttribs,
	                           uint32 outAttribs, uint32 outBufs) {
		return (top-outBufs)/(inAttribs*2+outAttribs*outBufs);
	}

	MatPipeline(uint32 platform);
	virtual void dump(void);
	void setTriBufferSizes(uint32 inputStride, uint32 stripCount);
	void instance(Geometry *g, InstanceData *inst, Mesh *m);
};

class ObjPipeline : public rw::ObjPipeline
{
public:
	MatPipeline *groupPipeline;

	ObjPipeline(uint32 platform);
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
};

extern ObjPipeline *defaultObjPipe;
extern MatPipeline *defaultMatPipe;

ObjPipeline *makeDefaultPipeline(void);
ObjPipeline *makeSkinPipeline(void);
ObjPipeline *makeMatFXPipeline(void);
void dumpPipeline(rw::Pipeline *pipe);

// Skin plugin

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

// ADC plugin

// Each element in adcBits corresponds to an index in Mesh->indices,
// this assumes the Mesh indices are ADC formatted.
// ADCData->numBits != Mesh->numIndices. ADCData->numBits is probably
// equal to Mesh->numIndices before the Mesh gets ADC formatted.
//
// Can't convert between ADC-formatted and non-ADC-formatted yet :(

struct ADCData
{
	bool32 adcFormatted;
	int8 *adcBits;
	int32 numBits;
};

void registerADCPlugin(void);

// PDS plugin

// IDs used by SA
//    n   atomic   material
//  1892  53f20080 53f20081	// ?
//     1  53f20080 53f2008d	// triad_buddha01.dff
// 56430  53f20082 53f20083	// world
//    39  53f20082 53f2008f	// reflective world
//  6941  53f20084 53f20085	// vehicles
//  3423  53f20084 53f20087	// vehicles
//  4640  53f20084 53f2008b	// vehicles
//   418  53f20088 53f20089	// peds


void registerPDSPlugin(void);

}
}
