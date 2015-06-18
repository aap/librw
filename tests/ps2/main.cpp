#include <cstdio>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cassert>

#include <rw.h>
#include <src/gtaplg.h>

#include "ps2.h"
#include "dma.h"
#include "gif.h"
#include "gs.h"

#include "math.h"

using namespace std;

Matrix projMat, viewMat, worldMat;

extern uint32 MyDmaPacket[];
extern Matrix vuMat;
extern Matrix vuLightMat;
extern float  vuOffset[];
extern uint64 vuGIFtag[];
extern float  vuMatcolor[];
extern float  vuSurfProps[];
extern uint32 vuGeometry[];
extern uint32 mpgCall[];
extern uint32 geometryCall[];
extern uint32 defaultPipe[];
extern uint32 skinPipe[];

rw::Clump *clump;

void
drawAtomic(rw::Atomic *atomic)
{
	rw::Geometry *geo = atomic->geometry;
	assert(geo->instData != NULL);
	rw::ps2::InstanceDataHeader *instData =
	  (rw::ps2::InstanceDataHeader*)geo->instData;
	rw::MeshHeader *meshHeader = geo->meshHeader;
	rw::Mesh *mesh;
	uint8 *color;

	atomic->frame->updateLTM();
	matCopy(vuLightMat, atomic->frame->ltm);
	matMult(vuMat, atomic->frame->ltm);
	rw::Skin *skin = *PLUGINOFFSET(rw::Skin*, geo, rw::skinGlobals.offset);
	for(uint i = 0; i < instData->numMeshes; i++){
		if(instData->instanceMeshes[i].arePointersFixed == 0)
			rw::ps2::fixDmaOffsets(&instData->instanceMeshes[i]);
		geometryCall[1] = (uint32)instData->instanceMeshes[i].data;
		mesh = &meshHeader->mesh[i];
		color = mesh->material->color;

		vuGIFtag[0] = MAKE_GIF_TAG(0,1,1,0xC|0x10,0,3);
		vuGIFtag[1] = 0x412;
		vuMatcolor[0] = color[0]/255.0f;
		vuMatcolor[1] = color[1]/255.0f;
		vuMatcolor[2] = color[2]/255.0f;
		vuMatcolor[3] = color[3]/2.0f/255.0f;
		// only when modulating textures actually
		vuMatcolor[0] /= 2.0f;
		vuMatcolor[1] /= 2.0f;
		vuMatcolor[2] /= 2.0f;
		if(rw::skinGlobals.offset && skin){
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

	matMakeIdentity(viewMat);
//	matTranslate(viewMat, 0.0f, 0.0f, -34.0f);
//	matTranslate(viewMat, 0.0f, 0.0f, -10.0f);
	matTranslate(viewMat, 0.0f, 0.0f, -8.0f);
	matRotateX(viewMat, rot);
	matRotateY(viewMat, rot);
	matRotateZ(viewMat, rot);

	matCopy(vuMat, projMat);
	matMult(vuMat, viewMat);

	for(int i = 0; i < clump->numAtomics; i++){
		char *name = PLUGINOFFSET(char, clump->atomicList[i]->frame,
		                          gta::nodeNameOffset);
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

GIF_DECLARE_PACKET(gifDmaBuf2, 256)

uint32
uploadTGA(rw::Image *tga)
{
	GsState *g = gsCurState;
	uint32 destAddr = g->currentMemPtr;
	uint32 size = tga->width*tga->height*4;
	g->currentMemPtr += size;

	printf("image @ %x\n", destAddr);

	GIF_BEGIN_PACKET(gifDmaBuf2);
	GIF_TAG(gifDmaBuf2, 4, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf2, GS_BITBLTBUF,
	            MAKE_GS_BITBLTBUF(0, 0, 0,
	                              destAddr/4/64, tga->width/64, PSMCT32));
	GIF_DATA_AD(gifDmaBuf2, GS_TRXPOS,
	            MAKE_GS_TRXPOS(0, 0, 0, 0, 0));
	GIF_DATA_AD(gifDmaBuf2, GS_TRXREG,
	            MAKE_GS_TRXREG(tga->width, tga->height));
	GIF_DATA_AD(gifDmaBuf2, GS_TRXDIR, 0);
	GIF_SEND_PACKET(gifDmaBuf2);

	GIF_BEGIN_PACKET(gifDmaBuf2);
	GIF_TAG(gifDmaBuf2, size/0x10, 1, 0, 0, 2, 0, 0);
	GIF_SEND_PACKET(gifDmaBuf2);

	FlushCache(0);
	SET_REG32(D2_QWC,  MAKE_DN_QWC(size/0x10));
	SET_REG32(D2_MADR, MAKE_DN_MADR(tga->pixels, 0));
	SET_REG32(D2_CHCR, MAKE_DN_CHCR(1, 0, 0, 0, 0, 1));
	DMA_WAIT(D2_CHCR);

	int logw = 0, logh = 0;
	int s;
	for(s = 1; s < tga->width; s *= 2)
		logw++;
	for(s = 1; s < tga->height; s *= 2)
		logh++;

	GIF_BEGIN_PACKET(gifDmaBuf2);
	GIF_TAG(gifDmaBuf2, 3, 1, 0, 0, 0, 1, 0x0e);
	GIF_DATA_AD(gifDmaBuf2, GS_TEXFLUSH, 1);
	GIF_DATA_AD(gifDmaBuf2, GS_TEX0_1,
	            MAKE_GS_TEX0(destAddr/4/64, tga->width/64, PSMCT32,
	                         logw, logh, 0, 0, 0, 0, 0, 0, 0));
	GIF_DATA_AD(gifDmaBuf2, GS_TEX1_1,
	            MAKE_GS_TEX1(0, 0, 1, 1, 0, 0, 0));
	GIF_SEND_PACKET(gifDmaBuf2);

	return destAddr;
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

	gta::registerEnvSpecPlugin();
	rw::registerMatFXPlugin();
	rw::registerMaterialRightsPlugin();
	rw::registerAtomicRightsPlugin();
	rw::registerHAnimPlugin();
	gta::registerNodeNamePlugin();
	gta::registerBreakableModelPlugin();
	gta::registerExtraVertColorPlugin();
	rw::ps2::registerADCPlugin();
	rw::registerSkinPlugin();
	rw::registerNativeDataPlugin();
//	rw::ps2::registerNativeDataPlugin();
	rw::registerMeshPlugin();

	rw::uint32 len;
	rw::uint8 *data = rw::getFileContents("host:player-vc-ps2.dff", &len);
//	rw::uint8 *data = rw::getFileContents("host:od_newscafe_dy-ps2.dff", &len);
//	rw::uint8 *data = rw::getFileContents("host:admiral-ps2.dff", &len);
	rw::StreamMemory in;
	in.open(data, len);
	rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL);
	clump = rw::Clump::streamRead(&in);
	in.close();
	delete[] data;

	data = new rw::uint8[256*1024];
	rw::StreamMemory out;
	out.open(data, 0, 256*1024);
	clump->streamWrite(&out);
//	cf = fopen("host:out-ps2.dff", "wb");
//	assert(cf != NULL);
//	fwrite(data, out.getLength(), 1, cf);
//	fclose(cf);
	out.close();
	delete[] data;

	rw::Image *tga = rw::readTGA("host:player_.tga");
	printf("read tga\n");

	uint32 destAddr = uploadTGA(tga);
/*
	rw::writeTGA(tga, "host:out.tga");
	printf("wrote tga\n");
*/


	vuOffset[0] = 2048.0f;
	vuOffset[1] = 2048.0f;

	for(int i = 0; i < clump->numAtomics; i++){
		rw::Atomic *a = clump->atomicList[i];
		char *name = 
		  PLUGINOFFSET(char, a->frame, gta::nodeNameOffset);
		printf("%s\n", name);
	}

//	gsDumpState();

//	matPerspective(mathProjectionMat, 60.0f, (float)gss.width/gss.height,
//	matPerspective(mathProjectionMat, 60.0f, (float)gss.width/gss.dh,
//	matPerspective(mathProjectionMat, 60.0f, 4.0/3.0, 1.0f, 100.0f);
//	matPerspective(mathProjectionMat, 60.0f, 16.0/9.0, 1.0f, 100.0f);
	matPerspective2(projMat, 60.0f, 4.0/3.0, gss.width,gss.height,
	                1.0f, 100.0f, 16777216, 1);

	for(;;){
		draw();
		gsFlip();
	}

	return 0;
}
