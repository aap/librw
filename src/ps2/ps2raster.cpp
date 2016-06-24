#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "rwps2.h"

#define PLUGIN_ID 0

#define max(a, b) ((a) > (b) ? (a) : (b))

namespace rw {
namespace ps2 {

int32 nativeRasterOffset;

#define MAXLEVEL(r) ((r)->tex1[1]>>18 & 0x3F)
#define SETMAXLEVEL(r, l) ((r)->tex1[1] = (r)->tex1[1]&~0xFF0000 | l<<18)
#define SETKL(r, val) ((r)->tex1[1] = (r)->tex1[1]&~0xFFFF | (uint16)(val))
static bool32 noNewStyleRasters;

// i don't really understand this, stolen from RW
static void
ps2MinSize(int32 psm, int32 flags, int32 *minw, int32 *minh)
{
	*minh = 1;
	switch(psm){
	case 0x00:
	case 0x30:
		*minw = 2; // 32 bit
		break;
	case 0x02:
	case 0x0A:
	case 0x32:
	case 0x3A:
		*minw = 4; // 16 bit
		break;
	case 0x01:
	case 0x13:
	case 0x14:
	case 0x1B:
	case 0x24:
	case 0x2C:
	case 0x31:
		*minw = 8; // everything else
		break;
	}
	if(flags & 0x2 && psm == 0x13){	// PSMT8
		*minw = 16;
		*minh = 4;
	}
	if(flags & 0x4 && psm == 0x14){	// PSMT4
		*minw = 32;
		*minh = 4;
	}
}

struct dword
{
	uint32 lo;
	uint32 hi;
};

#define ALIGN64(x) ((x) + 0x3F & ~0x3F)

static void
rasterCreate(Raster *raster)
{
	uint64 bufferWidth[7], bufferBase[7];
	int32 pageWidth, pageHeight;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);

	//printf("%x %x %x %x\n", raster->format, raster->flags, raster->type, noNewStyleRasters);
	assert(raster->type == 4);	// Texture
	switch(raster->depth){
	case 4:
		pageWidth = 128;
		pageHeight = 128;
		break;
	case 8:
		pageWidth = 128;
		pageHeight = 64;
		break;
	case 16:
		pageWidth = 64;
		pageHeight = 64;
		break;
	case 32:
		pageWidth = 64;
		pageHeight = 32;
		break;
	default:
		assert(0 && "unsupported depth");
	}
	int32 logw = 0, logh = 0;
	int32 s;
	for(s = 1; s < raster->width; s *= 2)
		logw++;
	for(s = 1; s < raster->height; s *= 2)
		logh++;
	SETKL(ras, 0xFC0);
	//printf("%d %d %d %d\n", raster->width, logw, raster->height, logh);
	ras->tex0[0] |= (raster->width < pageWidth ? pageWidth : raster->width)/64 << 14;
	ras->tex0[0] |= logw << 26;
	ras->tex0[0] |= logh << 30;
	ras->tex0[1] |= logh >> 2;

	int32 paletteWidth, paletteHeight, paletteDepth;
	int32 palettePagewidth, palettePageheight;
	if(raster->format & (Raster::PAL4 | Raster::PAL8))
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[1] |= 0xA << 19; // PSMCT16S
			paletteDepth = 2;
			palettePagewidth = palettePageheight = 64;
			break;
		case Raster::C8888:
			// PSMCT32
			paletteDepth = 4;
			palettePagewidth = 64;
			palettePageheight = 32;
			break;
		default:
			assert(0 && "unsupported palette format\n");
		}
	if(raster->format & Raster::PAL4){
		ras->tex0[0] |= 0x14 << 20;   // PSMT4
		ras->tex0[1] |= 1<<29 | 1<<2; // CLD 1, TCC RGBA
		paletteWidth = 8;
		paletteHeight = 2;
	}else if(raster->format & Raster::PAL8){
		ras->tex0[0] |= 0x13 << 20;   // PSMT8
		ras->tex0[1] |= 1<<29 | 1<<2; // CLD 1, TCC RGBA
		paletteWidth = paletteHeight = 16;
	}else{
		paletteWidth = 0;
		paletteHeight = 0;
		paletteDepth = 0;
		palettePagewidth = 0;
		palettePageheight = 0;
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[0] |= 0xA << 20; // PSMCT16S
			ras->tex0[1] |= 1 << 2;  // TCC RGBA
			break;
		case Raster::C8888:
			// PSMCT32
			ras->tex0[1] |= 1 << 2;  // TCC RGBA
			break;
		case Raster::C888:
			ras->tex0[0] |= 1 << 20; // PSMCT24
			break;
		default:
			assert(0 && "unsupported raster format\n");
		}
	}

	for(int i = 0; i < 7; i++){
		bufferWidth[i] = 1;
		bufferBase[i] = 0;
	}

	int32 mipw, miph;
	int32 n;
	int32 nPagW, nPagH;
	int32 w = raster->width;
	int32 h = raster->height;
	int32 d = raster->depth;
	raster->stride = w*d/8;

	if(raster->format & Raster::MIPMAP){
		static uint32 blockOffset32_24_8[8] = { 0, 2, 2, 8, 8, 10, 10, 32 };
		static uint32 blockOffset16_4[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
		static uint32 blockOffset16S[8] = { 0, 1, 8, 9, 4, 5, 12, 13 };
		uint64 lastBufferWidth;
		mipw = w;
		miph = h;
		lastBufferWidth = max(pageWidth, w)/64;
		ras->texelSize = 0;
		int32 gsoffset = 0;
		int32 gsaddress = 0;
		for(n = 0; n < 7; n++){
			if(w >= 8 && h >= 8 && (mipw < 8 || miph < 8))
				break;
			ras->texelSize += ALIGN64(mipw*miph*d/8);
			bufferWidth[n] = max(pageWidth, mipw)/64;

			if(bufferWidth[n] != lastBufferWidth){
				nPagW = ((w >> n-1) + pageWidth-1)/pageWidth;
				nPagH = ((h >> n-1) + pageHeight-1)/pageHeight;
				gsaddress = (gsoffset + nPagW*nPagH*0x800) & ~0x7FF;
			}
			lastBufferWidth = bufferWidth[n];
			gsaddress = ALIGN64(gsaddress);
			uint32 b = gsaddress/256 & 7;
			switch(ras->tex0[0]>>20 & 0x3F){
			case 0: case 1: case 0x13:
				b = blockOffset32_24_8[b];
				break;
			case 2: case 0x14:
				b = blockOffset16_4[b];
				break;
			case 0xA:
				b = blockOffset16S[b];
				break;
			default:
				// can't happen
				break;
			}
			bufferBase[n] = b + (gsaddress>>11 << 5);
			int32 stride = bufferWidth[n]/64*d/8;
			gsaddress = ALIGN64(miph*stride/4 + gsoffset);

			mipw /= 2;
			miph /= 2;
		}
		assert(0);
	}else{
		ras->texelSize = raster->stride*raster->height+0xF & ~0xF;
		ras->paletteSize = paletteWidth*paletteHeight*paletteDepth;
		ras->miptbp1[0] |= 1<<14;        // TBW1
		ras->miptbp1[1] |= 1<<2 | 1<<22; // TBW2,3
		ras->miptbp2[0] |= 1<<14;	 // TBW4
		ras->miptbp2[1] |= 1<<2 | 1<<22; // TBW5,6
		SETMAXLEVEL(ras, 0);
		nPagW = (raster->width + pageWidth-1)/pageWidth;
		nPagH = (raster->height + pageHeight-1)/pageHeight;
		bufferBase[0] = 0;
		bufferWidth[0] = nPagW * (pageWidth >> 6);
		ras->gsSize = (nPagW*nPagH*0x800)&~0x7FF;
		if(ras->paletteSize){
			// BITBLTBUF DBP
			if(pageWidth*nPagW > raster->width ||
			   pageHeight*nPagH > raster->height)
				ras->tex1[0] = (ras->gsSize >> 6) - 4;
			else
				ras->tex1[0] = ras->gsSize >> 6;
			nPagW = (paletteWidth + palettePagewidth-1)/palettePagewidth;
			nPagH = (paletteHeight + palettePageheight-1)/palettePageheight;
			ras->gsSize += (nPagW*nPagH*0x800)&~0x7FF;
		}else
			ras->tex1[0] = 0;
	}

	// allocate data and fill with GIF packets
	ras->texelSize = ras->texelSize+0xF & ~0xF;
	int32 numLevels = MAXLEVEL(ras)+1;
	if(noNewStyleRasters ||
	   (raster->width*raster->height*raster->depth & ~0x7F) >= 0x3FFF80){
		assert(0);
	}else{
		ras->flags |= 1;	// include GIF packets
		int32 psm = ras->tex0[0]>>20 & 0x3F;
		//int32 cpsm = ras->tex0[1]>>19 & 0x3F;
		if(psm == 0x13){	// PSMT8
			ras->flags |= 2;
			// TODO: stuff
		}
		if(psm == 0x14){	// PSMT4
			// swizzle flag probably depends on version :/
			if(rw::version > 0x31000)
				ras->flags |= 4;
			// TODO: stuff
		}
		ras->texelSize = 0x50*numLevels;	// GIF packets
		int32 minW, minH;
		ps2MinSize(psm, ras->flags, &minW, &minH);
		w = raster->width;
		h = raster->height;
		n = numLevels;
		while(n--){
			mipw = w < minW ? minW : w;
			miph = h < minH ? minH : h;
			ras->texelSize += mipw*miph*raster->depth/8+0xF & ~0xF;
			w /= 2;
			h /= 2;
		}
		if(ras->paletteSize){
			if(rw::version > 0x31000 && paletteHeight == 2)
				paletteHeight = 3;
			ras->paletteSize = 0x50 +
			    paletteDepth*paletteWidth*paletteHeight;
		}
		// TODO: allocate space for more DMA packets
		ras->dataSize = ras->paletteSize+ras->texelSize;
		uint8 *data = new uint8[ras->dataSize];
		assert(data);
		ras->data = data;
		raster->texels = data + 0x50;
		if(ras->paletteSize)
			raster->palette = data + ras->texelSize + 0x50;
		uint32 *p = (uint32*)data;
		w = raster->width;
		h = raster->height;
		for(n = 0; n < numLevels; n++){
			mipw = w < minW ? minW : w;
			miph = h < minH ? minH : h;

			// GIF tag
			*p++ = 3;          // NLOOP = 3
			*p++ = 0x10000000; // NREG = 1
			*p++ = 0xE;        // A+D
			*p++ = 0;

			// TRXPOS
			*p++ = 0;	// TODO
			*p++ = 0;	// TODO
			*p++ = 0x51;
			*p++ = 0;

			// TRXREG
			if(ras->flags & 2 && psm == 0x13 ||
			   ras->flags & 4 && psm == 0x14){
				*p++ = mipw/2;
				*p++ = miph/2;
			}else{
				*p++ = mipw;
				*p++ = miph;
			}
			*p++ = 0x52;
			*p++ = 0;

			// TRXDIR
			*p++ = 0;	// host -> local
			*p++ = 0;
			*p++ = 0x53;
			*p++ = 0;

			// GIF tag
			uint32 sz = mipw*miph*raster->depth/8 + 0xF >> 4;
			*p++ = sz;
			*p++ = 0x08000000; // IMAGE
			*p++ = 0;
			*p++ = 0;

			p += sz*4;
			w /= 2;
			h /= 2;
		}

		if(ras->paletteSize){
			p = (uint32*)(raster->palette - 0x50);
			// GIF tag
			*p++ = 3;          // NLOOP = 3
			*p++ = 0x10000000; // NREG = 1
			*p++ = 0xE;        // A+D
			*p++ = 0;

			// TRXPOS
			*p++ = 0;	// TODO
			*p++ = 0;	// TODO
			*p++ = 0x51;
			*p++ = 0;

			// TRXREG
			*p++ = paletteWidth;
			*p++ = paletteHeight;
			*p++ = 0x52;
			*p++ = 0;

			// TRXDIR
			*p++ = 0;	// host -> local
			*p++ = 0;
			*p++ = 0x53;
			*p++ = 0;

			// GIF tag
			uint32 sz = ras->paletteSize - 0x50 + 0xF >> 4;
			*p++ = sz;
			*p++ = 0x08000000; // IMAGE
			*p++ = 0;
			*p++ = 0;
		}
	}
}

static uint8*
rasterLock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
	return nil;
}

static void
rasterUnlock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
}

static int32
rasterNumLevels(Raster *raster)
{
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	if(raster->texels == nil) return 0;
	if(raster->format & Raster::MIPMAP)
		return MAXLEVEL(ras)+1;
	return 1;
}
	
static void*
createNativeRaster(void *object, int32 offset, int32)
{
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, object, offset);
	raster->tex0[0] = 0;
	raster->tex0[1] = 0;
	raster->tex1[0] = 0;
	raster->tex1[1] = 0;
	raster->miptbp1[0] = 0;
	raster->miptbp1[1] = 0;
	raster->miptbp2[0] = 0;
	raster->miptbp2[1] = 0;
	raster->texelSize = 0;
	raster->paletteSize = 0;
	raster->gsSize = 0;
	raster->flags = 0;
	SETKL(raster, 0xFC0);

	raster->dataSize = 0;
	raster->data = nil;
	return object;
}

static void*
destroyNativeRaster(void *object, int32 offset, int32)
{
	// TODO
	(void)offset;
	return object;
}

static void*
copyNativeRaster(void *dst, void *src, int32 offset, int32)
{
	Ps2Raster *dstraster = PLUGINOFFSET(Ps2Raster, dst, offset);
	Ps2Raster *srcraster = PLUGINOFFSET(Ps2Raster, src, offset);
	*dstraster = *srcraster;
	return dst;
}

static Stream*
readMipmap(Stream *stream, int32, void *object, int32 offset, int32)
{
	uint16 val = stream->readI32();
	Texture *tex = (Texture*)object;
	if(tex->raster == nil)
		return stream;
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, offset);
	SETKL(raster, val);
	return stream;
}

static Stream*
writeMipmap(Stream *stream, int32, void *object, int32 offset, int32)
{
	Texture *tex = (Texture*)object;
	if(tex->raster)
		return nil;
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, offset);
	stream->writeI32(raster->tex1[1]&0xFFFF);
	return stream;
}

static int32
getSizeMipmap(void*, int32, int32)
{
	return rw::platform == PLATFORM_PS2 ? 4 : 0;
}

void
registerNativeRaster(void)
{
	nativeRasterOffset = Raster::registerPlugin(sizeof(Ps2Raster),
	                                            0x12340000 | PLATFORM_PS2, 
                                                    createNativeRaster,
                                                    destroyNativeRaster,
                                                    copyNativeRaster);
	driver[PLATFORM_PS2].rasterNativeOffset = nativeRasterOffset;
	driver[PLATFORM_PS2].rasterCreate = rasterCreate;
	driver[PLATFORM_PS2].rasterLock = rasterLock;
	driver[PLATFORM_PS2].rasterUnlock = rasterUnlock;
	driver[PLATFORM_PS2].rasterNumLevels = rasterNumLevels;

	Texture::registerPlugin(0, ID_SKYMIPMAP, nil, nil, nil);
	Texture::registerPluginStream(ID_SKYMIPMAP, readMipmap, writeMipmap, getSizeMipmap);
}

struct StreamRasterExt
{
	int32 width;
	int32 height;
	int32 depth;
	uint16 rasterFormat;
	int16  type;
	uint32 tex0[2];
	uint32 tex1[2];
	uint32 miptbp1[2];
	uint32 miptbp2[2];
	uint32 texelSize;
	uint32 paletteSize;
	uint32 gsSize;
	uint32 mipmapVal;
};

Texture*
readNativeTexture(Stream *stream)
{
	uint32 length, oldversion, version;
	uint32 fourcc;
	Raster *raster;
	Ps2Raster *natras;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		return nil;
	}
	fourcc = stream->readU32();
	if(fourcc != FOURCC_PS2){
		RWERROR((ERR_PLATFORM, fourcc));
		return nil;
	}
	Texture *tex = Texture::create(nil);
	if(tex == nil)
		return nil;

	// Texture
	tex->filterAddressing = stream->readU32();
	if(!findChunk(stream, ID_STRING, &length, nil)){
		RWERROR((ERR_CHUNK, "STRING"));
		goto fail;
	}
	stream->read(tex->name, length);
	if(!findChunk(stream, ID_STRING, &length, nil)){
		RWERROR((ERR_CHUNK, "STRING"));
		goto fail;
	}
	stream->read(tex->mask, length);

	// Raster
	StreamRasterExt streamExt;
	oldversion = rw::version;
	if(!findChunk(stream, ID_STRUCT, nil, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		goto fail;
	}
	if(!findChunk(stream, ID_STRUCT, nil, &version)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		goto fail;
	}
	stream->read(&streamExt, 0x40);
	noNewStyleRasters = streamExt.type < 2;
	rw::version = version;
	raster = Raster::create(streamExt.width, streamExt.height,
	                        streamExt.depth, streamExt.rasterFormat,
	                        PLATFORM_PS2);
	noNewStyleRasters = 0;
	rw::version = oldversion;
	tex->raster = raster;
	natras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	//printf("%08X%08X %08X%08X %08X%08X %08X%08X\n",
	//       ras->tex0[1], ras->tex0[0], ras->tex1[1], ras->tex1[0],
	//       ras->miptbp1[0], ras->miptbp1[1], ras->miptbp2[0], ras->miptbp2[1]);
	natras->tex0[0] = streamExt.tex0[0];
	natras->tex0[1] = streamExt.tex0[1];
	natras->tex1[0] = streamExt.tex1[0];
	natras->tex1[1] = natras->tex1[1]&~0xFF0000 |
	                  streamExt.tex1[1]<<16 & 0xFF0000;
	natras->miptbp1[0] = streamExt.miptbp1[0];
	natras->miptbp1[1] = streamExt.miptbp1[1];
	natras->miptbp2[0] = streamExt.miptbp2[0];
	natras->miptbp2[1] = streamExt.miptbp2[1];
	natras->texelSize = streamExt.texelSize;
	natras->paletteSize = streamExt.paletteSize;
	natras->gsSize = streamExt.gsSize;
	SETKL(natras, streamExt.mipmapVal);
	//printf("%08X%08X %08X%08X %08X%08X %08X%08X\n",
	//       ras->tex0[1], ras->tex0[0], ras->tex1[1], ras->tex1[0],
	//       ras->miptbp1[0], ras->miptbp1[1], ras->miptbp2[0], ras->miptbp2[1]);

	if(!findChunk(stream, ID_STRING, &length, nil)){
		RWERROR((ERR_CHUNK, "STRING"));
		goto fail;
	}
	if(streamExt.type < 2){
		stream->read(raster->texels, length);
	}else{
		stream->read(raster->texels-0x50, natras->texelSize);
		stream->read(raster->palette-0x50, natras->paletteSize);
	}
	if(tex->streamReadPlugins(stream))
		return tex;

fail:
	tex->destroy();
	return nil;
}

void
writeNativeTexture(Texture *tex, Stream *stream)
{
	Raster *raster = tex->raster;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	int32 chunksize = getSizeNativeTexture(tex);
	writeChunkHeader(stream, ID_TEXTURENATIVE, chunksize);
	writeChunkHeader(stream, ID_STRUCT, 8);
	stream->writeU32(FOURCC_PS2);
	stream->writeU32(tex->filterAddressing);
	int32 len = strlen(tex->name)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, len);
	stream->write(tex->name, len);
	len = strlen(tex->mask)+4 & ~3;
	writeChunkHeader(stream, ID_STRING, len);
	stream->write(tex->mask, len);

	int32 sz = ras->texelSize + ras->paletteSize;
	writeChunkHeader(stream, ID_STRUCT, 12 + 64 + 12 + sz);
	writeChunkHeader(stream, ID_STRUCT, 64);
	StreamRasterExt streamExt;
	streamExt.width = raster->width;
	streamExt.height = raster->height;
	streamExt.depth = raster->depth;
	streamExt.rasterFormat = raster->format | raster->type;
	streamExt.type = 0;
	if(ras->flags == 2 && raster->depth == 8)
		streamExt.type = 1;
	if(ras->flags & 1)
		streamExt.type = 2;
	streamExt.tex0[0] = ras->tex0[0];
	streamExt.tex0[1] = ras->tex0[1];
	streamExt.tex1[0] = ras->tex1[0];
	streamExt.tex1[1] = ras->tex1[1]>>16 & 0xFF;
	streamExt.miptbp1[0] = ras->miptbp1[0];
	streamExt.miptbp1[1] = ras->miptbp1[1];
	streamExt.miptbp2[0] = ras->miptbp2[0];
	streamExt.miptbp2[1] = ras->miptbp2[1];
	streamExt.texelSize = ras->texelSize;
	streamExt.paletteSize = ras->paletteSize;
	streamExt.gsSize = ras->gsSize;
	streamExt.mipmapVal = ras->tex1[1]&0xFFFF;
	stream->write(&streamExt, 64);

	writeChunkHeader(stream, ID_STRUCT, sz);
	if(streamExt.type < 2){
		stream->write(raster->texels, sz);
	}else{
		stream->write(raster->texels-0x50, ras->texelSize);
		stream->write(raster->palette-0x50, ras->paletteSize);
	}
	tex->streamWritePlugins(stream);
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 8;
	size += 12 + strlen(tex->name)+4 & ~3;
	size += 12 + strlen(tex->mask)+4 & ~3;
	size += 12 + 12 + 64 + 12;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, tex->raster, nativeRasterOffset);
	size += ras->texelSize + ras->paletteSize;
	size += 12 + tex->streamGetPluginSize();
	return size;
}

}
}
