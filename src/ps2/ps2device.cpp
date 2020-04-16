#ifdef RW_PS2

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "../rwbase.h"
#include "../rwerror.h"
#include "../rwplg.h"
#include "../rwpipeline.h"
#include "../rwobjects.h"
#include "../rwengine.h"
#include "../rwanim.h"
#include "../rwplugins.h"
#include "rwps2.h"
#include "rwps2plg.h"

#include "rwps2impl.h"

#define PLUGIN_ID 2

namespace rw {
namespace ps2 {

Device renderdevice = {
	16777215.0f, 0.0f,
	null::beginUpdate,
	null::endUpdate,
	null::clearCamera,
	null::showRaster,
	null::rasterRenderFast,
	null::setRenderState,
	null::getRenderState,
	null::im2DRenderLine,
	null::im2DRenderTriangle,
	null::im2DRenderPrimitive,
	null::im2DRenderIndexedPrimitive,
	null::im3DTransform,
	null::im3DRenderIndexed,
	null::im3DEnd,
	null::deviceSystem
};

}
}

#endif
