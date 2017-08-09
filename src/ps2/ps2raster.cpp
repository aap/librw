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

#define MAXLEVEL(r) ((r)->tex1low >> 2)
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

#define ALIGN16(x) ((x) + 0xF & ~0xF)
#define ALIGN64(x) ((x) + 0x3F & ~0x3F)

enum Psm {
	PSMCT32  = 0x0,
	PSMCT24  = 0x1,
	PSMCT16  = 0x2,
	PSMCT16S = 0xA,
	PSMT8    = 0x13,
	PSMT4    = 0x14,
	PSMT8H   = 0x1B,
	PSMT4HL  = 0x24,
	PSMT4HH  = 0x2C,
};

void*
mallocalign(size_t size, int32 alignment)
{
	void *p;
	void **pp;
	p = malloc(size + alignment + sizeof(void*));
	if(p == nil) return nil;
	pp = (void**)(((uintptr)p + sizeof(void*) + alignment)&~(alignment-1));
	pp[-1] = p;
	return (void*)pp;
}

void
freealign(void *p)
{
	void *pp;
	if(p == nil) return;
	pp = ((void**)p)[-1];
	free(pp);
}

void
rasterCreate(Raster *raster)
{
	enum {
		TCC_RGBA = 1 << 2,
		CLD_1 = 1 << 29,
	};
	uint64 bufferWidth[7], bufferBase[7];
	int32 pageWidth, pageHeight;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);

	if(raster->flags & Raster::DONTALLOCATE)
		return;

	//printf("%x %x %x %x\n", raster->format, raster->flags, raster->type, noNewStyleRasters);
	assert(raster->type == Raster::TEXTURE);
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
	ras->kl = 0xFC0;
	//printf("%d %d %d %d\n", raster->width, logw, raster->height, logh);

	// round up to page width, set TBW, TW, TH
	ras->tex0[0] |= (raster->width < pageWidth ? pageWidth : raster->width)/64 << 14;
	ras->tex0[0] |= logw << 26;
	ras->tex0[0] |= logh << 30;
	ras->tex0[1] |= logh >> 2;

	// set PSM, TCC, CLD, CPSM and figure out palette format
	int32 paletteWidth, paletteHeight, paletteDepth;
	int32 palettePagewidth, palettePageheight;
	if(raster->format & (Raster::PAL4 | Raster::PAL8))
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[1] |= PSMCT16S << 19;
			paletteDepth = 2;
			palettePagewidth = palettePageheight = 64;
			break;
		case Raster::C8888:
			ras->tex0[1] |= PSMCT32 << 19;
			paletteDepth = 4;
			palettePagewidth = 64;
			palettePageheight = 32;
			break;
		default:
			assert(0 && "unsupported palette format\n");
		}
	if(raster->format & Raster::PAL4){
		ras->tex0[0] |= PSMT4 << 20;
		ras->tex0[1] |= CLD_1 | TCC_RGBA;
		paletteWidth = 8;
		paletteHeight = 2;
	}else if(raster->format & Raster::PAL8){
		ras->tex0[0] |= PSMT8 << 20;
		ras->tex0[1] |= CLD_1 | TCC_RGBA;
		paletteWidth = paletteHeight = 16;
	}else{
		paletteWidth = 0;
		paletteHeight = 0;
		paletteDepth = 0;
		palettePagewidth = 0;
		palettePageheight = 0;
		switch(raster->format & 0xF00){
		case Raster::C1555:
			ras->tex0[0] |= PSMCT16S << 20;
			ras->tex0[1] |= TCC_RGBA;
			break;
		case Raster::C8888:
			ras->tex0[0] |= PSMCT32 << 20;
			ras->tex0[1] |= TCC_RGBA;
			break;
		case Raster::C888:
			ras->tex0[0] |= PSMCT24 << 20;
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
		// No mipmaps

		ras->texelSize = ALIGN16(raster->stride*raster->height);
		ras->paletteSize = paletteWidth*paletteHeight*paletteDepth;
		ras->miptbp1[0] |= 1<<14;        // TBW1
		ras->miptbp1[1] |= 1<<2 | 1<<22; // TBW2,3
		ras->miptbp2[0] |= 1<<14;	 // TBW4
		ras->miptbp2[1] |= 1<<2 | 1<<22; // TBW5,6
		ras->tex1low = 0;	// one mipmap level

		// find out number of pages needed
		nPagW = (raster->width + pageWidth-1)/pageWidth;
		nPagH = (raster->height + pageHeight-1)/pageHeight;

		// calculate buffer width in units of pixels/64
		bufferBase[0] = 0;
		bufferWidth[0] = nPagW*pageWidth / 64;

		// calculate whole buffer size in words
		ras->gsSize = nPagW*nPagH*0x800;

		// calculate palette offset on GS in units of words/64
		if(ras->paletteSize){
			// Maximum palette size will be 256 words.
			// If there is still room, use it!
			// TODO: find out why this check works
			if(pageWidth*nPagW > raster->width ||
			   pageHeight*nPagH > raster->height)
				ras->paletteOffset = (ras->gsSize - 256)/ 64;
			else{
				// Otherwise allocate more space...
				ras->paletteOffset = ras->gsSize / 64;
				// ...using the same calculation as above
				nPagW = (paletteWidth + palettePagewidth-1)/palettePagewidth;
				nPagH = (paletteHeight + palettePageheight-1)/palettePageheight;
				ras->gsSize += nPagW*nPagH*0x800;
			}
		}else
			ras->paletteOffset = 0;
	}

	// allocate data and fill with GIF packets
	ras->texelSize = ALIGN16(ras->texelSize);
	int32 numLevels = MAXLEVEL(ras)+1;
	// No GIF packet because we either don't want it (pre 0x310 rasters)
	// or the data wouldn't fit into a DMA packet
	if(noNewStyleRasters ||
	   (raster->width*raster->height*raster->depth/8/0x10) >= 0x7FFF){
		ras->dataSize = ras->paletteSize+ras->texelSize;
		uint8 *data = (uint8*)mallocalign(ras->dataSize, 0x40);
		ras->data = data;
		raster->texels = data;
		if(ras->paletteSize)
			raster->palette = data + ras->texelSize;
		if(raster->depth == 8)
			ras->flags |= Ps2Raster::SWIZZLED8;
	}else{
		ras->flags |= Ps2Raster::HASGIFPACKETS;
		int32 psm = ras->tex0[0]>>20 & 0x3F;
		//int32 cpsm = ras->tex0[1]>>19 & 0x3F;
		if(psm == PSMT8){
			ras->flags |= Ps2Raster::SWIZZLED8;
			// TODO: crazy stuff
		}
		if(psm == PSMT4){
			// swizzle flag probably depends on version :/
			// but which version? ....
			if(rw::version > 0x31000)
				ras->flags |= Ps2Raster::SWIZZLED4;
			// TODO: crazy stuff
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
			ras->texelSize += ALIGN16(mipw*miph*raster->depth/8);
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
		// every upload as 4 qwords:
		// DMAcnt(2) [NOP, DIRECT]
		//   GIF tag  A+D
		//     BITBLTBUF
		// DMAref(pixel data) [NOP, DIRECT]
		ras->dataSize = ras->paletteSize+ras->texelSize;
		uint8 *data = (uint8*)mallocalign(ras->dataSize, 0x40);
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
			if(ras->flags & Ps2Raster::SWIZZLED8 && psm == PSMT8 ||
			   ras->flags & Ps2Raster::SWIZZLED4 && psm == PSMT4){
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

uint8*
rasterLock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
	return nil;
}

void
rasterUnlock(Raster *raster, int32 level)
{
	// TODO
	(void)raster;
	(void)level;
}

int32
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
	raster->paletteOffset = 0;
	raster->kl = 0xFC0;
	raster->tex1low = 0;
	raster->unk2 = 0;
	raster->miptbp1[0] = 0;
	raster->miptbp1[1] = 0;
	raster->miptbp2[0] = 0;
	raster->miptbp2[1] = 0;
	raster->texelSize = 0;
	raster->paletteSize = 0;
	raster->gsSize = 0;
	raster->flags = 0;

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
	raster->kl = val;
	return stream;
}

static Stream*
writeMipmap(Stream *stream, int32, void *object, int32 offset, int32)
{
	Texture *tex = (Texture*)object;
	if(tex->raster)
		return nil;
	Ps2Raster *raster = PLUGINOFFSET(Ps2Raster, tex->raster, offset);
	stream->writeI32(raster->kl);
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
	                                            ID_RASTERPS2,
	                                            createNativeRaster,
	                                            destroyNativeRaster,
	                                            copyNativeRaster);

	Texture::registerPlugin(0, ID_SKYMIPMAP, nil, nil, nil);
	Texture::registerPluginStream(ID_SKYMIPMAP, readMipmap, writeMipmap, getSizeMipmap);
}

void
printTEX0(uint64 tex0)
{
	printf("%016llX ", tex0);
	uint32 tbp0 = tex0 & 0x3FFF; tex0 >>= 14;
	uint32 tbw = tex0 & 0x3F; tex0 >>= 6;
	uint32 psm = tex0 & 0x3F; tex0 >>= 6;
	uint32 tw = tex0 & 0xF; tex0 >>= 4;
	uint32 th = tex0 & 0xF; tex0 >>= 4;
	uint32 tcc = tex0 & 0x1; tex0 >>= 1;
	uint32 tfx = tex0 & 0x3; tex0 >>= 2;
	uint32 cbp = tex0 & 0x3FFF; tex0 >>= 14;
	uint32 cpsm = tex0 & 0xF; tex0 >>= 4;
	uint32 csm = tex0 & 0x1; tex0 >>= 1;
	uint32 csa = tex0 & 0x1F; tex0 >>= 5;
	uint32 cld = tex0 & 0x7;
	printf("TBP0:%4X TBW:%2X PSM:%2X TW:%X TH:%X TCC:%X TFX:%X CBP:%4X CPSM:%X CSM:%X CSA:%2X CLD:%X\n",
		tbp0, tbw, psm, tw, th, tcc, tfx, cbp, cpsm, csm, csa, cld);
}

void
printTEX1(uint64 tex1)
{
	printf("%016llX ", tex1);
	uint32 lcm = tex1 & 0x1; tex1 >>= 2;
	uint32 mxl = tex1 & 0x7; tex1 >>= 3;
	uint32 mmag = tex1 & 0x1; tex1 >>= 1;
	uint32 mmin = tex1 & 0x7; tex1 >>= 3;
	uint32 mtba = tex1 & 0x1; tex1 >>= 10;
	uint32 l = tex1 & 0x3; tex1 >>= 13;
	uint32 k = tex1 & 0xFFF;
	printf("LCM:%X MXL:%X MMAG:%X MMIN:%X MTBA:%X L:%X K:%X\n",
		lcm, mxl, mmag, mmin, mtba, l, k);
}

void
calcTEX1(Raster *raster, uint64 *tex1, int32 filter)
{
	enum {
		NEAREST = 0,
		LINEAR,
		NEAREST_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_NEAREST,
		LINEAR_MIPMAP_LINEAR,
	};
	Ps2Raster *natras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
	uint64 t1 = natras->tex1low;
	uint64 k = natras->kl & 0xFFF;
	uint64 l = (natras->kl >> 12) & 0x3;
	t1 |= k << 32;
	t1 |= l << 19;
	switch(filter){
	case Texture::NEAREST:
		t1 |= (NEAREST << 5) |
		      (NEAREST << 6);
		break;
	case Texture::LINEAR:
		t1 |= (LINEAR << 5) |
		      (LINEAR << 6);
		break;
	case Texture::MIPNEAREST:
		t1 |= (NEAREST << 5) |
		      (NEAREST_MIPMAP_NEAREST << 6);
		break;
	case Texture::MIPLINEAR:
		t1 |= (LINEAR << 5) |
		      (LINEAR_MIPMAP_NEAREST << 6);
		break;
	case Texture::LINEARMIPNEAREST:
		t1 |= (NEAREST << 5) |
		      (NEAREST_MIPMAP_LINEAR << 6);
		break;
	case Texture::LINEARMIPLINEAR:
		t1 |= (LINEAR << 5) |
		      (LINEAR_MIPMAP_LINEAR << 6);
		break;
	}
	*tex1 = t1;
}

struct StreamRasterExt
{
	int32 width;
	int32 height;
	int32 depth;
	uint16 rasterFormat;
	int16  type;
	uint32 tex0[2];
	uint32 paletteOffset;
	uint32 tex1low;
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
printf("%X %X %X %X %X %08X%08X %X %X %08X%08X %08X%08X %X %X %X %X\n",
streamExt.width,
streamExt.height,
streamExt.depth,
streamExt.rasterFormat,
streamExt.type,
streamExt.tex0[1],
streamExt.tex0[0],
streamExt.paletteOffset,
streamExt.tex1low,
streamExt.miptbp1[1],
streamExt.miptbp1[0],
streamExt.miptbp2[1],
streamExt.miptbp2[0],
streamExt.texelSize,
streamExt.paletteSize,
streamExt.gsSize,
streamExt.mipmapVal);

	noNewStyleRasters = streamExt.type < 2;
	rw::version = version;
	raster = Raster::create(streamExt.width, streamExt.height,
	                        streamExt.depth, streamExt.rasterFormat,
	                        PLATFORM_PS2);
	noNewStyleRasters = 0;
	rw::version = oldversion;
	tex->raster = raster;
	natras = PLUGINOFFSET(Ps2Raster, raster, nativeRasterOffset);
//printf("%X %X\n", natras->paletteOffset, natras->tex1low);
//	printf("%08X%08X %08X%08X %08X%08X\n",
//	       natras->tex0[1], natras->tex0[0],
//	       natras->miptbp1[0], natras->miptbp1[1], natras->miptbp2[0], natras->miptbp2[1]);
//	printTEX0(((uint64)natras->tex0[1] << 32) | natras->tex0[0]);
	uint64 tex1;
	calcTEX1(raster, &tex1, tex->filterAddressing & 0xF);
//	printTEX1(tex1);

	natras->tex0[0] = streamExt.tex0[0];
	natras->tex0[1] = streamExt.tex0[1];
	natras->paletteOffset = streamExt.paletteOffset;
	natras->tex1low = streamExt.tex1low;
	natras->miptbp1[0] = streamExt.miptbp1[0];
	natras->miptbp1[1] = streamExt.miptbp1[1];
	natras->miptbp2[0] = streamExt.miptbp2[0];
	natras->miptbp2[1] = streamExt.miptbp2[1];
	natras->texelSize = streamExt.texelSize;
	natras->paletteSize = streamExt.paletteSize;
	natras->gsSize = streamExt.gsSize;
	natras->kl = streamExt.mipmapVal;
//printf("%X %X\n", natras->paletteOffset, natras->tex1low);
//	printf("%08X%08X %08X%08X %08X%08X\n",
//	       natras->tex0[1], natras->tex0[0],
//	       natras->miptbp1[0], natras->miptbp1[1], natras->miptbp2[0], natras->miptbp2[1]);
//	printTEX0(((uint64)natras->tex0[1] << 32) | natras->tex0[0]);
	calcTEX1(raster, &tex1, tex->filterAddressing & 0xF);
//	printTEX1(tex1);

	if(!findChunk(stream, ID_STRUCT, &length, nil)){
		RWERROR((ERR_CHUNK, "STRUCT"));
		goto fail;
	}
	if(streamExt.type < 2){
		stream->read(raster->texels, length);
	}else{
		stream->read(raster->texels-0x50, natras->texelSize);
		stream->read(raster->palette-0x50, natras->paletteSize);
	}
//printf("\n");
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
	if(ras->flags == Ps2Raster::SWIZZLED8 && raster->depth == 8)
		streamExt.type = 1;
	if(ras->flags & Ps2Raster::HASGIFPACKETS)
		streamExt.type = 2;
	streamExt.tex0[0] = ras->tex0[0];
	streamExt.tex0[1] = ras->tex0[1];
	streamExt.paletteOffset = ras->paletteOffset;
	streamExt.tex1low = ras->tex1low;
	streamExt.miptbp1[0] = ras->miptbp1[0];
	streamExt.miptbp1[1] = ras->miptbp1[1];
	streamExt.miptbp2[0] = ras->miptbp2[0];
	streamExt.miptbp2[1] = ras->miptbp2[1];
	streamExt.texelSize = ras->texelSize;
	streamExt.paletteSize = ras->paletteSize;
	streamExt.gsSize = ras->gsSize;
	streamExt.mipmapVal = ras->kl;
	stream->write(&streamExt, 64);

	writeChunkHeader(stream, ID_STRUCT, sz);
	if(streamExt.type < 2){
		stream->write(raster->texels, sz);
	}else{
		stream->write(raster->texels-0x50, ras->texelSize);
		stream->write(raster->palette-0x50, ras->paletteSize);
	}
}

uint32
getSizeNativeTexture(Texture *tex)
{
	uint32 size = 12 + 8;
	size += 12 + strlen(tex->name)+4 & ~3;
	size += 12 + strlen(tex->mask)+4 & ~3;
	size += 12;
	size += 12 + 64;
	Ps2Raster *ras = PLUGINOFFSET(Ps2Raster, tex->raster, nativeRasterOffset);
	size += 12 + ras->texelSize + ras->paletteSize;
	return size;
}

}
}
