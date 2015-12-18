#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <rw.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;

int
main(int argc, char *argv[])
{
//	rw::version = 0x31000;
//	rw::build = 0;

//	rw::version = 0x32000;
	rw::version = 0x33002;
	rw::platform = PLATFORM_D3D8;
//	rw::version = 0x30200;

	int lcs = 1;
	matFXGlobals.hack = lcs;

	gta::attachPlugins();

	rw::Clump *c;

//	rw::StreamFile in;
//	in.open(argv[1], "rb");

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
		ChunkHeaderInfo header;
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

	if(lcs)
		for(int32 i = 0; i < c->numAtomics; i++)
			convertRslGeometry(c->atomicList[i]->geometry);

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
