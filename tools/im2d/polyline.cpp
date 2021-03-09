#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

float PolyLineData[16][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{ 0.672f,  0.672f,  0.836f,  0.836f},
	{ 0.900f,  0.000f,  0.950f,  0.500f},
	{ 0.601f, -0.601f,  0.800f,  0.200f},
	{ 0.000f, -0.800f,  0.500f,  0.100f},
	{-0.530f, -0.530f,  0.245f,  0.245f},
	{-0.700f,  0.000f,  0.150f,  0.500f},
	{-0.460f,  0.460f,  0.270f,  0.770f},
	{ 0.000f,  0.600f,  0.500f,  0.800f},
	{ 0.389f,  0.389f,  0.695f,  0.695f},
	{ 0.500f,  0.000f,  0.750f,  0.500f},
	{ 0.318f, -0.318f,  0.659f,  0.341f},
	{ 0.000f, -0.400f,  0.500f,  0.300f},
	{-0.247f, -0.247f,  0.376f,  0.376f},
	{-0.300f,  0.000f,  0.350f,  0.500f},
	{-0.177f,  0.177f,  0.411f,  0.589f}
};

float IndexedPolyLineData[21][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},

	{-0.200f,  0.600f,  0.400f,  0.800f},
	{ 0.200f,  0.600f,  0.600f,  0.800f},
    
	{-0.400f,  0.200f,  0.300f,  0.600f},
	{ 0.000f,  0.200f,  0.500f,  0.600f},
	{ 0.400f,  0.200f,  0.700f,  0.600f},

	{-0.600f, -0.200f,  0.200f,  0.400f},
	{-0.200f, -0.200f,  0.400f,  0.400f},
	{ 0.200f, -0.200f,  0.600f,  0.400f},
	{ 0.600f, -0.200f,  0.800f,  0.400f},

	{-0.800f, -0.600f,  0.100f,  0.200f},
	{-0.400f, -0.600f,  0.300f,  0.200f},
	{ 0.000f, -0.600f,  0.500f,  0.200f},
	{ 0.400f, -0.600f,  0.700f,  0.200f},
	{ 0.800f, -0.600f,  0.900f,  0.200f},

	{-1.000f, -1.000f,  0.000f,  0.000f},
	{-0.600f, -1.000f,  0.200f,  0.000f},
	{-0.200f, -1.000f,  0.400f,  0.000f},
	{ 0.200f, -1.000f,  0.600f,  0.000f},
	{ 0.600f, -1.000f,  0.800f,  0.000f},
	{ 1.000f, -1.000f,  1.000f,  0.000f}
};

rw::uint16 IndexedPolyLineIndices[46] = {
	 0,  2,  5,  4,  8,  5,  9,  8, 13,  9,
	14, 13, 19, 14, 20, 19, 18, 13, 12,  8,
	 7, 12, 18, 17, 12, 11, 17, 16, 15, 10,
	16, 11, 10,  6, 11,  7,  6,  3,  7,  4,
	 3, 1, 4, 2, 1, 0
};

rw::RWDEVICE::Im2DVertex PolyLine[16];
rw::RWDEVICE::Im2DVertex IndexedPolyLine[21];


void
PolyLineCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 16; i++){
		PolyLine[i].setScreenX(ScreenSize.x/2.0f + PolyLineData[i][0]*Scale);
		PolyLine[i].setScreenY(ScreenSize.y/2.0f - PolyLineData[i][1]*Scale);
		PolyLine[i].setScreenZ(rw::im2d::GetNearZ());
		PolyLine[i].setRecipCameraZ(recipZ);
		PolyLine[i].setU(PolyLineData[i][2], recipZ);
		PolyLine[i].setV(PolyLineData[i][3], recipZ);
	}
}

void
PolyLineSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidBlue;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 16; i++)
		PolyLine[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
PolyLineRender(void)
{
	rw::im2d::RenderPrimitive(rw::PRIMTYPEPOLYLINE, PolyLine, 16);
}


void
IndexedPolyLineCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 21; i++){
		IndexedPolyLine[i].setScreenX(ScreenSize.x/2.0f + IndexedPolyLineData[i][0]*Scale);
		IndexedPolyLine[i].setScreenY(ScreenSize.y/2.0f - IndexedPolyLineData[i][1]*Scale);
		IndexedPolyLine[i].setScreenZ(rw::im2d::GetNearZ());
		IndexedPolyLine[i].setRecipCameraZ(recipZ);
		IndexedPolyLine[i].setU(IndexedPolyLineData[i][2], recipZ);
		IndexedPolyLine[i].setV(IndexedPolyLineData[i][3], recipZ);
	}
}

void
IndexedPolyLineSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidBlue;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 21; i++)
		IndexedPolyLine[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedPolyLineRender(void)
{
	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPEPOLYLINE,
		IndexedPolyLine, 21, IndexedPolyLineIndices, 46);
}
