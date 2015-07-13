#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include "rw.h"
#include "src/gtaplg.h"

using namespace std;
using namespace rw;

int
main(int argc, char *argv[])
{
	rw::version = 0x33002;
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
	rw::registerMeshPlugin();

	rw::platform = rw::PLATFORM_PS2;

	rw::Pipeline *defpipe = rw::ps2::makeDefaultPipeline();
//	rw::Pipeline *skinpipe = rw::ps2::makeSkinPipeline();
//	rw::ps2::dumpPipeline(defpipe);
//	rw::ps2::dumpPipeline(skinpipe);

	int uninstance = 0;
	int arg = 1;

	if(argc < 2){
		printf("usage: %s [-u] ps2.dff\n", argv[0]);
		return 0;
	}

	if(strcmp(argv[arg], "-u") == 0){
		uninstance++;
		arg++; 
		if(argc < 3){
			printf("usage: %s [-u] ps2.dff\n", argv[0]);
			return 0;
		}
	}

	Clump *c;
	uint32 len;
	uint8 *data = getFileContents(argv[arg], &len);
	assert(data != NULL);
	StreamMemory in;
	in.open(data, len);
	findChunk(&in, ID_CLUMP, NULL, NULL);
	debugFile = argv[arg];
	c = Clump::streamRead(&in);
	assert(c != NULL);

	printf("%s\n", argv[arg]);
	for(int32 i = 0; i < c->numAtomics; i++){
		Atomic *a = c->atomicList[i];
		if(a->pipeline){
			printf("has pipeline %x %x %x\n",
				a->pipeline->pluginID,
				a->pipeline->pluginData,
				a->pipeline->platform);
			if(uninstance)
				a->pipeline->uninstance(a);
			else
				a->pipeline->instance(a);
		}else{
			printf("default pipeline\n");
			if(uninstance)
				defpipe->uninstance(a);
			else
				defpipe->instance(a);
		}
	}

/*
	data = new rw::uint8[256*1024];
	rw::StreamMemory out;
	out.open(data, 0, 256*1024);
	c->streamWrite(&out);

	FILE *cf = fopen("out.dff", "wb");
	assert(cf != NULL);
	fwrite(data, out.getLength(), 1, cf);
	fclose(cf);
	out.close();
	delete[] data;
*/

	delete c;

	return 0;
}
