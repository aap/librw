#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

float TriListData[18][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{-0.500f,  0.500f,  0.250f,  0.750f},
	{ 0.500f,  0.500f,  0.750f,  0.750f},

	{-0.500f,  0.500f,  0.250f,  0.750f},
	{ 0.500f, -0.500f,  0.750f,  0.250f},
	{ 0.500f,  0.500f,  0.750f,  0.750f},

	{ 0.500f,  0.500f,  0.750f,  0.750f},
	{ 0.500f, -0.500f,  0.750f,  0.250f},
	{ 1.000f,  0.000f,  1.000f,  0.500f},
   
	{ 0.500f, -0.500f,  0.750f,  0.250f},
	{-0.500f, -0.500f,  0.250f,  0.250f},
	{ 0.000f, -1.000f,  0.500f,  0.000f},

	{ 0.500f, -0.500f,  0.750f,  1.250f},
	{-0.500f,  0.500f,  0.250f,  1.750f},
	{-0.500f, -0.500f,  0.250f,  1.250f},

	{-0.500f, -0.500f,  0.250f,  0.250f},
	{-0.500f,  0.500f,  0.250f,  0.750f},
	{-1.000f,  0.000f,  0.000f,  0.500f}
};

float IndexedTriListData[21][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},

	{-0.200f,  0.600f,  0.400f,  0.800f},
	{ 0.200f,  0.600f,  0.600f,  0.800f},
    
	{-0.400f,  0.200f,  0.300f,  0.600f},
	{ 0.000f,  0.200f,  0.500f,  0.600f},
	{ 0.400f,  0.200f,  0.300f,  0.600f},

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

rw::uint16 IndexedTriListIndices[45] = {
	0, 1, 2,
	1, 3, 4, 2, 4, 5,
	3, 6, 7, 4, 7, 8, 5, 8, 9,
	6, 10, 11, 7, 11, 12, 8, 12, 13, 9, 13, 14,
	10, 15, 16, 11, 16, 17, 12, 17, 18, 13, 18, 19, 14, 19, 20
};

rw::RWDEVICE::Im2DVertex TriList[18];
rw::RWDEVICE::Im2DVertex IndexedTriList[21];


void
TriListCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 18; i++){
		TriList[i].setScreenX(ScreenSize.x/2.0f + TriListData[i][0]*Scale);
		TriList[i].setScreenY(ScreenSize.y/2.0f - TriListData[i][1]*Scale);
		TriList[i].setScreenZ(rw::im2d::GetNearZ());
		TriList[i].setRecipCameraZ(recipZ);
		TriList[i].setU(TriListData[i][2], recipZ);
		TriList[i].setV(TriListData[i][3], recipZ);
	}
}

void
TriListSetColor(bool white)
{
	int i;
	rw::RGBA SolidColor1 = SolidBlue;
	rw::RGBA SolidColor2 = SolidRed;
	rw::RGBA SolidColor3 = SolidGreen;
	rw::RGBA SolidColor4 = SolidYellow;
	rw::RGBA SolidColor5 = SolidPurple;
	rw::RGBA SolidColor6 = SolidCyan;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
		SolidColor3 = SolidWhite;
		SolidColor4 = SolidWhite;
		SolidColor5 = SolidWhite;
		SolidColor6 = SolidWhite;
	}

	for(i = 0; i < 3; i++)
		TriList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
	for(; i < 6; i++)
		TriList[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	for(; i < 9; i++)
		TriList[i].setColor(SolidColor3.red, SolidColor3.green,
			SolidColor3.blue, SolidColor3.alpha);
	for(; i < 12; i++)
		TriList[i].setColor(SolidColor4.red, SolidColor4.green,
			SolidColor4.blue, SolidColor4.alpha);
	for(; i < 15; i++)
		TriList[i].setColor(SolidColor5.red, SolidColor5.green,
			SolidColor5.blue, SolidColor5.alpha);
	for(; i < 18; i++)
		TriList[i].setColor(SolidColor6.red, SolidColor6.green,
			SolidColor6.blue, SolidColor6.alpha);
}

void
TriListRender(void)
{
	rw::im2d::RenderPrimitive(rw::PRIMTYPETRILIST, TriList, 18);
}


void
IndexedTriListCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 21; i++){
		IndexedTriList[i].setScreenX(ScreenSize.x/2.0f + IndexedTriListData[i][0]*Scale);
		IndexedTriList[i].setScreenY(ScreenSize.y/2.0f - IndexedTriListData[i][1]*Scale);
		IndexedTriList[i].setScreenZ(rw::im2d::GetNearZ());
		IndexedTriList[i].setRecipCameraZ(recipZ);
		IndexedTriList[i].setU(IndexedTriListData[i][2], recipZ);
		IndexedTriList[i].setV(IndexedTriListData[i][3], recipZ);
	}
}

void
IndexedTriListSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidBlue;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 21; i++)
		IndexedTriList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedTriListRender(void)
{
	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST,
		IndexedTriList, 21, IndexedTriListIndices, 45);
}
