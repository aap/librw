#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

float PolyLineData[21][5] = {
	{ 0.000f,  1.000f,  1.000f,  0.500f,  1.000f},
	{ 0.707f,  0.707f,  0.900f,  0.854f,  0.854f},
	{ 1.000f,  0.000f,  0.800f,  1.000f,  0.500f},
	{ 0.707f, -0.707f,  0.700f,  0.854f,  0.146f},
	{ 0.000f, -1.000f,  0.600f,  0.500f,  0.000f},
	{-0.707f, -0.707f,  0.500f,  0.146f,  0.146f},
	{-1.000f, -0.000f,  0.400f,  0.000f,  0.500f},
	{-0.707f,  0.707f,  0.300f,  0.146f,  0.854f},

	{ 0.000f,  1.000f,  0.200f,  0.500f,  1.000f},
	{ 0.707f,  0.707f,  0.100f,  0.854f,  0.854f},
	{ 1.000f,  0.000f,  0.000f,  1.000f,  0.500f},
	{ 0.707f, -0.707f, -0.100f,  0.854f,  0.146f},
	{ 0.000f, -1.000f, -0.200f,  0.500f,  0.000f},
	{-0.707f, -0.707f, -0.300f,  0.146f,  0.146f},
	{-1.000f, -0.000f, -0.400f,  0.000f,  0.500f},
	{-0.707f,  0.707f, -0.500f,  0.146f,  0.854f},

	{ 0.000f,  1.000f, -0.600f,  0.500f,  1.000f},
	{ 0.707f,  0.707f, -0.700f,  0.854f,  0.854f},
	{ 1.000f,  0.000f, -0.800f,  1.000f,  0.500f},
	{ 0.707f, -0.707f, -0.900f,  0.854f,  0.146f},
	{ 0.000f, -1.000f, -1.000f,  0.500f,  0.000f}
};

float IndexedPolyLineData[8][5] = {
	{ 1.000f,  1.000f,  1.000f,  1.000f,  0.000f},
	{-1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{-1.000f, -1.000f,  1.000f,  0.500f,  1.000f},    
	{ 1.000f, -1.000f,  1.000f,  0.500f,  0.000f},    

	{ 1.000f,  1.000f, -1.000f,  0.500f,  0.000f},
	{-1.000f,  1.000f, -1.000f,  0.500f,  1.000f},
	{-1.000f, -1.000f, -1.000f,  0.000f,  1.000f},    
	{ 1.000f, -1.000f, -1.000f,  0.000f,  0.000f}
};

rw::uint16 IndexedPolyLineIndices[25] = {
	0, 1, 2, 3, 0, 2, 6, 5, 1, 3, 7, 4, 0, 5, 4, 6, 1, 4, 3, 6, 7, 5, 2, 7, 0
};

rw::RWDEVICE::Im3DVertex PolyLine[21];
rw::RWDEVICE::Im3DVertex IndexedPolyLine[8];


void
PolyLineCreate(void)
{
	for(int i = 0; i < 21; i++){
		PolyLine[i].setX(PolyLineData[i][0]);
		PolyLine[i].setY(PolyLineData[i][1]);
		PolyLine[i].setZ(PolyLineData[i][2]);
		PolyLine[i].setU(PolyLineData[i][3]);
		PolyLine[i].setV(PolyLineData[i][4]);
	}
}

void
PolyLineSetColor(bool white)
{
	int i;
	rw::RGBA SolidColor1 = SolidRed;
	rw::RGBA SolidColor2 = SolidGreen;
	rw::RGBA SolidColor3 = SolidBlue;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
		SolidColor3 = SolidWhite;
	}

	for(i = 0; i < 7; i++)
		PolyLine[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
	for(; i < 14; i++)
		PolyLine[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	for(; i < 21; i++)
		PolyLine[i].setColor(SolidColor3.red, SolidColor3.green,
			SolidColor3.blue, SolidColor3.alpha);
}

void
PolyLineRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(PolyLine, 21, transform, transformFlags);
	rw::im3d::RenderPrimitive(rw::PRIMTYPEPOLYLINE);
	rw::im3d::End();
}


void
IndexedPolyLineCreate(void)
{
	for(int i = 0; i < 8; i++){
		IndexedPolyLine[i].setX(IndexedPolyLineData[i][0]);
		IndexedPolyLine[i].setY(IndexedPolyLineData[i][1]);
		IndexedPolyLine[i].setZ(IndexedPolyLineData[i][2]);
		IndexedPolyLine[i].setU(IndexedPolyLineData[i][3]);
		IndexedPolyLine[i].setV(IndexedPolyLineData[i][4]);
	}
}

void
IndexedPolyLineSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidRed;
	rw::RGBA SolidColor2 = SolidYellow;
	rw::RGBA SolidColor3 = SolidBlack;
	rw::RGBA SolidColor4 = SolidPurple;
	rw::RGBA SolidColor5 = SolidGreen;
	rw::RGBA SolidColor6 = SolidCyan;
	rw::RGBA SolidColor7 = SolidBlue;
	rw::RGBA SolidColor8 = SolidWhite;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
		SolidColor3 = SolidWhite;
		SolidColor4 = SolidWhite;
		SolidColor5 = SolidWhite;
		SolidColor6 = SolidWhite;
		SolidColor7 = SolidWhite;
		SolidColor8 = SolidWhite;
	}

	IndexedPolyLine[0].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
	IndexedPolyLine[1].setColor(SolidColor2.red, SolidColor2.green,
		SolidColor2.blue, SolidColor2.alpha);
	IndexedPolyLine[2].setColor(SolidColor3.red, SolidColor3.green,
		SolidColor3.blue, SolidColor3.alpha);
	IndexedPolyLine[3].setColor(SolidColor4.red, SolidColor4.green,
		SolidColor4.blue, SolidColor4.alpha);
	IndexedPolyLine[4].setColor(SolidColor5.red, SolidColor5.green,
		SolidColor5.blue, SolidColor5.alpha);
	IndexedPolyLine[5].setColor(SolidColor6.red, SolidColor6.green,
		SolidColor6.blue, SolidColor6.alpha);
	IndexedPolyLine[6].setColor(SolidColor7.red, SolidColor7.green,
		SolidColor7.blue, SolidColor7.alpha);
	IndexedPolyLine[7].setColor(SolidColor8.red, SolidColor8.green,
		SolidColor8.blue, SolidColor8.alpha);
}

void
IndexedPolyLineRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(IndexedPolyLine, 8, transform, transformFlags);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPEPOLYLINE, IndexedPolyLineIndices, 25);
	rw::im3d::End();
}
