#include <math.h>

#include "demoreel.h"

// Procedural textures so the demos need no asset files at all.

using namespace rw;

static Texture*
imageToTexture(Image *img)
{
	Raster *ras = Raster::createFromImage(img);
	img->destroy();
	if(ras == nil)
		return nil;
	Texture *tex = Texture::create(ras);
	tex->setFilter(Texture::LINEAR);
	tex->setAddressU(Texture::WRAP);
	tex->setAddressV(Texture::WRAP);
	return tex;
}

// soft radial blob for additive particles
Texture*
MakeGlowTexture(void)
{
	const int SZ = 64;
	Image *img = Image::create(SZ, SZ, 32);
	img->allocate();

	int x, y;
	for(y = 0; y < SZ; y++){
		uint8 *line = img->pixels + y*img->stride;
		for(x = 0; x < SZ; x++){
			float dx = (x + 0.5f - SZ/2) / (SZ/2);
			float dy = (y + 0.5f - SZ/2) / (SZ/2);
			float r = sqrtf(dx*dx + dy*dy);
			float v = 1.0f - r;
			if(v < 0.0f) v = 0.0f;
			v = v*v*(3.0f - 2.0f*v);	// smoothstep
			v = v*v;			// sharpen the core
			uint8 c = (uint8)(v*255.0f);
			line[x*4+0] = c;
			line[x*4+1] = c;
			line[x*4+2] = c;
			line[x*4+3] = c;
		}
	}
	return imageToTexture(img);
}

// glowing neon grid on dark ground
Texture*
MakeGridTexture(void)
{
	const int SZ = 128;
	const int CELL = 32;
	Image *img = Image::create(SZ, SZ, 32);
	img->allocate();

	int x, y;
	for(y = 0; y < SZ; y++){
		uint8 *line = img->pixels + y*img->stride;
		for(x = 0; x < SZ; x++){
			int mx = x % CELL; if(mx > CELL/2) mx = CELL-mx;
			int my = y % CELL; if(my > CELL/2) my = CELL-my;
			int d = mx < my ? mx : my;
			float v = 1.0f - d/6.0f;
			if(v < 0.0f) v = 0.0f;
			v = v*v;
			// dark blue ground, cyan-white lines
			float r = 0.02f + v*0.75f;
			float g = 0.03f + v*0.95f;
			float b = 0.10f + v*0.90f;
			line[x*4+0] = (uint8)(r*255.0f);
			line[x*4+1] = (uint8)(g*255.0f);
			line[x*4+2] = (uint8)(b*255.0f);
			line[x*4+3] = 255;
		}
	}
	return imageToTexture(img);
}

// fake sky-ground gradient with highlight streaks, for env mapping
Texture*
MakeEnvTexture(void)
{
	const int SZ = 128;
	Image *img = Image::create(SZ, SZ, 32);
	img->allocate();

	int x, y;
	for(y = 0; y < SZ; y++){
		uint8 *line = img->pixels + y*img->stride;
		float fy = (float)y/SZ;
		for(x = 0; x < SZ; x++){
			float fx = (float)x/SZ;
			float r, g, b;
			if(fy < 0.5f){
				// sky: bright at horizon
				float t = fy*2.0f;
				r = 0.05f + t*0.55f;
				g = 0.15f + t*0.65f;
				b = 0.35f + t*0.65f;
			}else{
				// ground: dark, fading down
				float t = (fy-0.5f)*2.0f;
				r = 0.45f - t*0.40f;
				g = 0.35f - t*0.32f;
				b = 0.30f - t*0.28f;
			}
			// horizon band
			float h = fabsf(fy - 0.5f);
			float band = 1.0f - h*8.0f;
			if(band > 0.0f){
				band = band*band;
				r += band*0.5f;
				g += band*0.5f;
				b += band*0.4f;
			}
			// vertical highlight streaks in the sky
			float streak = sinf(fx*3.14159f*6.0f);
			streak = streak*streak*streak*streak;
			if(fy < 0.5f)
				r += streak*0.15f, g += streak*0.15f, b += streak*0.1f;

			if(r > 1.0f) r = 1.0f;
			if(g > 1.0f) g = 1.0f;
			if(b > 1.0f) b = 1.0f;
			line[x*4+0] = (uint8)(r*255.0f);
			line[x*4+1] = (uint8)(g*255.0f);
			line[x*4+2] = (uint8)(b*255.0f);
			line[x*4+3] = 255;
		}
	}
	return imageToTexture(img);
}
