#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <rw.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;
#include "rsl.h"

void
RslStream::relocate(void)
{
	uint32 off = (uint32)this->data;
	off -= 0x20;
	*(uint32*)&this->reloc += off;
	*(uint32*)&this->hashTab += off;

	uint8 **rel = (uint8**)this->reloc;
	for(uint32 i = 0; i < this->relocSize; i++){
		rel[i] += off;
		*this->reloc[i] += off;
	}
}

RslFrame*
RslFrameForAllChildren(RslFrame *frame, RslFrameCallBack callBack, void *data)
{
	for(RslFrame *child = frame->child;
	    child;
	    child = child->next)
		if(callBack(child, data) == NULL)
			break;
	return frame;
}

RslClump*
RslClumpForAllAtomics(RslClump *clump, RslAtomicCallBack callback, void *pData)
{
	RslAtomic *a;
	RslLLLink *link;
	for(link = rslLLLinkGetNext(&clump->atomicList.link);
	    link != &clump->atomicList.link;
	    link = link->next){
		a = rslLLLinkGetData(link, RslAtomic, inClumpLink);
		if(callback(a, pData) == NULL)
			break;
	}
	return clump;
}

RslGeometry*
RslGeometryForAllMaterials(RslGeometry *geometry, RslMaterialCallBack fpCallBack, void *pData)
{
	for(int32 i = 0; i < geometry->matList.numMaterials; i++)
		if(fpCallBack(geometry->matList.materials[i], pData) == NULL)
			break;
	return geometry;
}


RslFrame *dumpFrameCB(RslFrame *frame, void *data)
{
	printf(" frm: %x %s %x\n", frame->nodeId, frame->name, frame->hierId);
	RslFrameForAllChildren(frame, dumpFrameCB, data);
	return frame;
}

RslMaterial *dumpMaterialCB(RslMaterial *material, void*)
{
	printf("  mat: %s %x\n", material->texname, material->refCount);
	if(material->matfx){
		RslMatFX *fx = material->matfx;
		printf("   matfx: ", fx->effectType);
		if(fx->effectType == 2)
			printf("env[%s %f] ", fx->env.texname, fx->env.intensity);
		printf("\n");
	}
	return material;
}

RslAtomic *dumpAtomicCB(RslAtomic *atomic, void*)
{
	printf(" atm: %x %x %x %p\n", atomic->unk1, atomic->unk2, atomic->unk3, atomic->hier);
	RslGeometry *g = atomic->geometry;
	RslGeometryForAllMaterials(g, dumpMaterialCB, NULL);
	return atomic;
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();
	rw::version = 0;

	assert(sizeof(void*) == 4);

	if(argc < 2){
		printf("usage: %s [-u] in.dff\n", argv[0]);
		return 0;
	}

	StreamFile stream;
	assert(stream.open(argv[1], "rb"));
	RslStream *rslstr = new RslStream;
	stream.read(rslstr, 0x20);
	rslstr->data = new uint8[rslstr->fileSize-0x20];
	stream.read(rslstr->data, rslstr->fileSize-0x20);
	stream.close();
	rslstr->relocate();

	RslClump *clump;
	if(rslstr->numAtomics){
		uint8 *p = *rslstr->hashTab;
		p -= 0x24;
		RslAtomic *a = (RslAtomic*)p;
		clump = a->clump;
		if(clump)
			//RslClumpForAllAtomics(clump, dumpAtomicCB, NULL);
			RslFrameForAllChildren(RslClumpGetFrame(clump), dumpFrameCB, NULL);
		else
			//dumpAtomicCB(a, NULL);
			RslFrameForAllChildren(RslAtomicGetFrame(a), dumpFrameCB, NULL);
	}

	return 0;
}
