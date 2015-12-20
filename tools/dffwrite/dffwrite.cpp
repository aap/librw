#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <ctype.h>
#include <new>

#include <rw.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;

struct bone {
	int32 id;
	char *name;
};

bone idMapVC[] = {
	{ 0,		"Root" },
	{ 1,		"Pelvis" },
	{ 2,		"Spine" },
	{ 3,		"Spine1" },
	{ 4,		"Neck" },
	{ 5,		"Head" },
	{ 31,		"Bip01 L Clavicle" },
	{ 32,		"L UpperArm" },
	{ 33,		"L Forearm" },
	{ 34,		"L Hand" },
	{ 35,		"L Finger" },
	{ 21,		"Bip01 R Clavicle" },
	{ 22,		"R UpperArm" },
	{ 23,		"R Forearm" },
	{ 24,		"R Hand" },
	{ 25,		"R Finger" },
	{ 41,		"L Thigh" },
	{ 42,		"L Calf" },
	{ 43,		"L Foot" },
	{ 2000,		"L Toe0" },
	{ 51,		"R Thigh" },
	{ 52,		"R Calf" },
	{ 53,		"R Foot" },
	{ 2001,		"R Toe0" },
};

bone csIdMapVC[] = {
	{ 0,		"Root" },
	{ 1,		"Pelvis" },
	{ 2,		"Spine" },
	{ 3,		"Spine1" },
	{ 4,		"Neck" },
	{ 5,		"Head" },
	{ 2000,		"HeadNub" },		// "Bip01 HeadNub"
	{ 5006,		"llid" },
	{ 5005,		"rlid" },
	{ 5001,		"rbrow1" },
	{ 5002,		"rbrow2" },
	{ 5004,		"lbrow1" },
	{ 5003,		"lbrow2" },
	{ 5008,		"lcheek" },
	{ 5015,		"rcorner" },
	{ 5016,		"lcorner" },
	{ 5017,		"jaw1" },
	{ 5018,		"jaw2" },
	{ 5019,		"llip" },
	{ 5020,		"llip01" },
	{ 5012,		"ltlip1" },
	{ 5013,		"ltlip2" },
	{ 5014,		"ltlip3" },
	{ 5009,		"rtlip1" },
	{ 5010,		"rtlip2" },
	{ 5011,		"rtlip3" },
	{ 5007,		"rcheek" },
	{ 5021,		"reye" },
	{ 5022,		"leye" },
	{ 31,		"L Clavicle" },		// "Bip01 L Clavicle"
	{ 32,		"L UpperArm" },
	{ 33,		"L Forearm" },
	{ 34,		"L Hand" },
	{ 35,		"L Finger0" },		// "L Finger"
	{ 36,		"L Finger01" },		// "Root L Finger01"
	{ 2001,		"L Finger0Nub" },	// "Bip01 L Finger0Nub"
	{ 21,		"R Clavicle" },		// "Bip01 R Clavicle"
	{ 22,		"R UpperArm" },
	{ 23,		"R Forearm" },
	{ 24,		"R Hand" },
	{ 25,		"R Finger0" },		// "R Finger"
	{ 26,		"R Finger01" },		// "Root R Finger01"
	{ 2002,		"R Finger0Nub" },	// "Bip01 R Finger0Nub"
	{ 41,		"L Thigh" },
	{ 42,		"L Calf" },
	{ 43,		"L Foot" },
	{ 2003,		"L Toe0" },
	{ 2004,		"L Toe0Nub" },		// "Bip01 L Toe0Nub"
	{ 51,		"R Thigh" },
	{ 52,		"R Calf" },
	{ 53,		"R Foot" },
	{ 2005,		"R Toe0" },
	{ 2006,		"R Toe0Nub" },		// "Bip01 R Toe0Nub"
};

int
strcmpci(const char *s, const char *t)
{
	while(*s && *t && tolower(*s) == tolower(*t)){
		s++;
		t++;
	}
	return *s-*t;
}

Frame*
assignIdCB(Frame *f, void *p)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, f, hAnimOffset);
	char *name = PLUGINOFFSET(char, f, gta::nodeNameOffset);
	HAnimHierarchy *hier = (HAnimHierarchy*)p;
	bone *map = hier->numNodes == 24 ? idMapVC : csIdMapVC;
	for(int32 i = 0; i < hier->numNodes; i++){
		if(strcmpci(name, map[i].name) == 0){
			hanim->id = map[i].id;
			break;
		}
	}
	f->forAllChildren(assignIdCB, hier);
	return f;
}

Frame*
findHierCB(Frame *f, void *p)
{
	HAnimData *hanim = PLUGINOFFSET(HAnimData, f, hAnimOffset);
	if(hanim->hierarchy){
		*(HAnimHierarchy**)p = hanim->hierarchy;
		return NULL;
	}
	f->forAllChildren(findHierCB, p);
	return f;
}

HAnimHierarchy*
getHierarchy(Clump *c)
{
	HAnimHierarchy *hier = NULL;
	findHierCB((Frame*)c->parent, &hier);
	return hier;
}

void
assignNodeIDs(Clump *c)
{
	HAnimHierarchy *hier = getHierarchy(c);
	if(hier == NULL || (hier->numNodes != 24 && hier->numNodes != 53))
		return;
	bone *map = hier->numNodes == 24 ? idMapVC : csIdMapVC;
	for(int32 i = 0; i < hier->numNodes; i++)
		hier->nodeInfo[i].id = map[hier->nodeInfo[i].index].id;
	assignIdCB((Frame*)c->parent, hier);
}

void
mirrorFrameHier(Frame *f)
{
	if(f->next){
		Frame *n = f->next;
		Frame *p = (Frame*)f->parent;
		f->removeChild();
		mirrorFrameHier(n);
		p->addChild(f);
	}
	if(f->child)
		mirrorFrameHier(f->child);
}

void
generateInvBoneMats(Atomic *a)
{
	float32 rootInv[16], tmp[16];
	HAnimHierarchy *h = getHierarchy(a->clump);
	Frame *root = h->parentFrame;
	root->updateLTM();
	matrixInvert(rootInv, root->ltm);
	Skin *skin = *PLUGINOFFSET(Skin*, a->geometry, skinGlobals.offset);
	assert(skin->numBones == h->numNodes);
	for(int32 i = 0; i < h->numNodes; i++){
		//assert(h->nodeInfo[i].frame);
		if(h->nodeInfo[i].frame == NULL){
			printf("warning: no node for node %d/%d\n", i, h->nodeInfo[i].id);
			continue;
		}
		h->nodeInfo[i].frame->updateLTM();
		float32 *m = &skin->inverseMatrices[i*16];
		matrixMult(tmp, rootInv, h->nodeInfo[i].frame->ltm);
		matrixInvert(m, tmp);
	}
}

Frame*
findFrameById(Frame *f, int32 id)
{
	if(f == NULL)
		return NULL;
	HAnimData *hanim = PLUGINOFFSET(HAnimData, f, hAnimOffset);
	if(hanim->id == id)
		return f;
	Frame *ff = findFrameById(f->next, id);
	if(ff) return ff;
	return findFrameById(f->child, id);
}

void
attachFrames(Clump *c)
{
	HAnimHierarchy *h = getHierarchy(c);
	for(int32 i = 0; i < h->numNodes; i++)
		h->nodeInfo[i].frame = findFrameById(h->parentFrame, h->nodeInfo[i].id);
}

int
main(int argc, char *argv[])
{
//	rw::version = 0x31000;
//	rw::build = 0;

//	rw::version = 0;
//	rw::version = 0x32000;
	rw::version = 0x33002;
	rw::platform = PLATFORM_D3D8;
//	rw::version = 0x30200;

	int lcs = 1;
	matFXGlobals.hack = lcs;

	gta::attachPlugins();

	rw::Clump *c;

	if(argc < 2){
		printf("usage: %s input.dff [output.dff]\n", argv[0]);
		return 0;
	}

//	rw::StreamFile in;
//	in.open(argv[1], "rb");

	ChunkHeaderInfo header;
	if(0){
		FILE *cf = fopen(argv[1], "rb");
		assert(cf != NULL);
		fseek(cf, 0, SEEK_END);
		rw::uint32 len = ftell(cf);
		fseek(cf, 0, SEEK_SET);
		rw::uint8 *data = new rw::uint8[len];
		fread(data, len, 1, cf);
		fclose(cf);
		rw::StreamMemory in;
		in.open(data, len);
	
		rw::findChunk(&in, rw::ID_CLUMP, NULL, NULL);
		rw::debugFile = argv[1];
		c = rw::Clump::streamRead(&in);
		assert(c != NULL);
	
		in.close();
		delete[] data;
	}else{
		rw::StreamFile in;
		if(in.open(argv[1], "rb") == NULL){
			printf("couldn't open file\n");
			return 1;
		}
		debugFile = argv[1];
		readChunkHeaderInfo(&in, &header);
		if(header.type == ID_UVANIMDICT){
			UVAnimDictionary *dict = UVAnimDictionary::streamRead(&in);
			currentUVAnimDictionary = dict;
			readChunkHeaderInfo(&in, &header);
		}
		assert(header.type == ID_CLUMP);
		if(lcs)
			c = clumpStreamReadRsl(&in);
		else
			c = Clump::streamRead(&in);
		assert(c != NULL);
	}

	if(lcs){
		HAnimHierarchy *hier = getHierarchy(c);
		if(hier)
			hier->maxInterpKeyFrameSize = findAnimInterpolatorInfo(1)->keyFrameSize;
		mirrorFrameHier((Frame*)c->parent);
		assignNodeIDs(c);
		attachFrames(c);
		for(int32 i = 0; i < c->numAtomics; i++){
			Skin *skin = *PLUGINOFFSET(Skin*, c->atomicList[i]->geometry, skinGlobals.offset);
			convertRslGeometry(c->atomicList[i]->geometry);
			if(skin){
				c->atomicList[i]->pipeline = skinGlobals.pipelines[rw::platform];
				generateInvBoneMats(c->atomicList[i]);
			}
		}	
	}

	if(rw::version == 0){
		rw::version = header.version;
		rw::build = header.build;
	}

//	rw::Image::setSearchPath("./;/home/aap/gamedata/ps2/gtavc/MODELS/gta3_archive/txd_extracted/");

/*
//	rw::Image *tga = rw::readTGA("b.tga");
	rw::Image *tga = rw::readTGA("player.tga");
	assert(tga != NULL);
	rw::writeTGA(tga, "out.tga");
*/

//	for(rw::int32 i = 0; i < c->numAtomics; i++)
//		rw::Gl::Instance(c->atomicList[i]);

	if(0){
		uint8 *data = new rw::uint8[1024*1024];
		rw::StreamMemory out;
		out.open(data, 0, 1024*1024);

		c->streamWrite(&out);

		FILE *cf = fopen(argv[2], "wb");
		assert(cf != NULL);
		fwrite(data, out.getLength(), 1, cf);
		fclose(cf);
		out.close();
		delete[] data;
	}else{
		rw::StreamFile out;
		if(argc > 2)
			out.open(argv[2], "wb");
		else
			out.open("out.dff", "wb");
//		if(lcs)
//			clumpStreamWriteRsl(&out, c);
//		else
			c->streamWrite(&out);
		out.close();
	}

	delete c;

	return 0;

}
