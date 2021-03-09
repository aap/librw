#include <rw.h>
#include <skeleton.h>

#include "im2d.h"

float TriStripData[18][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{ 0.000f,  0.500f,  0.500f,  0.750f},

	{ 0.707f,  0.707f,  0.853f,  0.853f},
	{ 0.354f,  0.354f,  0.677f,  0.677f},

	{ 1.000f,  0.000f,  1.000f,  0.500f},
	{ 0.500f,  0.000f,  0.750f,  0.500f},

	{ 0.707f, -0.707f,  0.853f,  0.147f},
	{ 0.354f, -0.354f,  0.677f,  0.323f},

	{ 0.000f, -1.000f,  0.500f,  0.000f},
	{ 0.000f, -0.500f,  0.500f,  0.250f},

	{-0.707f, -0.707f,  0.147f,  0.147f},
	{-0.354f, -0.354f,  0.323f,  0.323f},

	{-1.000f,  0.000f,  0.000f,  0.500f},
	{-0.500f,  0.000f,  0.250f,  0.500f},

	{-0.707f,  0.707f,  0.147f,  0.853f},
	{-0.354f,  0.354f,  0.323f,  0.677f},

	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{ 0.000f,  0.500f,  0.500f,  0.750f}
};

float IndexedTriStripData[16][4] = {
	{ 0.000f,  1.000f,  0.500f,  1.000f},
	{ 0.707f,  0.707f,  0.853f,  0.853f},
	{ 1.000f,  0.000f,  1.000f,  0.500f},
	{ 0.707f, -0.707f,  0.853f,  0.147f},
	{ 0.000f, -1.000f,  0.500f,  0.000f},
	{-0.707f, -0.707f,  0.147f,  0.147f},
	{-1.000f,  0.000f,  0.000f,  0.500f},
	{-0.707f,  0.707f,  0.147f,  0.853f},

	{ 0.000f,  0.500f,  0.500f,  0.750f},
	{ 0.354f,  0.354f,  0.677f,  0.677f},
	{ 0.500f,  0.000f,  0.750f,  0.500f},
	{ 0.354f, -0.354f,  0.677f,  0.323f},
	{ 0.000f, -0.500f,  0.500f,  0.250f},
	{-0.354f, -0.354f,  0.323f,  0.323f},
	{-0.500f,  0.000f,  0.250f,  0.500f},
	{-0.354f,  0.354f,  0.323f,  0.677f}
};

rw::uint16 IndexedTriStripIndices[18] = {
	0, 8,  1, 9,  2, 10, 3, 11,
	4, 12, 5, 13, 6, 14, 7, 15, 0, 8
};

rw::RWDEVICE::Im2DVertex TriStrip[18];
rw::RWDEVICE::Im2DVertex IndexedTriStrip[16];


void
TriStripCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 18; i++){
		TriStrip[i].setScreenX(ScreenSize.x/2.0f + TriStripData[i][0]*Scale);
		TriStrip[i].setScreenY(ScreenSize.y/2.0f - TriStripData[i][1]*Scale);
		TriStrip[i].setScreenZ(rw::im2d::GetNearZ());
		TriStrip[i].setRecipCameraZ(recipZ);
		TriStrip[i].setU(TriStripData[i][2], recipZ);
		TriStrip[i].setV(TriStripData[i][3], recipZ);
	}
}

void
TriStripSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidPurple;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 18; i++)
		TriStrip[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
TriStripRender(void)
{
	rw::im2d::RenderPrimitive(rw::PRIMTYPETRISTRIP, TriStrip, 18);
}


void
IndexedTriStripCreate(rw::Camera *camera)
{
	float recipZ = 1.0f/camera->nearPlane;
	for(int i = 0; i < 16; i++){
		IndexedTriStrip[i].setScreenX(ScreenSize.x/2.0f + IndexedTriStripData[i][0]*Scale);
		IndexedTriStrip[i].setScreenY(ScreenSize.y/2.0f - IndexedTriStripData[i][1]*Scale);
		IndexedTriStrip[i].setScreenZ(rw::im2d::GetNearZ());
		IndexedTriStrip[i].setRecipCameraZ(recipZ);
		IndexedTriStrip[i].setU(IndexedTriStripData[i][2], recipZ);
		IndexedTriStrip[i].setV(IndexedTriStripData[i][3], recipZ);
	}
}

void
IndexedTriStripSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidCyan;

	if(white)
		SolidColor1 = SolidWhite;

	for(int i = 0; i < 16; i++)
		IndexedTriStrip[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedTriStripRender(void)
{
	rw::im2d::RenderIndexedPrimitive(rw::PRIMTYPETRISTRIP,
		IndexedTriStrip, 16, IndexedTriStripIndices, 18);
}
