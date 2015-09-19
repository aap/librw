#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <rw.h>
#include <src/gtaplg.h>

using namespace std;
using namespace rw;

Raster*
xboxToD3d8(Raster *raster)
{
	using namespace xbox;

	Raster *newras;
	if(raster->platform != PLATFORM_XBOX)
		return raster;
	XboxRaster *ras = PLUGINOFFSET(XboxRaster, raster, nativeRasterOffset);

	int32 numLevels = raster->getNumLevels();

	int32 format = raster->format;
//	format &= ~Raster::MIPMAP;
	if(ras->format){
		newras = new Raster(raster->width, raster->height, raster->depth,
		                    format | raster->type | 0x80, PLATFORM_D3D8);
		int32 dxt = 0;
		switch(ras->format){
		case D3DFMT_DXT1:
			dxt = 1;
			break;
		case D3DFMT_DXT3:
			dxt = 3;
			break;
		case D3DFMT_DXT5:
			dxt = 5;
			break;
		}
		d3d::allocateDXT(newras, dxt, numLevels, ras->hasAlpha);
	}else{
		printf("swizzled!\n");
		newras = new Raster(raster->width, raster->height, raster->depth,
		                    format | raster->type, PLATFORM_D3D8);
	}

	if(raster->format & Raster::PAL4)
		d3d::setPalette(newras, ras->palette, 32);
	else if(raster->format & Raster::PAL8)
		d3d::setPalette(newras, ras->palette, 256);

	uint8 *data;
	for(int32 i = 0; i < numLevels; i++){
		if(i >= newras->getNumLevels())
			break;
		data = raster->lock(i);
		d3d::setTexels(newras, data, i);
		raster->unlock(i);
	}

	delete raster;
	return newras;
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();

//	rw::version = 0x33002;
	rw::version = 0x32000;
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
		printf("couldn't open file %s\n", argv[1]);
		return 1;
	}
	rw::findChunk(&in, rw::ID_TEXDICTIONARY, NULL, NULL);
	rw::TexDictionary *txd;
	txd = rw::TexDictionary::streamRead(&in);
	assert(txd);
	in.close();
	rw::currentTexDictionary = txd;

//	for(Texture *tex = txd->first; tex; tex = tex->next)
//		tex->raster = xboxToD3d8(tex->raster);
	for(Texture *tex = txd->first; tex; tex = tex->next)
		tex->filterAddressing = (tex->filterAddressing&~0xF) | 0x2;

	rw::StreamFile out;
	if(argc > 2)
		out.open(argv[2], "wb");
	else
		out.open("out.txd", "wb");
	txd->streamWrite(&out);
	out.close();

	return 0;
}
