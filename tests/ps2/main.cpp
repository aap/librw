#include <fstream>

#include <stdio.h>
#include "ps2.h"
#include "dma.h"
#include "gif.h"
#include "gs.h"

#include "math.h"
#include "mesh.h"

#include <rw.h>
#include <src/gtaplg.h>

using namespace std;

extern uint32 MyDmaPacket[];
extern Matrix vuMat;
extern float  vuOffset[];
extern uint64 vuGIFtag[];
extern float  vuMatcolor[];
extern float  vuSurfProps[];
extern uint32 vuGeometry[];
extern uint32 mpgCall[];
extern uint32 geometryCall[];
extern uint32 defaultPipe[];
extern uint32 skinPipe[];

Rw::Clump *clump;

void
drawAtomic(Rw::Atomic *atomic)
{
	Rw::Geometry *geo = atomic->geometry;
	assert(geo->instData != NULL);
	Rw::Ps2::InstanceDataHeader *instData =
	  (Rw::Ps2::InstanceDataHeader*)geo->instData;

	atomic->frame->updateLTM();
	matMult(vuMat, atomic->frame->ltm);
	for(int i = 0; i < instData->numMeshes; i++){
		if(instData->instanceMeshes[i].arePointersFixed == 0)
			Rw::Ps2::fixDmaOffsets(&instData->instanceMeshes[i]);
		geometryCall[1] = (uint32)instData->instanceMeshes[i].data;

		vuGIFtag[0] = MAKE_GIF_TAG(0,1,1,0xC,0,2);
		vuGIFtag[1] = 0x41;
		vuMatcolor[0] = 1.0f;
		vuMatcolor[1] = 1.0f;
		vuMatcolor[2] = 1.0f;
		vuMatcolor[3] = 0.5f;
		Rw::Skin *skin =
		  *PLUGINOFFSET(Rw::Skin*, geo, Rw::SkinGlobals.offset);
		if(Rw::SkinGlobals.offset && skin){
			geometryCall[3] = 0x020000DC;
			mpgCall[1] = (uint32)skinPipe;
		}else{
			geometryCall[3] = 0x02000114;
			mpgCall[1] = (uint32)defaultPipe;
		}

		SET_REG32(D1_QWC,  0x00);
		SET_REG32(D1_TADR, (uint)MyDmaPacket & 0x0FFFFFFF);
		FlushCache(0);
		SET_REG32(D1_CHCR, MAKE_DN_CHCR(1, 1, 0, 1, 0, 1));
		DMA_WAIT(D1_CHCR);
	}

}

void
draw(void)
{
	Matrix m;
	static float rot = 0.0f;

	gsClear();

	matMakeIdentity(mathModelViewMat);
	matTranslate(mathModelViewMat, 0.0f, 0.0f, -34.0f);
//	matTranslate(mathModelViewMat, 0.0f, 0.0f, -10.0f);
//	matTranslate(mathModelViewMat, 0.0f, 0.0f, -8.0f);
	matRotateX(mathModelViewMat, rot);
	matRotateY(mathModelViewMat, rot);
	matRotateZ(mathModelViewMat, rot);
	matInverse(mathNormalMat, mathModelViewMat);

	matCopy(vuMat, mathProjectionMat);
	matMult(vuMat, mathModelViewMat);

	for(int i = 0; i < clump->numAtomics; i++){
		char *name = PLUGINOFFSET(char, clump->atomicList[i]->frame,
		                          Rw::NodeNameOffset);
		if(strstr(name, "_dam") || strstr(name, "_vlo"))
			continue;
		matCopy(m, vuMat);
		drawAtomic(clump->atomicList[i]);
		matCopy(vuMat, m);
	}

	rot += 0.01f;
	if(rot > 2*M_PI)
		rot -= 2*M_PI;
}

int
main()
{
	GsState gss;
	gsCurState = &gss;

	dmaReset(1);

//	gsInitState(NONINTERLACED, PAL, FRAME);
//	gsInitState(INTERLACED, PAL, FRAME);
	gsInitState(INTERLACED, PAL, FIELD);
	gsCurState->clearcol = 0x80404040;

	gsInit();
	printf("hallo\n");

	Rw::RegisterMaterialRightsPlugin();
	Rw::RegisterMatFXPlugin();
	Rw::RegisterAtomicRightsPlugin();
	Rw::RegisterNodeNamePlugin();
	Rw::RegisterBreakableModelPlugin();
	Rw::RegisterExtraVertColorPlugin();
	Rw::Ps2::RegisterADCPlugin();
	Rw::RegisterSkinPlugin();
	Rw::RegisterNativeDataPlugin();
//      Rw::Ps2::RegisterNativeDataPlugin();
	Rw::RegisterMeshPlugin();

//	Rw::StreamFile in;
//	in.open("host:player-vc-ps2.dff", "rb");

//	FILE *cf = fopen("host:player-vc-ps2.dff", "rb");
	FILE *cf = fopen("host:od_newscafe_dy-ps2.dff", "rb");
//	FILE *cf = fopen("host:admiral-ps2.dff", "rb");
	assert(cf != NULL);
	fseek(cf, 0, SEEK_END);
	Rw::uint32 len = ftell(cf);
	fseek(cf, 0, SEEK_SET);
	Rw::uint8 *data = new Rw::uint8[len];
	fread(data, len, 1, cf);
	fclose(cf);
	Rw::StreamMemory in;
	in.open(data, len);

	printf("opened file\n");
	Rw::FindChunk(&in, Rw::ID_CLUMP, NULL, NULL);
	printf("found chunk\n");
	clump = Rw::Clump::streamRead(&in);
	printf("read file\n");
	in.close();
	printf("closed file\n");
	delete[] data;

	data = new Rw::uint8[256*1024];
	Rw::StreamMemory out;
	out.open(data, 0, 256*1024);
	clump->streamWrite(&out);
//	cf = fopen("host:out-ps2.dff", "wb");
//	assert(cf != NULL);
//	fwrite(data, out.getLength(), 1, cf);
//	fclose(cf);
	out.close();
	delete[] data;

//	Rw::StreamFile out;
//	out.open("host:out-ps2.dff", "wb");
//	c->streamWrite(&out);
//	out.close();
//
//	printf("wrote file\n");

	vuOffset[0] = 2048.0f;
	vuOffset[1] = 2048.0f;

	for(int i = 0; i < clump->numAtomics; i++){
		Rw::Atomic *a = clump->atomicList[i];
		char *name = 
		  PLUGINOFFSET(char, a->frame, Rw::NodeNameOffset);
		printf("%s\n", name);
	}

//	gsDumpState();

//	matPerspective(mathProjectionMat, 60.0f, (float)gss.width/gss.height,
//	matPerspective(mathProjectionMat, 60.0f, (float)gss.width/gss.dh,
//	matPerspective(mathProjectionMat, 60.0f, 4.0/3.0, 1.0f, 100.0f);
//	matPerspective(mathProjectionMat, 60.0f, 16.0/9.0, 1.0f, 100.0f);
	matPerspective2(mathProjectionMat, 60.0f, 4.0/3.0, gss.width,gss.height,
	                1.0f, 100.0f, 16777216, 1);

	for(;;){
		draw();
		gsFlip();
	}

	return 0;
}
