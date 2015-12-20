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
	skinGlobals.forceSkipUsedBones = lcs;

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
			fixLcsHier(hier);
		for(int32 i = 0; i < c->numAtomics; i++){
			Skin *skin = *PLUGINOFFSET(Skin*, c->atomicList[i]->geometry, skinGlobals.offset);
			convertRslGeometry(c->atomicList[i]->geometry);
			if(skin)
				c->atomicList[i]->pipeline = skinGlobals.pipelines[rw::platform];
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
