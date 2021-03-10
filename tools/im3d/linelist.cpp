#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

float LineListData[28][5] = {
	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f,  1.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f, -1.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f,  0.000f,  1.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.000f,  0.000f, -1.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 1.000f,  0.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-1.000f,  0.000f,  0.000f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.577f,  0.577f,  0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.577f, -0.577f,  0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-0.577f,  0.577f, -0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-0.577f, -0.577f, -0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.577f, -0.577f, -0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.577f,  0.577f, -0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-0.577f, -0.577f,  0.577f,  1.000f,  1.000f},

	{ 0.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-0.577f,  0.577f,  0.577f,  1.000f,  1.000f}       
};

float IndexedLineListData[18][5] = {
	{ 0.000f,  1.000f,  0.000f,  0.000f,  0.000f},

	{ 0.577f,  0.577f,  0.577f,  0.000f,  0.000f},
	{ 0.577f,  0.577f, -0.577f,  0.000f,  0.000f},
	{-0.577f,  0.577f, -0.577f,  0.000f,  0.000f},
	{-0.577f,  0.577f,  0.577f,  0.000f,  0.000f},

	{ 0.000f,  0.000f,  1.000f,  0.000f,  0.000f},
	{ 0.707f,  0.000f,  0.707f,  0.000f,  0.000f},
	{ 1.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{ 0.707f,  0.000f, -0.707f,  0.000f,  0.000f},
	{ 0.000f,  0.000f, -1.000f,  0.000f,  0.000f},
	{-0.707f,  0.000f, -0.707f,  0.000f,  0.000f},
	{-1.000f,  0.000f,  0.000f,  0.000f,  0.000f},
	{-0.707f,  0.000f,  0.707f,  0.000f,  0.000f},

	{ 0.577f, -0.577f,  0.577f,  0.000f,  0.000f},
	{ 0.577f, -0.577f, -0.577f,  0.000f,  0.000f},
	{-0.577f, -0.577f, -0.577f,  0.000f,  0.000f},
	{-0.577f, -0.577f,  0.577f,  0.000f,  0.000f},

	{ 0.000f, -1.000f,  0.000f,  0.000f,  0.000f}
};

rw::uint16 IndexedLineListIndices[96] = {
	0,1,   0,2,   0,3,   0,4,   1,2,   2,3,   3,4,   4,1,

	1,5,   1,6,   1,7,   2,7,   2,8,   2,9,   3,9,   3,10,  3,11,  4,11,  4,12,  4,5,
	5,6,   6,7,   7,8,   8,9,   9,10,  10,11, 11,12, 12,5,
	13,5,  13,6,  13,7,  14,7,  14,8,  14,9,  15,9,  15,10, 15,11, 16,11, 16,12, 16,5,

	17,13, 17,14, 17,15, 17,16, 13,14,  14,15, 15,16, 16,13
};

rw::RWDEVICE::Im3DVertex LineList[28];
rw::RWDEVICE::Im3DVertex IndexedLineList[18];


void
LineListCreate(void)
{
	for(int i = 0; i < 28; i++){
		LineList[i].setX(LineListData[i][0]);
		LineList[i].setY(LineListData[i][1]);
		LineList[i].setZ(LineListData[i][2]);
		LineList[i].setU(LineListData[i][3]);
		LineList[i].setV(LineListData[i][4]);
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

	for(int i = 0; i < 28; i += 2){
		LineList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
		LineList[i+1].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	}
}

void
LineListRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(LineList, 28, transform, transformFlags);
	rw::im3d::RenderPrimitive(rw::PRIMTYPELINELIST);
	rw::im3d::End();
}


void
IndexedLineListCreate(void)
{
	for(int i = 0; i < 18; i++){
		IndexedLineList[i].setX(IndexedLineListData[i][0]);
		IndexedLineList[i].setY(IndexedLineListData[i][1]);
		IndexedLineList[i].setZ(IndexedLineListData[i][2]);
		IndexedLineList[i].setU(IndexedLineListData[i][3]);
		IndexedLineList[i].setV(IndexedLineListData[i][4]);
	}
}

void
IndexedLineListSetColor(bool white)
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

	IndexedLineList[0].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
	for(i = 1; i < 5; i++)
		IndexedLineList[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	for(; i < 13; i++)
		IndexedLineList[i].setColor(SolidColor3.red, SolidColor3.green,
			SolidColor3.blue, SolidColor3.alpha);
	for(; i < 17; i++)
		IndexedLineList[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	IndexedLineList[i].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
}

void
IndexedLineListRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(IndexedLineList, 18, transform, transformFlags);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPELINELIST, IndexedLineListIndices, 96);
	rw::im3d::End();
}
