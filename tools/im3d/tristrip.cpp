#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

float TriStripData[36][5] = {
	{ 0.000f,  1.000f, -1.000f,  0.000f,  0.000f},
	{ 0.000f,  1.000f,  1.000f,  0.000f,  1.000f},

	{ 0.707f,  0.707f, -1.000f,  0.125f,  0.000f},
	{ 0.707f,  0.707f,  1.000f,  0.125f,  1.000f},

	{ 1.000f,  0.000f, -1.000f,  0.250f,  0.000f},
	{ 1.000f,  0.000f,  1.000f,  0.250f,  1.000f},

	{ 0.707f, -0.707f, -1.000f,  0.375f,  0.000f},
	{ 0.707f, -0.707f,  1.000f,  0.375f,  1.000f},
    
	{ 0.000f, -1.000f, -1.000f,  0.500f,  0.000f},
	{ 0.000f, -1.000f,  1.000f,  0.500f,  1.000f},
    
	{-0.707f, -0.707f, -1.000f,  0.625f,  0.000f},
	{-0.707f, -0.707f,  1.000f,  0.625f,  1.000f},
    
	{-1.000f, -0.000f, -1.000f,  0.750f,  0.000f},
	{-1.000f, -0.000f,  1.000f,  0.750f,  1.000f},
    
	{-0.707f,  0.707f, -1.000f,  0.875f,  0.000f},
	{-0.707f,  0.707f,  1.000f,  0.875f,  1.000f},

	{ 0.000f,  1.000f, -1.000f,  1.000f,  0.000f},
	{ 0.000f,  1.000f,  1.000f,  1.000f,  1.000f},

	{ 0.000f,  1.000f,  1.000f,  0.000f,  0.000f},
	{ 0.000f,  1.000f, -1.000f,  0.000f,  1.000f},

	{ 0.707f,  0.707f,  1.000f,  0.125f,  0.000f},
	{ 0.707f,  0.707f, -1.000f,  0.125f,  1.000f},
    
	{ 1.000f,  0.000f,  1.000f,  0.250f,  0.000f},
	{ 1.000f,  0.000f, -1.000f,  0.250f,  1.000f},
    
	{ 0.707f, -0.707f,  1.000f,  0.375f,  0.000f},
	{ 0.707f, -0.707f, -1.000f,  0.375f,  1.000f},
    
	{ 0.000f, -1.000f,  1.000f,  0.500f,  0.000f},
	{ 0.000f, -1.000f, -1.000f,  0.500f,  1.000f},
    
	{-0.707f, -0.707f,  1.000f,  0.625f,  0.000f},
	{-0.707f, -0.707f, -1.000f,  0.625f,  1.000f},
    
	{-1.000f, -0.000f,  1.000f,  0.750f,  0.000f},
	{-1.000f, -0.000f, -1.000f,  0.750f,  1.000f},
    
	{-0.707f,  0.707f,  1.000f,  0.875f,  0.000f},
	{-0.707f,  0.707f, -1.000f,  0.875f,  1.000f},
    
	{ 0.000f,  1.000f,  1.000f,  1.000f,  0.000f},
	{ 0.000f,  1.000f, -1.000f,  1.000f,  1.000f}      
};

float IndexedTriStripData[16][5] = {
	{ 0.000f,  1.000f,  1.000f,  0.000f,  0.000f},
	{ 0.707f,  0.707f,  1.000f,  0.250f,  0.000f},
	{ 1.000f,  0.000f,  1.000f,  0.500f,  0.000f},
	{ 0.707f, -0.707f,  1.000f,  0.750f,  0.000f},
	{ 0.000f, -1.000f,  1.000f,  1.000f,  0.000f},
	{-0.707f, -0.707f,  1.000f,  0.750f,  0.000f},
	{-1.000f, -0.000f,  1.000f,  0.500f,  0.000f},
	{-0.707f,  0.707f,  1.000f,  0.250f,  0.000f},

	{ 0.000f,  1.000f, -1.000f,  0.000f,  1.000f},
	{ 0.707f,  0.707f, -1.000f,  0.250f,  1.000f},
	{ 1.000f,  0.000f, -1.000f,  0.500f,  1.000f},
	{ 0.707f, -0.707f, -1.000f,  0.750f,  1.000f},
	{ 0.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{-0.707f, -0.707f, -1.000f,  0.750f,  1.000f},
	{-1.000f, -0.000f, -1.000f,  0.500f,  1.000f},
	{-0.707f,  0.707f, -1.000f,  0.250f,  1.000f},
};

rw::uint16 IndexedTriStripIndices[36] = {
	0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15, 0, 8,
	8, 0, 9, 1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7, 8, 0
};

rw::RWDEVICE::Im3DVertex TriStrip[36];
rw::RWDEVICE::Im3DVertex IndexedTriStrip[16];


void
TriStripCreate(void)
{
	for(int i = 0; i < 36; i++){
		TriStrip[i].setX(TriStripData[i][0]);
		TriStrip[i].setY(TriStripData[i][1]);
		TriStrip[i].setZ(TriStripData[i][2]);
		TriStrip[i].setU(TriStripData[i][3]);
		TriStrip[i].setV(TriStripData[i][4]);
	}
}

void
TriStripSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidRed;
	rw::RGBA SolidColor2 = SolidYellow;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
	}

	for(int i = 0; i < 36; i += 2){
		TriStrip[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
		TriStrip[i+1].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	}
}

void
TriStripRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(TriStrip, 36, transform, transformFlags);
	rw::im3d::RenderPrimitive(rw::PRIMTYPETRISTRIP);
	rw::im3d::End();
}


void
IndexedTriStripCreate(void)
{
	for(int i = 0; i < 16; i++){
		IndexedTriStrip[i].setX(IndexedTriStripData[i][0]);
		IndexedTriStrip[i].setY(IndexedTriStripData[i][1]);
		IndexedTriStrip[i].setZ(IndexedTriStripData[i][2]);
		IndexedTriStrip[i].setU(IndexedTriStripData[i][3]);
		IndexedTriStrip[i].setV(IndexedTriStripData[i][4]);
	}
}

void
IndexedTriStripSetColor(bool white)
{
	int i;
	rw::RGBA SolidColor1 = SolidBlue;
	rw::RGBA SolidColor2 = SolidGreen;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
	}

	for(i = 0; i < 8; i++)
		IndexedTriStrip[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
	for(; i < 16; i++)
		IndexedTriStrip[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
}

void
IndexedTriStripRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(IndexedTriStrip, 16, transform, transformFlags);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRISTRIP, IndexedTriStripIndices, 36);
	rw::im3d::End();
}
