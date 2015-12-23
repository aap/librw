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
		printf("usage: %s in\n", argv[0]);
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

	int largefile = rslstr->dataSize > 0x100000;

	World *world;
	RslClump *clump;
	Sector *sector;
	if(rslstr->ident = WRLD_IDENT && largefile){	// hack
		world = (World*)rslstr->data;

		int len = strlen(argv[1])+1;
		char filename[1024];
		strncpy(filename, argv[1], len);
		filename[len-3] = 'i';
		filename[len-2] = 'm';
		filename[len-1] = 'g';
		filename[len] = '\0';
		assert(stream.open(filename, "rb"));
		filename[len-4] = '\\';
		filename[len-3] = '\0';

		char name[1024];
		uint8 *data;
		StreamFile outf;
		RslStreamHeader *h;
		int i = 0;
		for(h = world->sectors->sector; h->ident == WRLD_IDENT; h++){
			sprintf(name, "world%04d.wrld", i++);
			strcat(filename, name);
			assert(outf.open(filename, "wb"));
			data = new uint8[h->fileEnd];
			memcpy(data, h, 0x20);
			stream.seek(h->root, 0);
			stream.read(data+0x20, h->fileEnd-0x20);
			outf.write(data, h->fileEnd);
			outf.close();
			filename[len-3] = '\0';
		}
		// radar textures
		h = world->textures;
		for(i = 0; i < world->numTextures; i++){
			sprintf(name, "txd%04d.chk", i);
			strcat(filename, name);
			assert(outf.open(filename, "wb"));
			data = new uint8[h->fileEnd];
			memcpy(data, h, 0x20);
			stream.seek(h->root, 0);
			stream.read(data+0x20, h->fileEnd-0x20);
			outf.write(data, h->fileEnd);
			outf.close();
			filename[len-3] = '\0';
			h++;
		}
		stream.close();
	}else if(rslstr->ident = WRLD_IDENT){	// sector
		sector = (Sector*)rslstr->data;
		printf("resources\n");
		for(uint32 i = 0; i < sector->numResources; i++){
			OverlayResource *r = &sector->resources[i];
			printf(" %d %p\n", r->id, r->raw);
		}
		printf("placement\n");
		Placement *p;
		for(p = sector->sectionA; p < sector->sectionEnd; p++){
			printf(" %d, %d, %f %f %f\n", p->id &0x7FFF, p->resId, p->matrix[12], p->matrix[13], p->matrix[14]);
		}
	}else if(rslstr->ident = MDL_IDENT){
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
