#include <rw.h>
#include <skeleton.h>

using namespace rw;

//
// This is a test to implement T&L in software and render with Im2D

#define MAX_LIGHTS 8

struct Directional {
	V3d at;
	RGBAf color;
};
static Directional directionals[MAX_LIGHTS];
static int32 numDirectionals;
static RGBAf ambLight;

static void
enumLights(Matrix *lightmat)
{
	int32 n;
	World *world;

	world = (World*)engine->currentWorld;
	ambLight.red = 0.0;
	ambLight.green = 0.0;
	ambLight.blue = 0.0;
	ambLight.alpha = 0.0;
	numDirectionals = 0;
	// only unpositioned lights right now
	FORLIST(lnk, world->directionalLights){
		Light *l = Light::fromWorld(lnk);
		if(l->getType() == Light::DIRECTIONAL){
			if(numDirectionals >= MAX_LIGHTS)
				continue;
			n = numDirectionals++;
			V3d::transformVectors(&directionals[n].at, &l->getFrame()->getLTM()->at, 1, lightmat);
			directionals[n].color = l->color;
			directionals[n].color.alpha = 0.0f;
		}else if(l->getType() == Light::AMBIENT){
			ambLight.red   += l->color.red;
			ambLight.green += l->color.green;
			ambLight.blue  += l->color.blue;
		}
	}
}

static void
drawAtomic(Atomic *a)
{
	using namespace RWDEVICE;
	Im2DVertex *im2dverts;
	V3d *xvert;
	Matrix xform;
	Matrix lightmat;
	Camera *cam = (Camera*)engine->currentCamera;
	Geometry *g = a->geometry;
	MeshHeader *mh = g->meshHeader;
	Mesh *m = mh->getMeshes();
	int32 width = cam->frameBuffer->width;
	int32 height = cam->frameBuffer->height;
	RGBA *prelight;
	V3d *normals;
	TexCoords *texcoords;

	Matrix::mult(&xform, a->getFrame()->getLTM(), &cam->viewMatrix);
	Matrix::invert(&lightmat, a->getFrame()->getLTM());

	enumLights(&lightmat);

	xvert = rwNewT(V3d, g->numVertices, MEMDUR_FUNCTION);
	im2dverts = rwNewT(Im2DVertex, g->numVertices, MEMDUR_FUNCTION);

	prelight = g->colors;
	normals = g->morphTargets[0].normals;
	texcoords = g->texCoords[0];

	V3d::transformPoints(xvert, g->morphTargets[0].vertices, g->numVertices, &xform);
	for(int32 i = 0; i < g->numVertices; i++){
		float32 recipZ = 1.0f/xvert[i].z;

		im2dverts[i].setScreenX(xvert[i].x * recipZ * width);
		im2dverts[i].setScreenY((xvert[i].y * recipZ * height));
		im2dverts[i].setScreenZ(recipZ * cam->zScale + cam->zShift);
		im2dverts[i].setCameraZ(xvert[i].z);
		im2dverts[i].setRecipCameraZ(recipZ);
		im2dverts[i].setColor(255, 0, 0, 255);
		im2dverts[i].setU(texcoords[i].u);
		im2dverts[i].setV(texcoords[i].v);
	}
	for(int32 i = 0; i < mh->numMeshes; i++){
		for(int32 j = 0; j < m[i].numIndices; j++){
			int32 idx = m[i].indices[j];
			RGBA col;
			RGBAf colf, color;
			if(prelight)
				convColor(&color, &prelight[idx]);
			else{
				color.red = color.green = color.blue = 0.0f;
				color.alpha = 1.0f;
			}
			color = add(color, ambLight);
			if(normals)
				for(int32 k = 0; k < numDirectionals; k++){
					float32 f = dot(normals[idx], neg(directionals[k].at));
					if(f <= 0.0f) continue;
					colf = scale(directionals[k].color, f);
					color = add(color, colf);
				}
			convColor(&colf, &m[i].material->color);
			color = modulate(color, colf);
			clamp(&color);
			convColor(&col, &color);
			im2dverts[idx].setColor(col.red, col.green, col.blue, col.alpha);
		}

		engine->imtexture = m[i].material->texture;
		rw::engine->device.im2DRenderIndexedPrimitive(rw::PRIMTYPETRILIST,
			im2dverts, g->numVertices, m[i].indices, m[i].numIndices);
	}

	rwFree(xvert);
	rwFree(im2dverts);
}

void
tlTest(Clump *clump)
{
	FORLIST(lnk, clump->atomics){
		Atomic *a = Atomic::fromClump(lnk);
		drawAtomic(a);
	}
}
