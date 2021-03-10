#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

bool Im3DColored = true;
bool Im3DTextured;

rw::int32 Im3DPrimType;

rw::RGBA SolidWhite = {255, 255, 255, 255};
rw::RGBA SolidBlack = {0, 0, 0, 255};
rw::RGBA SolidRed = {200, 64, 64, 255};
rw::RGBA SolidGreen = {64, 200, 64, 255};
rw::RGBA SolidBlue = {64, 64, 200, 255};
rw::RGBA SolidYellow = {200, 200, 64, 255};
rw::RGBA SolidPurple = {200, 64, 200, 255};
rw::RGBA SolidCyan = {64, 200, 200, 255};

rw::Matrix *Im3DMatrix;
rw::Texture *Im3DTexture;

void
Im3DInitialize(void)
{
	Im3DMatrix = rw::Matrix::create();
	assert(Im3DMatrix);

	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::V3d pos = rw::scale(cameraMatrix->at, 6.0f);
	Im3DMatrix->rotate(&cameraMatrix->up, 30.0f);
	Im3DMatrix->translate(&pos);

	rw::Image::setSearchPath("files/");
	Im3DTexture = rw::Texture::read("whiteash", nil);

	LineListCreate();
	LineListSetColor(!Im3DColored);

	IndexedLineListCreate();
	IndexedLineListSetColor(!Im3DColored);

	PolyLineCreate();
	PolyLineSetColor(!Im3DColored);

	IndexedPolyLineCreate();
	IndexedPolyLineSetColor(!Im3DColored);

	TriListCreate();
	TriListSetColor(!Im3DColored);

	IndexedTriListCreate();
	IndexedTriListSetColor(!Im3DColored);

	TriStripCreate();
	TriStripSetColor(!Im3DColored);

	IndexedTriStripCreate();
	IndexedTriStripSetColor(!Im3DColored);

	TriFanCreate();
	TriFanSetColor(!Im3DColored);

	IndexedTriFanCreate();
	IndexedTriFanSetColor(!Im3DColored);
}

void
Im3DTerminate(void)
{
	if(Im3DMatrix){
		Im3DMatrix->destroy();
		Im3DMatrix = nil;
	}

	if(Im3DTexture){
		Im3DTexture->destroy();
		Im3DTexture = nil;
	}
}

void
Im3DRender(void)
{
	rw::SetRenderState(rw::ZTESTENABLE, 1);
	rw::SetRenderState(rw::ZWRITEENABLE, 1);
	rw::SetRenderState(rw::SRCBLEND, rw::BLENDONE);
	rw::SetRenderState(rw::DESTBLEND, rw::BLENDZERO);
	rw::SetRenderState(rw::TEXTUREFILTER, rw::Texture::FilterMode::LINEAR);
	rw::SetRenderState(rw::CULLMODE, rw::CULLBACK);

	rw::uint32 flags;
	if(Im3DTextured){
		rw::SetRenderStatePtr(rw::TEXTURERASTER, Im3DTexture->raster);
		flags = rw::im3d::VERTEXUV | rw::im3d::ALLOPAQUE;
	}else{
		rw::SetRenderStatePtr(rw::TEXTURERASTER, nil);
		flags = rw::im3d::ALLOPAQUE;
	}

	switch(Im3DPrimType){
	case 0: LineListRender(Im3DMatrix, flags); break;
	case 1: IndexedLineListRender(Im3DMatrix, flags); break;
	case 2: PolyLineRender(Im3DMatrix, flags); break;
	case 3: IndexedPolyLineRender(Im3DMatrix, flags); break;
	case 4: TriListRender(Im3DMatrix, flags); break;
	case 5: IndexedTriListRender(Im3DMatrix, flags); break;
	case 6: TriStripRender(Im3DMatrix, flags); break;
	case 7: IndexedTriStripRender(Im3DMatrix, flags); break;
	case 8: TriFanRender(Im3DMatrix, flags); break;
	case 9: IndexedTriFanRender(Im3DMatrix, flags); break;
	}
}

void
Im3DRotate(float xAngle, float yAngle)
{
	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::V3d pos = Im3DMatrix->pos;

	pos = rw::scale(pos, -1.0f);
	Im3DMatrix->translate(&pos);

	Im3DMatrix->rotate(&cameraMatrix->up, xAngle);
	Im3DMatrix->rotate(&cameraMatrix->right, yAngle);

	pos = rw::scale(pos, -1.0f);
	Im3DMatrix->translate(&pos);
}

void
Im3DTranslateZ(float zDelta)
{
	rw::Matrix *cameraMatrix = &Camera->getFrame()->matrix;
	rw::V3d delta = rw::scale(cameraMatrix->at, zDelta);
	Im3DMatrix->translate(&delta);
}

