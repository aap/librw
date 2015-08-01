#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include "rw.h"
#include "src/gtaplg.h"

using namespace std;

int
main(int argc, char *argv[])
{
//	rw::version = 0x31000;
//	rw::build = 0;

//	rw::version = 0x33002;

	gta::registerEnvSpecPlugin();
	rw::registerMatFXPlugin();
	rw::registerMaterialRightsPlugin();
	rw::registerAtomicRightsPlugin();
	rw::registerHAnimPlugin();
	gta::registerNodeNamePlugin();
	gta::registerBreakableModelPlugin();
	gta::registerExtraVertColorPlugin();
	rw::ps2::registerADCPlugin();
	rw::ps2::registerPDSPlugin();
	rw::registerSkinPlugin();
	rw::registerNativeDataPlugin();
//	rw::ps2::registerNativeDataPlugin();
	rw::registerMeshPlugin();

	rw::Clump *c;

//	rw::StreamFile in;
//	in.open(argv[1], "rb");

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

	rw::Image::setSearchPath("./;/home/aap/gamedata/ps2/gtavc/MODELS/gta3_archive/txd_extracted/");

/*
//	rw::Image *tga = rw::readTGA("b.tga");
	rw::Image *tga = rw::readTGA("player.tga");
	assert(tga != NULL);
	rw::writeTGA(tga, "out.tga");
*/

//	for(rw::int32 i = 0; i < c->numAtomics; i++)
//		rw::Gl::Instance(c->atomicList[i]);

//	ofstream out(argv[2], ios::binary);
//	rw::StreamFile out;
//	out.open(argv[2], "wb");
	data = new rw::uint8[256*1024];
	rw::StreamMemory out;
	out.open(data, 0, 256*1024);
	c->streamWrite(&out);

	cf = fopen(argv[2], "wb");
	assert(cf != NULL);
	fwrite(data, out.getLength(), 1, cf);
	fclose(cf);
	out.close();
	delete[] data;

	delete c;

	return 0;

}
