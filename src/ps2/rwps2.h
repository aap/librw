namespace rw {
namespace ps2 {

void initializePlatform(void);

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

// Not really types as in RW but offsets
enum PS2AttibTypes {
	AT_XYZ		= 0,
	AT_UV		= 1,
	AT_RGBA		= 2,
	AT_NORMAL	= 3
};

void *destroyNativeData(void *object, int32, int32);
Stream *readNativeData(Stream *stream, int32 len, void *object, int32, int32);
Stream *writeNativeData(Stream *stream, int32 len, void *object, int32, int32);
int32 getSizeNativeData(void *object, int32, int32);
void registerNativeDataPlugin(void);

void printDMA(InstanceData *inst);
void sizedebug(InstanceData *inst);

// only RW_PS2
void fixDmaOffsets(InstanceData *inst);
void unfixDmaOffsets(InstanceData *inst);
//

struct PipeAttribute
{
	const char *name;
	uint32 attrib;
};

extern PipeAttribute attribXYZ;
extern PipeAttribute attribXYZW;
extern PipeAttribute attribUV;
extern PipeAttribute attribUV2;
extern PipeAttribute attribRGBA;
extern PipeAttribute attribNormal;
extern PipeAttribute attribWeights;

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
	// RW has more:
	//  instanceTestCB()
	//  resEntryAllocCB()
	//  bridgeCB()
	//  postMeshCB()
	//  vu1code
	//  primtype

	static uint32 getVertCount(uint32 top, uint32 inAttribs,
	                           uint32 outAttribs, uint32 outBufs) {
		return (top-outBufs)/(inAttribs*2+outAttribs*outBufs);
	}

	MatPipeline(uint32 platform);
	void dump(void);
	void setTriBufferSizes(uint32 inputStride, uint32 stripCount);
	void instance(Geometry *g, InstanceData *inst, Mesh *m);
	uint8 *collectData(Geometry *g, InstanceData *inst, Mesh *m, uint8 *data[]);
};

class ObjPipeline : public rw::ObjPipeline
{
public:
	MatPipeline *groupPipeline;
	// RW has more:
	//  setupCB()
	//  finalizeCB()
	//  lightOffset
	//  lightSize

	ObjPipeline(uint32 platform);
};

struct Vertex {
	float32 p[3];
	float32 t[2];
	float32 t1[2];
	uint8   c[4];
	float32 n[3];
	// skin
	float32 w[4];
	uint8   i[4];
};

void insertVertex(Geometry *geo, int32 i, uint32 mask, Vertex *v);

extern ObjPipeline *defaultObjPipe;
extern MatPipeline *defaultMatPipe;

void genericUninstanceCB(MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[]);
void genericPreCB(MatPipeline *pipe, Geometry *geo);	// skin and ADC
//void defaultUninstanceCB(MatPipeline *pipe, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[]);
void skinInstanceCB(MatPipeline *, Geometry *g, Mesh *m, uint8 **data);
//void skinUninstanceCB(MatPipeline*, Geometry *geo, uint32 flags[], Mesh *mesh, uint8 *data[]);

ObjPipeline *makeDefaultPipeline(void);
void dumpPipeline(rw::Pipeline *pipe);

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

int8 *getADCbits(Geometry *geo);
int8 *getADCbitsForMesh(Geometry *geo, Mesh *mesh);
void convertADC(Geometry *g);
void unconvertADC(Geometry *geo);
void allocateADC(Geometry *geo);

// PDS plugin

Pipeline *getPDSPipe(uint32 data);
void registerPDSPipe(Pipeline *pipe);
void registerPDSPlugin(int32 n);
void registerPluginPDSPipes(void);

// Native Texture and Raster

struct Ps2Raster
{
	enum Flags {
		HASGIFPACKETS = 0x1,
		SWIZZLED8     = 0x2,
		SWIZZLED4     = 0x4,
	};

	uint32 tex0[2];
	uint32 paletteOffset;   // from beginning of GS data;
	                        // in words/64
	uint16 kl;
	uint8 tex1low;          // MXL and LCM of TEX1
	uint8 unk2;
	uint32 miptbp1[2];
	uint32 miptbp2[2];
	uint32 texelSize;
	uint32 paletteSize;
	uint32 gsSize;
	int8 flags;

	uint8 *data;	//tmp
	uint32 dataSize;
};

extern int32 nativeRasterOffset;
void registerNativeRaster(void);

Texture *readNativeTexture(Stream *stream);
void writeNativeTexture(Texture *tex, Stream *stream);
uint32 getSizeNativeTexture(Texture *tex);

}
}
