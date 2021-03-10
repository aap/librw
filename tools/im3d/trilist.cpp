#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

float TriListData[36][5] = {
	/* front */
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{-1.000f, -1.000f,  1.000f,  0.000f,  0.000f},
	{ 1.000f, -1.000f,  1.000f,  1.000f,  0.000f},
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{-1.000f,  1.000f,  1.000f,  0.000f,  1.000f},
	{-1.000f, -1.000f,  1.000f,  0.000f,  0.000f},
	/* back */
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{-1.000f,  1.000f, -1.000f,  1.000f,  0.000f},
	{ 1.000f,  1.000f, -1.000f,  0.000f,  0.000f},
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{ 1.000f,  1.000f, -1.000f,  0.000f,  0.000f},
	{ 1.000f, -1.000f, -1.000f,  0.000f,  1.000f},
	/* top */ 
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{ 1.000f,  1.000f, -1.000f,  1.000f,  0.000f},
	{-1.000f,  1.000f, -1.000f,  0.000f,  0.000f},
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{-1.000f,  1.000f, -1.000f,  0.000f,  0.000f},
	{-1.000f,  1.000f,  1.000f,  0.000f,  1.000f},
	/* bottom */
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{ 1.000f, -1.000f,  1.000f,  0.000f,  0.000f},
	{-1.000f, -1.000f,  1.000f,  1.000f,  0.000f},
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{ 1.000f, -1.000f, -1.000f,  0.000f,  1.000f},
	{ 1.000f, -1.000f,  1.000f,  0.000f,  0.000f},
	/* left */
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{-1.000f,  1.000f,  1.000f,  0.000f,  0.000f},
	{-1.000f,  1.000f, -1.000f,  1.000f,  0.000f},
	{-1.000f, -1.000f, -1.000f,  1.000f,  1.000f},
	{-1.000f, -1.000f,  1.000f,  0.000f,  1.000f},
	{-1.000f,  1.000f,  1.000f,  0.000f,  0.000f},
	/* right */
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{ 1.000f, -1.000f,  1.000f,  1.000f,  0.000f},
	{ 1.000f, -1.000f, -1.000f,  0.000f,  0.000f},
	{ 1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{ 1.000f, -1.000f, -1.000f,  0.000f,  0.000f},
	{ 1.000f,  1.000f, -1.000f,  0.000f,  1.000f}
};

float IndexedTriListData[8][5] = {
	{ 1.000f,  1.000f,  1.000f,  1.000f,  0.000f},
	{-1.000f,  1.000f,  1.000f,  1.000f,  1.000f},
	{-1.000f, -1.000f,  1.000f,  0.500f,  1.000f},
	{ 1.000f, -1.000f,  1.000f,  0.500f,  0.000f},

	{ 1.000f,  1.000f, -1.000f,  0.500f,  0.000f},
	{-1.000f,  1.000f, -1.000f,  0.500f,  1.000f},
	{-1.000f, -1.000f, -1.000f,  0.000f,  1.000f},
	{ 1.000f, -1.000f, -1.000f,  0.000f,  0.000f}    
};

rw::uint16 IndexedTriListIndices[36] = {
	/* front */
	0, 1, 3,  1, 2, 3,
	/* back */
	7, 5, 4,  5, 7, 6, 
	/* left */
	6, 2, 1,  1, 5, 6,
	/* right */
	0, 3, 4,  4, 3, 7, 
	/* top */
	1, 0, 4,  1, 4, 5,
	/* bottom */
	2, 6, 3,  6, 7, 3
};

rw::RWDEVICE::Im3DVertex TriList[36];
rw::RWDEVICE::Im3DVertex IndexedTriList[8];


void
TriListCreate(void)
{
	for(int i = 0; i < 36; i++){
		TriList[i].setX(TriListData[i][0]);
		TriList[i].setY(TriListData[i][1]);
		TriList[i].setZ(TriListData[i][2]);
		TriList[i].setU(TriListData[i][3]);
		TriList[i].setV(TriListData[i][4]);
	}
}

void
TriListSetColor(bool white)
{
	int i;
	rw::RGBA SolidColor1 = SolidRed;
	rw::RGBA SolidColor2 = SolidBlue;
	rw::RGBA SolidColor3 = SolidGreen;
	rw::RGBA SolidColor4 = SolidYellow;
	rw::RGBA SolidColor5 = SolidCyan;
	rw::RGBA SolidColor6 = SolidPurple;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
		SolidColor3 = SolidWhite;
		SolidColor4 = SolidWhite;
		SolidColor5 = SolidWhite;
		SolidColor6 = SolidWhite;
	}

	for(i = 0; i < 6; i++)
		TriList[i].setColor(SolidColor1.red, SolidColor1.green,
			SolidColor1.blue, SolidColor1.alpha);
	for(; i < 12; i++)
		TriList[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
	for(; i < 18; i++)
		TriList[i].setColor(SolidColor3.red, SolidColor3.green,
			SolidColor3.blue, SolidColor3.alpha);
	for(; i < 24; i++)
		TriList[i].setColor(SolidColor4.red, SolidColor4.green,
			SolidColor4.blue, SolidColor4.alpha);
	for(; i < 30; i++)
		TriList[i].setColor(SolidColor5.red, SolidColor5.green,
			SolidColor5.blue, SolidColor5.alpha);
	for(; i < 36; i++)
		TriList[i].setColor(SolidColor6.red, SolidColor6.green,
			SolidColor6.blue, SolidColor6.alpha);
}

void
TriListRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(TriList, 36, transform, transformFlags);
	rw::im3d::RenderPrimitive(rw::PRIMTYPETRILIST);
	rw::im3d::End();
}


void
IndexedTriListCreate(void)
{
	for(int i = 0; i < 8; i++){
		IndexedTriList[i].setX(IndexedTriListData[i][0]);
		IndexedTriList[i].setY(IndexedTriListData[i][1]);
		IndexedTriList[i].setZ(IndexedTriListData[i][2]);
		IndexedTriList[i].setU(IndexedTriListData[i][3]);
		IndexedTriList[i].setV(IndexedTriListData[i][4]);
	}
}

void
IndexedTriListSetColor(bool white)
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

	IndexedTriList[0].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
	IndexedTriList[1].setColor(SolidColor2.red, SolidColor2.green,
		SolidColor2.blue, SolidColor2.alpha);
	IndexedTriList[2].setColor(SolidColor3.red, SolidColor3.green,
		SolidColor3.blue, SolidColor3.alpha);
	IndexedTriList[3].setColor(SolidColor4.red, SolidColor4.green,
		SolidColor4.blue, SolidColor4.alpha);
	IndexedTriList[4].setColor(SolidColor5.red, SolidColor5.green,
		SolidColor5.blue, SolidColor5.alpha);
	IndexedTriList[5].setColor(SolidColor6.red, SolidColor6.green,
		SolidColor6.blue, SolidColor6.alpha);
	IndexedTriList[6].setColor(SolidColor7.red, SolidColor7.green,
		SolidColor7.blue, SolidColor7.alpha);
	IndexedTriList[7].setColor(SolidColor8.red, SolidColor8.green,
		SolidColor8.blue, SolidColor8.alpha);
}

void
IndexedTriListRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(IndexedTriList, 8, transform, transformFlags);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRILIST, IndexedTriListIndices, 36);
	rw::im3d::End();
}
