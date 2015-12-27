#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <rw.h>
#include <args.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;

struct {
	char *str;
	uint32 val;
} platforms[] = {
	{ "mobile", PLATFORM_OGL },
	{ "ps2",    PLATFORM_PS2 },
	{ "xbox",   PLATFORM_XBOX },
	{ "d3d8",   PLATFORM_D3D8 },
	{ "d3d9",   PLATFORM_D3D9 },
	{ NULL, 0 }
};

char *argv0;

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
fixLcsHier(HAnimHierarchy *hier)
{
	hier->maxInterpKeyFrameSize = findAnimInterpolatorInfo(1)->keyFrameSize;
	for(int32 i = 0; i < hier->numNodes; i++){
		int32 id = hier->nodeInfo[i].id;
		if(id == 255) hier->nodeInfo[i].id = -1;
		else if(id > 0x80) hier->nodeInfo[i].id |= 0x1300;
	}
}

void
usage(void)
{
	fprintf(stderr, "usage: %s [-u] [-i] [-s] [-v version] [-o platform] in.dff [out.dff]\n", argv0);
	fprintf(stderr, "\t-u uninstance\n");
	fprintf(stderr, "\t-i instance\n");
	fprintf(stderr, "\t-v RW version, e.g. 33004 for 3.3.0.4\n");
	fprintf(stderr, "\t-o output platform. ps2, xbox, mobile, d3d8, d3d9\n");
	fprintf(stderr, "\t-s expect iOS LCS dff as input\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();

	rw::version = 0;
//	rw::version = 0x34003;
//	rw::version = 0x33002;
//	rw::platform = rw::PLATFORM_PS2;
//	rw::platform = rw::PLATFORM_OGL;
//	rw::platform = rw::PLATFORM_XBOX;
	rw::platform = rw::PLATFORM_D3D8;
//	rw::platform = rw::PLATFORM_D3D9;

	int uninstance = 0;
	int instance = 0;
	int lcs = 0;
	int outplatform = rw::PLATFORM_D3D8;

	char *s;
	ARGBEGIN{
	case 'u':
		uninstance++;
		break;
	case 'i':
		instance++;
		break;
	case 'v':
		sscanf(EARGF(usage()), "%x", &rw::version);
		break;
	case 'o':
		s = EARGF(usage());
		for(int i = 0; platforms[i].str; i++){
			if(strcmp(platforms[i].str, s) == 0){
				outplatform = platforms[i].val;
				goto found;
			}
		}
		printf("unknown platform %s\n", s);
		outplatform = PLATFORM_D3D8;
	found:
		break;
	case 's':
		lcs++;
		break;
	default:
		usage();
	}ARGEND;

	if(uninstance && instance){
		fprintf(stderr, "cannot both instance and uninstance, choose one!\n");
		return 1;
	}

	matFXGlobals.hack = lcs;
	skinGlobals.forceSkipUsedBones = lcs;

	if(argc < 1)
		usage();

	Clump *c;
	//uint32 len;
	//uint8 *data = getFileContents(argv[0], &len);
	//assert(data != NULL);
	//StreamMemory in;
	//in.open(data, len);
	StreamFile in;
	in.open(argv[0], "rb");
	ChunkHeaderInfo header;
	readChunkHeaderInfo(&in, &header);
	if(header.type == ID_UVANIMDICT){
		UVAnimDictionary *dict = UVAnimDictionary::streamRead(&in);
		currentUVAnimDictionary = dict;
		readChunkHeaderInfo(&in, &header);
	}
	assert(header.type == ID_CLUMP);
	debugFile = argv[0];
	if(lcs)
		c = clumpStreamReadRsl(&in);
	else
		c = Clump::streamRead(&in);
	assert(c != NULL);
	in.close();

//	printf("%s\n", argv[arg]);

/*
	for(int32 i = 0; i < c->numAtomics; i++){
		Atomic *a = c->atomicList[i];
		Pipeline *ap = a->pipeline;
		Geometry *g = a->geometry;
		for(int32 j = 0; j < g->numMaterials; j++){
			Pipeline *mp = g->materialList[j]->pipeline;
			if(ap && mp)
				printf("%s %x %x\n", argv[arg], ap->pluginData, mp->pluginData);
		}
	}
*/

	int32 platform = findPlatform(c);
	if(platform){
		rw::platform = platform;
		switchPipes(c, rw::platform);
	}

	if(lcs){
		HAnimHierarchy *hier = getHierarchy(c);
		if(hier)
			fixLcsHier(hier);
		for(int32 i = 0; i < c->numAtomics; i++){
			Skin *skin = *PLUGINOFFSET(Skin*, c->atomicList[i]->geometry, skinGlobals.offset);
			convertRslGeometry(c->atomicList[i]->geometry);
			if(skin)
				c->atomicList[i]->pipeline = skinGlobals.pipelines[rw::platform];
		}
	}

	if(uninstance)
		for(int32 i = 0; i < c->numAtomics; i++){
			Atomic *a = c->atomicList[i];
			ObjPipeline *p = a->getPipeline();
			p->uninstance(a);
			if(outplatform != PLATFORM_PS2)
				ps2::unconvertADC(a->geometry);
		}

	rw::platform = outplatform;
	switchPipes(c, rw::platform);

	if(instance)
		for(int32 i = 0; i < c->numAtomics; i++){
			Atomic *a = c->atomicList[i];
			ObjPipeline *p = a->getPipeline();
			p->instance(a);
			if(outplatform != PLATFORM_PS2)
				ps2::convertADC(a->geometry);
		}

	if(rw::version == 0){
		rw::version = header.version;
		rw::build = header.build;
	}

	StreamFile out;
	if(argc > 1)
		assert(out.open(argv[1], "wb"));
	else
		assert(out.open("out.dff", "wb"));
	if(currentUVAnimDictionary)
		currentUVAnimDictionary->streamWrite(&out);
	c->streamWrite(&out);
	out.close();

//	data = new rw::uint8[1024*1024];
//	rw::StreamMemory out;
//	out.open(data, 0, 1024*1024);
//	if(currentUVAnimDictionary)
//		currentUVAnimDictionary->streamWrite(&out);
//	c->streamWrite(&out);
//
//	FILE *cf;
//	if(argc > 1)
//		cf = fopen(argv[1], "wb");
//	else
//		cf = fopen("out.dff", "wb");
//	assert(cf != NULL);
//	fwrite(data, out.getLength(), 1, cf);
//	fclose(cf);
//	out.close();
//	delete[] data;

	delete c;

	return 0;
}
