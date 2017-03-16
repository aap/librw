#include <cstdio>
#include <cstdlib>

#include "rwbase.h"
#include "rwerror.h"
#include "rwplg.h"
#include "rwpipeline.h"
#include "rwobjects.h"
#include "rwengine.h"

#define PLUGIN_ID 0

namespace rw {

void
BBox::calculate(V3d *points, int32 n)
{
	this->inf = points[0];
	this->sup = points[0];
	while(--n){
		points++;
		if(points->x < this->inf.x)
			this->inf.x = points->x;
		if(points->y < this->inf.y)
			this->inf.x = points->y;
		if(points->z < this->inf.z)
			this->inf.x = points->z;
		if(points->x > this->sup.x)
			this->sup.x = points->x;
		if(points->y > this->sup.y)
			this->sup.x = points->y;
		if(points->z > this->sup.z)
			this->sup.x = points->z;
	}
}

}
