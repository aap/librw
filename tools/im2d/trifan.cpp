#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

float TriFanData[17][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{-0.383f,  0.924f,  0.308f,  0.962f},
	{-0.707f,  0.707f,  0.146f,  0.854f},
	{-0.924f,  0.383f,  0.038f,  0.692f},
	{-1.000f,  0.000f,  0.000f,  0.500f},
	{-0.924f, -0.383f,  0.038f,  0.308f},
	{-0.707f, -0.707f,  0.146f,  0.146f},
	{-0.383f, -0.924f,  0.308f,  0.038f},
	{ 0.000f, -1.000f,  0.500f,  0.000f},
	{ 0.383f, -0.924f,  0.692f,  0.038f},
	{ 0.707f, -0.707f,  0.854f,  0.146f},
	{ 0.924f, -0.383f,  0.962f,  0.308f},
	{ 1.000f,  0.000f,  1.000f,  0.500f},
	{ 0.924f,  0.383f,  0.962f,  0.692f},
	{ 0.707f,  0.707f,  0.854f,  0.854f},
	{ 0.383f,  0.924f,  0.692f,  0.962f},
	{ 0.000f,  1.000f,  0.500f,  1.000f}
};

float IndexedTriFanData[16][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{-0.383f,  0.924f,  0.308f,  0.962f},
	{-0.707f,  0.707f,  0.146f,  0.854f},
	{-0.924f,  0.383f,  0.038f,  0.692f},
	{-1.000f,  0.000f,  0.000f,  0.500f},
	{-0.924f, -0.383f,  0.038f,  0.308f},
	{-0.707f, -0.707f,  0.146f,  0.146f},
	{-0.383f, -0.924f,  0.308f,  0.038f},
	{ 0.000f, -1.000f,  0.500f,  0.000f},
	{ 0.383f, -0.924f,  0.692f,  0.038f},
	{ 0.707f, -0.707f,  0.854f,  0.146f},
	{ 0.924f, -0.383f,  0.962f,  0.308f},
	{ 1.000f,  0.000f,  1.000f,  0.500f},
	{ 0.924f,  0.383f,  0.962f,  0.692f},
	{ 0.707f,  0.707f,  0.854f,  0.854f},
	{ 0.383f,  0.924f,  0.692f,  0.962f}
};

rw::uint16 IndexedTriFanIndices[17] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0
};

rw::RWDEVICE::Im2DVertex TriFan[17];
rw::RWDEVICE::Im2DVertex IndexedTriFan[16];


void
TriFanCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 17; i++){
		TriFan[i].setScreenX(ScreenSize.x/2.0f + TriFanData[i][0]*Scale);
		TriFan[i].setScreenY(ScreenSize.y/2.0f - TriFanData[i][1]*Scale);
		TriFan[i].setScreenZ(rw::im2d::GetNearZ());
		TriFan[i].setRecipCameraZ(recipZ);
		TriFan[i].setU(TriFanData[i][2], recipZ);
		TriFan[i].setV(TriFanData[i][3], recipZ);
	}
}

void
TriFanSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidYellow;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 17; i++)
		TriFan[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
TriFanRender(void)
{
	rw::im2d::RenderPrimitive(rw::PRIMTYPETRIFAN, TriFan, 17);
}


void
IndexedTriFanCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 16; i++){
		IndexedTriFan[i].setScreenX(ScreenSize.x/2.0f + IndexedTriFanData[i][0]*Scale);
		IndexedTriFan[i].setScreenY(ScreenSize.y/2.0f - IndexedTriFanData[i][1]*Scale);
		IndexedTriFan[i].setScreenZ(rw::im2d::GetNearZ());
		IndexedTriFan[i].setRecipCameraZ(recipZ);
		IndexedTriFan[i].setU(IndexedTriFanData[i][2], recipZ);
		IndexedTriFan[i].setV(IndexedTriFanData[i][3], recipZ);
	}
}

void
IndexedTriFanSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidGreen;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 16; i++)
		IndexedTriFan[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedTriFanRender(void)
{
	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRIFAN,
		IndexedTriFan, 16, IndexedTriFanIndices, 17);
}
