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

static struct {
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

void
usage(void)
{
	fprintf(stderr, "usage: %s [-u] [-i] [-s] [-v version] [-o platform] in.dff [out.dff]\n", argv0);
	fprintf(stderr, "\t-u uninstance\n");
	fprintf(stderr, "\t-i instance\n");
	fprintf(stderr, "\t-v RW version, e.g. 33004 for 3.3.0.4\n");
	fprintf(stderr, "\t-o output platform. ps2, xbox, mobile, d3d8, d3d9\n");
	exit(1);
}

void
dumpUVAnim(Animation *anim)
{
	UVAnimCustomData *cust = (UVAnimCustomData*)anim->customData;
	printf(" %s", cust->name);
	for(int i = 0; i < 8; i++)
		printf(" %d", cust->nodeToUVChannel[i]);
	printf("\n %d %x\n", anim->numFrames, anim->interpInfo->id);
	UVAnimKeyFrame *kf = (UVAnimKeyFrame*)anim->keyframes;
	for(int i = 0; i < anim->numFrames; i++){
		printf(" %f\n", kf->time);
		printf("  %f %f %f %f %f %f\n", kf->uv[0], kf->uv[1], kf->uv[2], kf->uv[3], kf->uv[4], kf->uv[5]);
		kf++;
	}
}

void
dumpFrameHier(Frame *frame, int ind = 0)
{
	for(int i = 0; i < ind; i++)
		printf("  ");
	char *name = gta::getNodeName(frame);
	HAnimData *hanim = HAnimData::get(frame);
	printf("*%s %d %d %s\n", name[0] ? name : "---", frame->objectList.count(), hanim->id, hanim->hierarchy ? "HIERARCHY" : "");
	if(hanim->hierarchy){
		HAnimHierarchy *h = hanim->hierarchy;
		for(int i = 0; i < h->numNodes; i++){
			name = h->nodeInfo[i].frame ? gta::getNodeName(h->nodeInfo[i].frame) : "";
			printf("\t\t%d %d\t%p %s\n", h->nodeInfo[i].id, h->nodeInfo[i].flags, h->nodeInfo[i].frame, name);

				{
				h->nodeInfo[i].frame->updateLTM();
				float *mat = h->nodeInfo[i].frame->ltm;
				printf("[ [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
				       "  [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
				       "  [ %8.4f, %8.4f, %8.4f, %8.4f ]\n"
				       "  [ %8.4f, %8.4f, %8.4f, %8.4f ] ]\n",
					mat[0], mat[4], mat[8], mat[12],
					mat[1], mat[5], mat[9], mat[13],
					mat[2], mat[6], mat[10], mat[14],
					mat[3], mat[7], mat[11], mat[15]);
				}
		}
	}
	for(Frame *child = frame->child;
	    child; child = child->next)
		dumpFrameHier(child, ind+1);
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
	default:
		usage();
	}ARGEND;

	if(uninstance && instance){
		fprintf(stderr, "cannot both instance and uninstance, choose one!\n");
		return 1;
	}

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
	currentUVAnimDictionary = NULL;
	currentTexDictionary = TexDictionary::create();
	ChunkHeaderInfo header;
	readChunkHeaderInfo(&in, &header);
	if(header.type == ID_UVANIMDICT){
		UVAnimDictionary *dict = UVAnimDictionary::streamRead(&in);
		currentUVAnimDictionary = dict;
		readChunkHeaderInfo(&in, &header);
	}
	if(header.type != ID_CLUMP){
		in.close();
		return 0;
	}
	debugFile = argv[0];
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
	//FORLIST(lnk, c->lights){
	//	Light *l = Light::fromClump(lnk);
	//	printf("%p %p\n", l, lnk);
	//	printf("%d %f %f %f\n", l->getType(), l->color.red, l->color.green, l->color.blue);
	//}

	HAnimHierarchy *hier = HAnimHierarchy::find(c->getFrame());
	if(hier)
		hier->attach();
	dumpFrameHier(c->getFrame());

	//if(currentUVAnimDictionary){
	//	FORLIST(lnk, currentUVAnimDictionary->animations){
	//		UVAnimDictEntry *de = UVAnimDictEntry::fromDict(lnk);
	//		Animation *anim = de->anim;
	//		dumpUVAnim(anim);
	//	}
	//}

	int32 platform = findPlatform(c);
	if(platform){
		rw::platform = platform;
		switchPipes(c, rw::platform);
	}

	if(uninstance)
		FORLIST(lnk, c->atomics){
			Atomic *a = Atomic::fromClump(lnk);
			ObjPipeline *p = a->getPipeline();
			p->uninstance(a);
			if(outplatform != PLATFORM_PS2)
				ps2::unconvertADC(a->geometry);
		}

	rw::platform = outplatform;
	switchPipes(c, rw::platform);

	if(instance)
		FORLIST(lnk, c->atomics){
			Atomic *a = Atomic::fromClump(lnk);
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

	c->destroy();
	if(currentUVAnimDictionary)
		currentUVAnimDictionary->destroy();
	currentTexDictionary->destroy();

	return 0;
}
