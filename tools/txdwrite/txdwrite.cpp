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
	gta::attachPlugins();
	rw::ps2::registerNativeRaster();
	rw::xbox::registerNativeRaster();
	rw::d3d::registerNativeRaster();

//	rw::version = 0x33002;
//	rw::platform = rw::PLATFORM_PS2;
//	rw::platform = rw::PLATFORM_OGL;
//	rw::platform = rw::PLATFORM_XBOX;
//	rw::platform = rw::PLATFORM_D3D8;
//	rw::platform = rw::PLATFORM_D3D9;

	if(argc < 2){
		printf("usage (%d): %s in.txd\n", rw::platform, argv[0]);
		return 0;
	}

	rw::StreamFile in;
	if(in.open(argv[1], "rb") == NULL){
		printf("couldn't open file\n");
		return 1;
	}
	rw::findChunk(&in, rw::ID_TEXDICTIONARY, NULL, NULL);
	rw::TexDictionary *txd;
	txd = rw::TexDictionary::streamRead(&in);
	assert(txd);
	in.close();
	rw::currentTexDictionary = txd;

	rw::StreamFile out;
	out.open("out.txd", "wb");
	txd->streamWrite(&out);
	out.close();

	return 0;
}
