#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>
#include <list>

#include "rwbase.h"
#include "rwplugin.h"
#include "rw.h"

using namespace std;

//
// X - Plugin of Clump
//
struct X
{
	Rw::int32 a;
	Rw::int32 b;
};

void*
ctorX(void *object, int offset, int)
{
	X *x = PLUGINOFFSET(X, object, offset);
	x->a = 123;
	x->b = 789;
	return object;
}

void*
dtorX(void *object, int, int)
{
	return object;
}

void*
cctorX(void *dst, void *src, int offset, int)
{
	X *srcx = PLUGINOFFSET(X, src, offset);
	X *dstx = PLUGINOFFSET(X, dst, offset);
	dstx->a = srcx->a;
	dstx->b = srcx->b;
	return dst;
}

Rw::int32
getSizeX(void *, int, int)
{
	return 8;
}

void
writeX(ostream &stream, Rw::int32, void *object, int offset, int)
{
	X *x = PLUGINOFFSET(X, object, offset);
	Rw::writeInt32(x->a, stream);
	Rw::writeInt32(x->b, stream);
}

void
readX(istream &stream, Rw::int32, void *object, int offset, int)
{
	X *x = PLUGINOFFSET(X, object, offset);
	x->a = Rw::readInt32(stream);
	x->b = Rw::readInt32(stream);
}

int
main(int argc, char *argv[])
{
//	Rw::Version = 0x31000;
//	Rw::Build = 0;

//	int Xoff = Rw::Clump::registerPlugin(8, 0x123, ctorX, dtorX, cctorX);
//	Xoff = Rw::Clump::registerPluginStream(0x123, readX, writeX, getSizeX);
	Rw::Clump *c;
//	X *x;

	ifstream in(argv[1], ios::binary);
	Rw::FindChunk(in, Rw::ID_CLUMP, NULL, NULL);
	c = Rw::Clump::streamRead(in);
	in.close();

	ofstream out("out.dff", ios::binary);
	c->streamWrite(out);
	out.close();

	delete c;

	return 0;

}
