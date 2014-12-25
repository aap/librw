#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <iostream>
#include <fstream>
#include <list>

#include "rw.h"

using namespace std;

int
main(int argc, char *argv[])
{
//	Rw::Version = 0x31000;
//	Rw::Build = 0;

	registerNodeNamePlugin();
	registerBreakableModelPlugin();
	registerNativeDataPlugin();
	registerMeshPlugin();
	Rw::Clump *c;

	ifstream in(argv[1], ios::binary);
	Rw::FindChunk(in, Rw::ID_CLUMP, NULL, NULL);
	c = Rw::Clump::streamRead(in);
	assert(c != NULL);
	in.close();

	Rw::Image *tga = Rw::readTGA("b.tga");
	assert(tga != NULL);
	Rw::writeTGA(tga, "out.tga");

//	for(Rw::int32 i = 0; i < c->numAtomics; i++)
//		Rw::Gl::Instance(c->atomicList[i]);

	ofstream out(argv[2], ios::binary);
	c->streamWrite(out);
	out.close();

	delete c;

	return 0;

}
