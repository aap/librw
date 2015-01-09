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
//	Rw::Version = 0x31000;
//	Rw::Build = 0;

//	Rw::Version = 0x33002;

	Rw::RegisterMaterialRightsPlugin();
	Rw::RegisterMatFXPlugin();
	Rw::RegisterAtomicRightsPlugin();
	Rw::RegisterNodeNamePlugin();
	Rw::RegisterBreakableModelPlugin();
	Rw::RegisterExtraVertColorPlugin();
	Rw::Ps2::RegisterADCPlugin();
	Rw::RegisterSkinPlugin();
	Rw::RegisterNativeDataPlugin();
//	Rw::Ps2::RegisterNativeDataPlugin();
	Rw::RegisterMeshPlugin();

	Rw::Clump *c;

//	ifstream in(argv[1], ios::binary);

//	Rw::StreamFile in;
//	in.open(argv[1], "rb");

	FILE *cf = fopen(argv[1], "rb");
	assert(cf != NULL);
	fseek(cf, 0, SEEK_END);
	Rw::uint32 len = ftell(cf);
	fseek(cf, 0, SEEK_SET);
	Rw::uint8 *data = new Rw::uint8[len];
	fread(data, len, 1, cf);
	fclose(cf);
	Rw::StreamMemory in;
	in.open(data, len);

	Rw::FindChunk(&in, Rw::ID_CLUMP, NULL, NULL);
	Rw::DebugFile = argv[1];
	c = Rw::Clump::streamRead(&in);
	assert(c != NULL);

	in.close();
	delete[] data;

//	Rw::Image *tga = Rw::readTGA("b.tga");
//	assert(tga != NULL);
//	Rw::writeTGA(tga, "out.tga");

//	for(Rw::int32 i = 0; i < c->numAtomics; i++)
//		Rw::Gl::Instance(c->atomicList[i]);

//	ofstream out(argv[2], ios::binary);
//	Rw::StreamFile out;
//	out.open(argv[2], "wb");
	data = new Rw::uint8[256*1024];
	Rw::StreamMemory out;
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
