#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#include "lodepng/lodepng.h"

#ifdef _WIN32
/* srsly? */
#define strdup _strdup
#endif

#define PLUGIN_ID 0

namespace rw {


Image*
readPNG(const char *filename)
{
	Image *image;
	uint32 length;
	uint8 *data = getFileContents(filename, &length);
	assert(data != nil);

	lodepng::State state;
	std::vector<uint8> raw;
	uint32 w, h;

	// First try: decode without conversion to see if we understand the format
	state.decoder.color_convert = 0;
	uint32 error = lodepng::decode(raw, w, h, state, data, length);
	if(error){
		RWERROR((ERR_GENERAL, lodepng_error_text(error)));
		return nil;
	}

	if(state.info_raw.bitdepth == 4 && state.info_raw.colortype == LCT_PALETTE){
		image = Image::create(w, h, 4);
		image->allocate();
		memcpy(image->palette, state.info_raw.palette, state.info_raw.palettesize*4);
		expandPal4_BE(image->pixels, image->stride, &raw[0], w/2, w, h);
	}else if(state.info_raw.bitdepth == 8){
		switch(state.info_raw.colortype){
		case LCT_PALETTE:
			image = Image::create(w, h, state.info_raw.palettesize <= 16 ? 4 : 8);
			image->allocate();
			memcpy(image->palette, state.info_raw.palette, state.info_raw.palettesize*4);
			memcpy(image->pixels, &raw[0], w*h);
			break;
		case LCT_RGB:
			image = Image::create(w, h, 24);
			image->allocate();
			memcpy(image->pixels, &raw[0], w*h*3);
			break;
		default:
			// Second try: just load as 32 bit
			lodepng_state_init(&state);
			error = lodepng::decode(raw, w, h, state, data, length);
			if(error){
				RWERROR((ERR_GENERAL, lodepng_error_text(error)));
				return nil;
			}
			// fall through
		case LCT_RGBA:
			image = Image::create(w, h, 32);
			image->allocate();
			memcpy(image->pixels, &raw[0], w*h*4);
			break;
		}
	}

	return image;
}

void
writePNG(Image *image, const char *filename)
{
	int32 i;
	StreamFile file;
	if(!file.open(filename, "wb")){
		RWERROR((ERR_FILE, filename));
		return;
	}

	std::vector<uint8> raw;
	uint8 *pixels;
	lodepng::State state;

	pixels = image->pixels;
	switch(image->depth){
	case 4:
		state.info_raw.bitdepth = 4;
		state.info_raw.colortype = LCT_PALETTE;
		state.info_png.color.bitdepth = 4;
		state.info_png.color.colortype = LCT_PALETTE;
		state.encoder.auto_convert = 0;
		for(i = 0; i < (1<<image->depth); i++){
			uint8 *col = &image->palette[i*4];
			lodepng_palette_add(&state.info_png.color, col[0], col[1], col[2], col[3]);
			lodepng_palette_add(&state.info_raw, col[0], col[1], col[2], col[3]);
		}
		pixels = rwNewT(uint8, image->width/2*image->height, ID_IMAGE | MEMDUR_FUNCTION);
		compressPal4_BE(pixels, image->width/2, image->pixels, image->width, image->width, image->height);
		break;
	case 8:
		state.info_raw.colortype = LCT_PALETTE;
		state.info_png.color.colortype = LCT_PALETTE;
		state.encoder.auto_convert = 0;
		for(i = 0; i < (1<<image->depth); i++){
			uint8 *col = &image->palette[i*4];
			lodepng_palette_add(&state.info_png.color, col[0], col[1], col[2], col[3]);
			lodepng_palette_add(&state.info_raw, col[0], col[1], col[2], col[3]);
		}
		break;
	case 16:
		// Don't think we can have 16 bits with PNG
		// TODO: don't change original image
		image->convertTo32();
		break;
	case 24:
		state.info_raw.colortype = LCT_RGB;
		state.info_png.color.colortype = LCT_RGB;
		state.encoder.auto_convert = 0;
		break;
	case 32:
		// already done
		break;
	}

	uint32 error = lodepng::encode(raw, pixels, image->width, image->height, state);
	if(error){
		RWERROR((ERR_GENERAL, lodepng_error_text(error)));
		return;
	}
	if(pixels != image->pixels)
		rwFree(pixels);
	file.write8(&raw[0], raw.size());
	file.close();
}

}
