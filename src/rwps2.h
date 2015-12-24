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

enum PS2Attribs {
	AT_V2_32	= 0x64000000,
	AT_V2_16	= 0x65000000,
	AT_V2_8		= 0x66000000,
	AT_V3_32	= 0x68000000,
	AT_V3_16	= 0x69000000,
	AT_V3_8		= 0x6A000000,
	AT_V4_32	= 0x6C000000,
	AT_V4_16	= 0x6D000000,
	AT_V4_8		= 0x6E000000,
	AT_UNSGN	= 0x00004000,

	AT_RW		= 0x6
};

enum PS2AttibTypes {
	AT_XYZ		= 0,
	AT_UV		= 1,
	AT_RGBA		= 2,
	AT_NORMAL	= 3
};

void *destroyNativeData(void *object, int32, int32);
void readNativeData(Stream *stream, int32 len, void *object, int32, int32);
void writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

void printDMA(InstanceData *inst);
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
	void (*instanceCB)(MatPipeline*, Geometry*, Mesh*, uint8**);
	void (*uninstanceCB)(MatPipeline*, Geometry*, uint32*, Mesh*, uint8**);
	void (*preUninstCB)(MatPipeline*, Geometry*);
	void (*postUninstCB)(MatPipeline*, Geometry*);

	static uint32 getVertCount(uint32 top, uint32 inAttribs,
	                           uint32 outAttribs, uint32 outBufs) {
		return (top-outBufs)/(inAttribs*2+outAttribs*outBufs);
	}

	MatPipeline(uint32 platform);
	virtual void dump(void);
	void setTriBufferSizes(uint32 inputStride, uint32 stripCount);
	void instance(Geometry *g, InstanceData *inst, Mesh *m);
	uint8 *collectData(Geometry *g, InstanceData *inst, Mesh *m, uint8 *data[]);
};

class ObjPipeline : public rw::ObjPipeline
{
public:
	MatPipeline *groupPipeline;

	ObjPipeline(uint32 platform);
	virtual void instance(Atomic *atomic);
	virtual void uninstance(Atomic *atomic);
};

struct Vertex {
	float32 p[3];
	float32 t[2];
	uint8   c[4];
	float32 n[3];
};

void insertVertex(Geometry *geo, int32 i, uint32 mask, Vertex *v);

extern ObjPipeline *defaultObjPipe;
extern MatPipeline *defaultMatPipe;

ObjPipeline *makeDefaultPipeline(void);
ObjPipeline *makeSkinPipeline(void);
ObjPipeline *makeMatFXPipeline(void);
void dumpPipeline(rw::Pipeline *pipe);

// Skin plugin

struct SkinVertex : Vertex {
	float32 w[4];
	uint8   i[4];
};

void insertVertexSkin(Geometry *geo, int32 i, uint32 mask, SkinVertex *v);
int32 findVertexSkin(Geometry *g, uint32 flags[], uint32 mask, SkinVertex *v);

void readNativeSkin(Stream *stream, int32, void *object, int32 offset);
void writeNativeSkin(Stream *stream, int32 len, void *object, int32 offset);
int32 getSizeNativeSkin(void *object, int32 offset);

void instanceSkinData(Geometry *g, Mesh *m, Skin *skin, uint32 *data);

void skinPreCB(MatPipeline*, Geometry*);
void skinPostCB(MatPipeline*, Geometry*);

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
extern int32 adcOffset;
void registerADCPlugin(void);

void convertADC(Geometry *g);
void unconvertADC(Geometry *geo);
void allocateADC(Geometry *geo);

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

Pipeline *getPDSPipe(uint32 data);
void registerPDSPipe(Pipeline *pipe);
void registerPDSPlugin(int32 n);

// Native Texture and Raster

struct Ps2Raster : NativeRaster
{
	int32 mipmap;
};

extern int32 nativeRasterOffset;
void registerNativeRaster(void);

}
}
