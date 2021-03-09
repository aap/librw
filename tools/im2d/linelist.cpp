#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

float LineListData[32][4] = {
	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f,  1.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.383f,  0.924f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.707f,  0.707f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.924f,  0.383f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 1.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.924f, -0.383f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.707f, -0.707f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.383f, -0.924f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f, -1.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.383f, -0.924f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.707f, -0.707f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.924f, -0.383f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-1.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.924f,  0.383f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.707f,  0.707f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f},
	{-0.383f,  0.924f,  1.000f,  1.000f}
};

float IndexedLineListData[16][4] = {
	{-1.000f,  1.000f,  0.000f,  1.000f},
	{-0.500f,  1.000f,  0.250f,  1.000f},
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{ 0.500f,  1.000f,  0.750f,  1.000f},
	{ 1.000f,  1.000f,  1.000f,  1.000f},

	{-1.000f,  0.500f,  0.000f,  0.750f},
	{-1.000f,  0.000f,  0.000f,  0.500f},
	{-1.000f, -0.500f,  0.000f,  0.250f},

	{ 1.000f,  0.500f,  1.000f,  0.750f},
	{ 1.000f,  0.000f,  1.000f,  0.500f},
	{ 1.000f, -0.500f,  1.000f,  0.250f},

	{-1.000f, -1.000f,  0.000f,  0.000f},
	{-0.500f, -1.000f,  0.250f,  0.000f},
	{ 0.000f, -1.000f,  0.500f,  0.000f},
	{ 0.500f, -1.000f,  0.750f,  0.000f},
	{ 1.000f, -1.000f,  1.000f,  0.000f}
};

rw::uint16 IndexedLineListIndices[20] = {
	0, 11, 1, 12, 2, 13, 3, 14, 4, 15, 
	0, 4, 5, 8, 6, 9, 7, 10, 11, 15
};

rw::RWDEVICE::Im2DVertex LineList[32];
rw::RWDEVICE::Im2DVertex IndexedLineList[16];


void
LineListCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 32; i++){
		LineList[i].setScreenX(ScreenSize.x/2.0f + LineListData[i][0]*Scale);
		LineList[i].setScreenY(ScreenSize.y/2.0f - LineListData[i][1]*Scale);
		LineList[i].setScreenZ(rw::im2d::GetNearZ());
		LineList[i].setRecipCameraZ(recipZ);
		LineList[i].setU(LineListData[i][2], recipZ);
		LineList[i].setV(LineListData[i][3], recipZ);
	}
}

void
LineListSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidRed;
	rw::RGBA SolidColor2 = SolidWhite;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
	}

	for(int i = 0; i < 32; i += 2){
		LineList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
		LineList[i+1].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	}
}

void
LineListRender(void)
{
	rw::im2d::RenderPrimitive(rw::PRIMTYPELINELIST, LineList, 32);
}


void
IndexedLineListCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 16; i++){
		IndexedLineList[i].setScreenX(ScreenSize.x/2.0f + IndexedLineListData[i][0]*Scale);
		IndexedLineList[i].setScreenY(ScreenSize.y/2.0f - IndexedLineListData[i][1]*Scale);
		IndexedLineList[i].setScreenZ(rw::im2d::GetNearZ());
		IndexedLineList[i].setRecipCameraZ(recipZ);
		IndexedLineList[i].setU(IndexedLineListData[i][2], recipZ);
		IndexedLineList[i].setV(IndexedLineListData[i][3], recipZ);
	}
}

void
IndexedLineListSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidRed;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 16; i++)
		IndexedLineList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedLineListRender(void)
{
	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPELINELIST,
		IndexedLineList, 16, IndexedLineListIndices, 20);
}
