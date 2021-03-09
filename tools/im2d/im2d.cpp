#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

bool Im2DColored = true;
bool Im2DTextured;

rw::int32 Im2DPrimType;

rw::V2d ScreenSize;
float Scale;

rw::RGBA SolidWhite = {255, 255, 255, 255};
rw::RGBA SolidBlack = {0, 0, 0, 255};
rw::RGBA SolidRed = {200, 64, 64, 255};
rw::RGBA SolidGreen = {64, 200, 64, 255};
rw::RGBA SolidBlue = {64, 64, 200, 255};
rw::RGBA SolidYellow = {200, 200, 64, 255};
rw::RGBA SolidPurple = {200, 64, 200, 255};
rw::RGBA SolidCyan = {64, 200, 200, 255};

rw::Texture *Im2DTexture;

void
Im2DInitialize(rw::Camera *camera)
{
	ScreenSize.x = camera->frameBuffer->width;
	ScreenSize.y = camera->frameBuffer->height;

	Scale = ScreenSize.y / 3.0f;

	rw::Image::setSearchPath("files/");
	Im2DTexture = rw::Texture::read("whiteash", nil);

	LineListCreate(camera);
	LineListSetColor(!Im2DColored);

	IndexedLineListCreate(camera);
	IndexedLineListSetColor(!Im2DColored);

	PolyLineCreate(camera);
	PolyLineSetColor(!Im2DColored);

	IndexedPolyLineCreate(camera);
	IndexedPolyLineSetColor(!Im2DColored);

	TriListCreate(camera);
	TriListSetColor(!Im2DColored);

	IndexedTriListCreate(camera);
	IndexedTriListSetColor(!Im2DColored);

	TriStripCreate(camera);
	TriStripSetColor(!Im2DColored);

	IndexedTriStripCreate(camera);
	IndexedTriStripSetColor(!Im2DColored);

	TriFanCreate(camera);
	TriFanSetColor(!Im2DColored);

	IndexedTriFanCreate(camera);
	IndexedTriFanSetColor(!Im2DColored);
}

void
Im2DTerminate(void)
{
	if(Im2DTexture){
		Im2DTexture->destroy();
		Im2DTexture = nil;
	}
}

void
Im2DRender(void)
{
	rw::SetRenderState(rw::ZTESTENABLE, 0);
	rw::SetRenderState(rw::ZWRITEENABLE, 0);
	rw::SetRenderState(rw::SRCBLEND, rw::BLENDSRCALPHA);
	rw::SetRenderState(rw::DESTBLEND, rw::BLENDINVSRCALPHA);
	rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::FilterMode::LINEAR);

	if(Im2DTextured)
		rw::SetRenderStatePtr(rw::TEXTURERASTER, Im2DTexture->raster);
	else
		rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);

	switch(Im2DPrimType){
	case 0: LineListRender(); break;
	case 1: IndexedLineListRender(); break;
	case 2: PolyLineRender(); break;
	case 3: IndexedPolyLineRender(); break;
	case 4: TriListRender(); break;
	case 5: IndexedTriListRender(); break;
	case 6: TriStripRender(); break;
	case 7: IndexedTriStripRender(); break;
	case 8: TriFanRender(); break;
	case 9: IndexedTriFanRender(); break;
	}
}

void
Im2DSize(rw::Camera *camera, rw::int32 w, rw::int32 h)
{
	ScreenSize.x = w;
	ScreenSize.y = h;

	if(ScreenSize.x > ScreenSize.y)
		Scale = ScreenSize.y / 3.0f;
	else
		Scale = ScreenSize.x / 3.0f;

	LineListCreate(camera);

	IndexedLineListCreate(camera);

	PolyLineCreate(camera);

	IndexedPolyLineCreate(camera);

	TriListCreate(camera);

	IndexedTriListCreate(camera);

	TriStripCreate(camera);

	IndexedTriStripCreate(camera);

	TriFanCreate(camera);

	IndexedTriFanCreate(camera);
}
