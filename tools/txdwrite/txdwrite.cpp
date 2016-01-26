#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>

#include <rw.h>
#include <args.h>
#include <src/gtaplg.h>

char *argv0;

using namespace std;
using namespace rw;

struct {
	char *str;
	uint32 val;
} platforms[] = {
	{ "ps2",    PLATFORM_PS2 },
	{ "xbox",   PLATFORM_XBOX },
	{ "d3d8",   PLATFORM_D3D8 },
	{ "d3d9",   PLATFORM_D3D9 },
	{ NULL, 0 }
};

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
		newras = Raster::create(raster->width, raster->height, raster->depth,
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
		newras = Raster::create(raster->width, raster->height, raster->depth,
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

void
usage(void)
{
	fprintf(stderr, "usage: %s [-v version] [-o platform] in.txd [out.txd]\n", argv0);
	fprintf(stderr, "\t-v RW version, e.g. 33004 for 3.3.0.4\n");
	fprintf(stderr, "\t-o output platform. ps2, xbox, mobile, d3d8, d3d9\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	gta::attachPlugins();

	rw::version = 0;
	rw::platform = rw::PLATFORM_PS2;
//	rw::platform = rw::PLATFORM_OGL;
//	rw::platform = rw::PLATFORM_XBOX;
//	rw::platform = rw::PLATFORM_D3D8;
//	rw::platform = rw::PLATFORM_D3D9;
	int outplatform = rw::PLATFORM_XBOX;

	char *s;
	ARGBEGIN{
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

	if(argc < 1)
		usage();

	rw::StreamFile in;
	if(in.open(argv[0], "rb") == NULL){
		printf("couldn't open file %s\n", argv[1]);
		return 1;
	}
	ChunkHeaderInfo header;
	readChunkHeaderInfo(&in, &header);
	assert(header.type == ID_TEXDICTIONARY);
	rw::TexDictionary *txd;
	txd = rw::TexDictionary::streamRead(&in);
	assert(txd);
	in.close();
	rw::currentTexDictionary = txd;

	if(rw::version == 0){
		rw::version = header.version;
		rw::build = header.build;
	}

	if(outplatform == PLATFORM_D3D8)
		FORLIST(lnk, txd->textures){
			Texture *tex = Texture::fromDict(lnk);
			tex->raster = xboxToD3d8(tex->raster);
		}
//	for(Texture *tex = txd->first; tex; tex = tex->next)
//		tex->filterAddressing = (tex->filterAddressing&~0xF) | 0x2;
	rw::platform = outplatform;

	rw::StreamFile out;
	if(argc > 1)
		out.open(argv[1], "wb");
	else
		out.open("out.txd", "wb");
	txd->streamWrite(&out);
	out.close();

	return 0;
}
