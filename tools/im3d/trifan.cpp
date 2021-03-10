#include <rw.h>
#include <skeleton.h>

#include "im3d.h"

float TriFanData[34][5] = {
	{ 0.000f,  0.000f, -1.000f,  0.500f,  0.500f},

	{ 0.000f,  1.000f,  0.000f,  0.500f,  1.000f},
	{ 0.383f,  0.924f,  0.000f,  0.691f,  0.962f},
	{ 0.707f,  0.707f,  0.000f,  0.854f,  0.854f},
	{ 0.924f,  0.383f,  0.000f,  0.962f,  0.691f},
	{ 1.000f,  0.000f,  0.000f,  1.000f,  0.500f},
	{ 0.924f, -0.383f,  0.000f,  0.962f,  0.309f},
	{ 0.707f, -0.707f,  0.000f,  0.854f,  0.146f},
	{ 0.383f, -0.924f,  0.000f,  0.691f,  0.038f},
	{ 0.000f, -1.000f,  0.000f,  0.500f,  0.000f},
	{-0.383f, -0.924f,  0.000f,  0.309f,  0.038f},
	{-0.707f, -0.707f,  0.000f,  0.146f,  0.146f},
	{-0.924f, -0.383f,  0.000f,  0.038f,  0.309f},
	{-1.000f, -0.000f,  0.000f,  0.000f,  0.500f},
	{-0.924f,  0.383f,  0.000f,  0.038f,  0.691f},
	{-0.707f,  0.707f,  0.000f,  0.146f,  0.854f},
	{-0.383f,  0.924f,  0.000f,  0.309f,  0.962f},

	{ 0.000f,  1.000f,  0.000f,  0.500f,  1.000f},

	{-0.383f,  0.924f,  0.000f,  0.309f,  0.962f},
	{-0.707f,  0.707f,  0.000f,  0.146f,  0.854f},
	{-0.924f,  0.383f,  0.000f,  0.038f,  0.691f},
	{-1.000f, -0.000f,  0.000f,  0.000f,  0.500f},
	{-0.924f, -0.383f,  0.000f,  0.038f,  0.309f},
	{-0.707f, -0.707f,  0.000f,  0.146f,  0.146f},
	{-0.383f, -0.924f,  0.000f,  0.309f,  0.038f},
	{ 0.000f, -1.000f,  0.000f,  0.500f,  0.000f},
	{ 0.383f, -0.924f,  0.000f,  0.691f,  0.038f},
	{ 0.707f, -0.707f,  0.000f,  0.854f,  0.146f},
	{ 0.924f, -0.383f,  0.000f,  0.962f,  0.309f},
	{ 1.000f,  0.000f,  0.000f,  1.000f,  0.500f},
	{ 0.924f,  0.383f,  0.000f,  0.962f,  0.691f},
	{ 0.707f,  0.707f,  0.000f,  0.854f,  0.854f},
	{ 0.383f,  0.924f,  0.000f,  0.691f,  0.962f},
	{ 0.000f,  1.000f,  0.000f,  0.500f,  1.000f}
};

float IndexedTriFanData[17][5] = {
	/* top */
	{ 0.000f,  0.000f, -1.000f,  0.500f,  0.500f},
	/* circle */
	{ 0.000f,  1.000f,  0.000f,  0.500f,  1.000f},
	{ 0.383f,  0.924f,  0.000f,  0.691f,  0.962f},
	{ 0.707f,  0.707f,  0.000f,  0.854f,  0.854f},
	{ 0.924f,  0.383f,  0.000f,  0.962f,  0.691f},
	{ 1.000f,  0.000f,  0.000f,  1.000f,  0.500f},
	{ 0.924f, -0.383f,  0.000f,  0.962f,  0.309f},
	{ 0.707f, -0.707f,  0.000f,  0.854f,  0.146f},
	{ 0.383f, -0.924f,  0.000f,  0.691f,  0.038f},
	{ 0.000f, -1.000f,  0.000f,  0.500f,  0.000f},
	{-0.383f, -0.924f,  0.000f,  0.309f,  0.038f},
	{-0.707f, -0.707f,  0.000f,  0.146f,  0.146f},
	{-0.924f, -0.383f,  0.000f,  0.038f,  0.309f},
	{-1.000f, -0.000f,  0.000f,  0.000f,  0.500f},
	{-0.924f,  0.383f,  0.000f,  0.038f,  0.691f},
	{-0.707f,  0.707f,  0.000f,  0.146f,  0.854f},
	{-0.383f,  0.924f,  0.000f,  0.309f,  0.962f}
};

rw::uint16 IndexedTriFanIndices[34] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1,
	16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
};

rw::RWDEVICE::Im3DVertex TriFan[34];
rw::RWDEVICE::Im3DVertex IndexedTriFan[17];


void
TriFanCreate(void)
{
	for(int i = 0; i < 34; i++){
		TriFan[i].setX(TriFanData[i][0]);
		TriFan[i].setY(TriFanData[i][1]);
		TriFan[i].setZ(TriFanData[i][2]);
		TriFan[i].setU(TriFanData[i][3]);
		TriFan[i].setV(TriFanData[i][4]);
	}
}

void
TriFanSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidYellow;
	rw::RGBA SolidColor2 = SolidBlue;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
	}

	TriFan[0].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
	for(int i = 1; i < 34; i++)
		TriFan[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
}

void
TriFanRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(TriFan, 34, transform, transformFlags);
	rw::im3d::RenderPrimitive(rw::PRIMTYPETRIFAN);
	rw::im3d::End();
}


void
IndexedTriFanCreate(void)
{
	for(int i = 0; i < 17; i++){
		IndexedTriFan[i].setX(IndexedTriFanData[i][0]);
		IndexedTriFan[i].setY(IndexedTriFanData[i][1]);
		IndexedTriFan[i].setZ(IndexedTriFanData[i][2]);
		IndexedTriFan[i].setU(IndexedTriFanData[i][3]);
		IndexedTriFan[i].setV(IndexedTriFanData[i][4]);
	}
}

void
IndexedTriFanSetColor(bool white)
{
	rw::RGBA SolidColor1 = SolidGreen;
	rw::RGBA SolidColor2 = SolidBlack;

	if(white){
		SolidColor1 = SolidWhite;
		SolidColor2 = SolidWhite;
	}

	IndexedTriFan[0].setColor(SolidColor1.red, SolidColor1.green,
		SolidColor1.blue, SolidColor1.alpha);
	for(int i = 1; i < 17; i++)
		IndexedTriFan[i].setColor(SolidColor2.red, SolidColor2.green,
			SolidColor2.blue, SolidColor2.alpha);
}

void
IndexedTriFanRender(rw::Matrix *transform, rw::uint32 transformFlags)
{
	rw::im3d::Transform(IndexedTriFan, 17, transform, transformFlags);
	rw::im3d::RenderIndexedPrimitive(rw::PRIMTYPETRIFAN, IndexedTriFanIndices, 34);
	rw::im3d::End();
}
